#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

#include <SDL_gamecontroller.h>
#include <SDL_joystick.h>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadachr.hpp>
#include <components/esm4/loadacti.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadwatr.hpp>
#include <components/esm4/loadwrld.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/falloutscript/worldcommands.hpp>
#include <components/falloutscript/worldmath.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwrender/camera.hpp"
#include "../mwworld/cell.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/datetimemanager.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/refdata.hpp"
#include "../mwworld/weather.hpp"

namespace OFScript
{
    namespace
    {
        constexpr double eyeHeight = 110.0;

        /// The number a script knows a form by, 0 for the null form and for one that scripts can not name
        FalloutScript::FormId numberOf(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }

        /// The plugins that the games call a DLC: what GetDLCCount counts is the entries of a list of the executable
        /// that is filled while the data files are read (FalloutNV.exe 0x5DF930 counts them), which is the add-on
        /// plugins that are loaded
        bool isAddOn(const std::string& file)
        {
            static constexpr std::string_view names[] = {
                // New Vegas
                "deadmoney.esm",
                "honesthearts.esm",
                "oldworldblues.esm",
                "lonesomeroad.esm",
                "gunrunnersarsenal.esm",
                "classicpack.esm",
                "mercenarypack.esm",
                "tribalpack.esm",
                "caravanpack.esm",
                // Fallout 3
                "anchorage.esm",
                "thepitt.esm",
                "brokensteel.esm",
                "pointlookout.esm",
                "zeta.esm",
            };
            const std::string lower = Misc::StringUtils::lowerCase(file);
            return std::find(std::begin(names), std::end(names), lower) != std::end(names);
        }
    }

    /// What the commands about the world ask of the game (components/falloutscript/worldcommands.hpp), answered from
    /// the scene, the records and the interface of the host
    class WorldHost final : public FalloutScript::WorldWorld
    {
    public:
        explicit WorldHost(FalloutScripts& host)
            : mHost(host)
        {
        }

        bool sightFacts(FalloutScript::FormId viewer, FalloutScript::FormId target,
            FalloutScript::World::SightFacts& facts) override
        {
            // The scene is for the thread of the frame
            if (!onMainThread())
                return false;
            const OFWorld::Ptr eyes = mHost.findReference(viewer);
            const OFWorld::Ptr seen = mHost.findReference(target);
            if (eyes.isEmpty() || seen.isEmpty() || !eyes.isInCell() || !seen.isInCell())
                return false;
            OFBase::World* world = OFBase::Environment::get().getWorld();
            facts.mViewerIsPlayer = viewer == mHost.mPlayer;
            facts.mTargetIsPlayer = target == mHost.mPlayer;
            facts.mTargetIsViewerTarget = mHost.mCombat.target(viewer) == target;
            facts.mTargetLoaded = seen.getRefData().isEnabled() && seen.getRefData().getBaseNode() != nullptr;
            facts.mTargetDead = mHost.mActors.isDead(target);
            facts.mSameSpace = eyes.getCell() == seen.getCell()
                || (eyes.getCell()->isExterior() && seen.getCell()->isExterior());
            const osg::Vec3f from = eyes.getRefData().getPosition().asVec3();
            const osg::Vec3f to = seen.getRefData().getPosition().asVec3();
            facts.mDistance = (to - from).length();
            facts.mEye = { from.x(), from.y(), from.z() + eyeHeight };
            facts.mFacing = eyes.getRefData().getPosition().rot[2];
            facts.mTarget = { to.x(), to.y(), to.z() };
            const osg::Vec3f extents = world->getHalfExtents(seen);
            facts.mTargetHeight = 2.0 * extents.z();
            mRayTolerance = std::max(extents.x(), extents.y());
            facts.mViewCone = mHost.gameSetting("fDetectionViewCone", 190);
            facts.mAllVisible = mHost.mWorldState.allVisible();
            if (facts.mViewerIsPlayer)
            {
                // The camera's place is where the player's rays start, and the target has to be in its view
                if (OFRender::Camera* camera = world->getCamera())
                {
                    const osg::Vec3d place = camera->getPosition();
                    facts.mEye = { place.x(), place.y(), place.z() };
                    const osg::Matrixf transform = camera->getViewMatrix() * camera->getProjectionMatrix();
                    for (const double share : { 0.0, 0.5, 1.0 })
                    {
                        const osg::Vec4f clip = osg::Vec4f(static_cast<float>(facts.mTarget[0]),
                                                    static_cast<float>(facts.mTarget[1]),
                                                    static_cast<float>(facts.mTarget[2] + share * facts.mTargetHeight),
                                                    1.f)
                            * transform;
                        if (clip.w() > 0.f && std::abs(clip.x()) <= clip.w() && std::abs(clip.y()) <= clip.w()
                            && std::abs(clip.z()) <= clip.w())
                        {
                            facts.mInFrustum = true;
                            break;
                        }
                    }
                }
            }
            return true;
        }

        bool rayReaches(const FalloutScript::World::Vec3& from, const FalloutScript::World::Vec3& to) override
        {
            if (!onMainThread())
                return false;
            const osg::Vec3f start(static_cast<float>(from[0]), static_cast<float>(from[1]), static_cast<float>(from[2]));
            const osg::Vec3f goal(static_cast<float>(to[0]), static_cast<float>(to[1]), static_cast<float>(to[2]));
            const float apart = (goal - start).length();
            if (apart < 1.f)
                return true;
            // The ray ends inside the target's own bound, so what it hits there is the target and not a wall
            const float reach = OFBase::Environment::get().getWorld()->getDistToNearestRayHit(start, goal - start, apart);
            return reach >= apart - 1.f - static_cast<float>(mRayTolerance);
        }

        std::optional<std::string> cellNameOfReference(FalloutScript::FormId reference) override
        {
            FalloutScripts::Place place;
            if (!mHost.whereIs(reference, place))
                return std::nullopt;
            const ESM::FormId* id = place.mCell.getIf<ESM::FormId>();
            if (id == nullptr)
                return std::nullopt;
            return cellNameOfRecord(*id);
        }

        std::optional<std::string> cellNameOfCell(FalloutScript::FormId cell) override
        {
            return cellNameOfRecord(ESM::FormId::fromUint32(cell));
        }

        std::optional<std::uint32_t> baseRecordType(FalloutScript::FormId reference) override
        {
            if (!onMainThread())
                return std::nullopt;
            const OFWorld::Ptr ptr = mHost.findReference(reference);
            if (ptr.isEmpty())
                return std::nullopt;
            return static_cast<std::uint32_t>(ptr.getType());
        }

        FalloutScript::FormId baseFormOf(FalloutScript::FormId reference) override
        {
            ESM::FormId base;
            return mHost.baseOf(reference, base) ? numberOf(base) : 0;
        }

        FalloutScript::FormId enableParentOf(FalloutScript::FormId reference) override
        {
            const ESM::FormId id = ESM::FormId::fromUint32(reference);
            if (id.isZeroOrUnset())
                return 0;
            if (const ESM4::Reference* record = mHost.mStore.get<ESM4::Reference>().search(id))
                return numberOf(record->mEsp.parent);
            if (const ESM4::ActorCharacter* record = mHost.mStore.get<ESM4::ActorCharacter>().search(id))
                return numberOf(record->mEsp.parent);
            if (const ESM4::ActorCreature* record = mHost.mStore.get<ESM4::ActorCreature>().search(id))
                return numberOf(record->mEsp.parent);
            return 0;
        }

        std::optional<FalloutScript::World::Pose> recordedPose(FalloutScript::FormId reference) override
        {
            const ESM::FormId id = ESM::FormId::fromUint32(reference);
            if (id.isZeroOrUnset())
                return std::nullopt;
            if (const ESM4::Reference* record = mHost.mStore.get<ESM4::Reference>().search(id))
                return poseOf(record->mPos);
            if (const ESM4::ActorCharacter* record = mHost.mStore.get<ESM4::ActorCharacter>().search(id))
                return poseOf(record->mPos);
            if (const ESM4::ActorCreature* record = mHost.mStore.get<ESM4::ActorCreature>().search(id))
                return poseOf(record->mPos);
            return std::nullopt;
        }

        std::optional<FalloutScript::World::Pose> currentPose(FalloutScript::FormId reference) override
        {
            if (!onMainThread())
                return std::nullopt;
            const OFWorld::Ptr ptr = mHost.findReference(reference);
            if (ptr.isEmpty())
                return std::nullopt;
            return poseOf(ptr.getRefData().getPosition());
        }

        /// IsInDangerousWater (eval 0x5A36B0): an actor that is in water (the flag at +0x14C of the actor) whose water type
        /// (the one of its cell, else that of its worldspace) has the first bit of its flags, "causes damage" (0x580060,
        /// the byte at +0x71 of the water record, FNAM). The place of the actor stands for the flag: its feet are below
        /// the water level of its cell. The water type the executable falls back on (0x11CA53C) is not looked for.
        bool inDangerousWater(FalloutScript::FormId actor) override
        {
            if (!onMainThread())
                return false;
            const OFWorld::Ptr ptr = mHost.findReference(actor);
            if (ptr.isEmpty() || !ptr.isInCell())
                return false;
            OFBase::World* world = OFBase::Environment::get().getWorld();
            if (!world->isUnderwater(ptr.getCell(), ptr.getRefData().getPosition().asVec3()))
                return false;
            FalloutScripts::Place place;
            if (!mHost.whereIs(actor, place))
                return false;
            const ESM::FormId* cellId = place.mCell.getIf<ESM::FormId>();
            const ESM4::Cell* cell = cellId != nullptr ? mHost.mStore.get<ESM4::Cell>().search(*cellId) : nullptr;
            if (cell == nullptr)
                return false;
            ESM::FormId water = cell->mWater;
            if (water.isZeroOrUnset())
            {
                const ESM::FormId* worldId = cell->mParent.getIf<ESM::FormId>();
                const ESM4::World* worldspace = worldId != nullptr ? mHost.mStore.get<ESM4::World>().search(*worldId) : nullptr;
                if (worldspace != nullptr)
                    water = worldspace->mWater;
            }
            const ESM4::Water* record = water.isZeroOrUnset() ? nullptr : mHost.mStore.get<ESM4::Water>().search(water);
            return record != nullptr && (record->mWaterFlags & 0x01) != 0;
        }

        int persistentReferenceCount() override
        {
            const std::lock_guard lock(mMutex);
            if (mPersistent < 0)
            {
                int count = 0;
                const auto tally = [&count](const auto& ref) {
                    if ((ref.mFlags & ESM4::Rec_Persistent) != 0)
                        ++count;
                };
                mHost.mStore.get<ESM4::Reference>().forEachPlaced(tally);
                mHost.mStore.get<ESM4::ActorCharacter>().forEachPlaced(tally);
                mHost.mStore.get<ESM4::ActorCreature>().forEachPlaced(tally);
                mPersistent = count;
            }
            return mPersistent;
        }

        bool isActor(FalloutScript::FormId reference) override { return mHost.isActor(reference); }
        bool isWaterActivator(FalloutScript::FormId reference) override
        {
            ESM::FormId base;
            if (!mHost.baseOf(reference, base) || base.isZeroOrUnset())
                return false;
            const ESM4::Activator* record = mHost.mStore.get<ESM4::Activator>().search(base);
            return record != nullptr && !record->mWaterType.isZeroOrUnset();
        }
        bool isDeadActor(FalloutScript::FormId reference) override { return mHost.mActors.isDead(reference); }

        bool recordedDefaultOpen(FalloutScript::FormId reference) override
        {
            const ESM::FormId id = ESM::FormId::fromUint32(reference);
            const ESM4::Reference* record = id.isZeroOrUnset() ? nullptr : mHost.mStore.get<ESM4::Reference>().search(id);
            return record != nullptr && record->mOpenByDefault;
        }

        void setDoorOpen(FalloutScript::FormId reference, bool open) override
        {
            mHost.runOnMainThread([this, reference, open] { mHost.mOpenStates.request(reference, open, mHost); });
        }

        void setActorSize(FalloutScript::FormId /*actor*/, double /*size*/) override
        {
            // The size is that of the capsule a character collides with, not of its picture: this engine's characters
            // collide with the box their model gives them, which nothing resizes
            note("SetSize", "the collision size of a character is not changed");
        }

        double secondsPassed() override { return mHost.mObjects.secondsPassed(); }

        FalloutScript::World::SkyView sky() override
        {
            using FalloutScript::World::SkyView;
            const auto weatherOf = [](const OFWorld::Weather& weather) {
                SkyView::Weather result;
                result.mPresent = true;
                result.mForm = 0;
                if (const ESM::FormId* id = weather.mId.getIf<ESM::FormId>())
                    result.mForm = id->toUint32();
                result.mClassification = weather.mClassification;
                result.mPrecipitationBeginFadeIn = weather.mPrecipitationBeginFadeIn;
                result.mPrecipitationEndFadeOut = weather.mPrecipitationEndFadeOut;
                result.mWindSpeed = static_cast<std::uint8_t>(
                    std::clamp(std::lround(static_cast<double>(weather.mWindSpeed) * 255.0), 0L, 255L));
                return result;
            };
            SkyView result;
            OFBase::World* world = OFBase::Environment::get().getWorld();
            if (world == nullptr || !onMainThread())
                return result;
            try
            {
                // The weather of the manager is the one that is left while it changes to the next, and the factor is the
                // share of it that is left; the percent of the games is the share of the change that is done
                const OFWorld::Weather& current = world->getCurrentWeather();
                if (const OFWorld::Weather* next = world->getNextWeather())
                {
                    result.mOutgoing = weatherOf(current);
                    result.mCurrent = weatherOf(*next);
                    result.mPercent = 1.0 - std::clamp(static_cast<double>(world->getWeatherTransition()), 0.0, 1.0);
                }
                else
                    result.mCurrent = weatherOf(current);
            }
            catch (const std::exception&)
            {
                return SkyView();
            }
            return result;
        }

        void setWeather(FalloutScript::FormId weather, bool hold) override { mHost.forceWeather(weather, hold); }

        bool timePasses() override
        {
            const std::lock_guard lock(mHost.mInteractionMutex);
            return mHost.mRest.mActive;
        }

        void setTimeMultiplier(double multiplier) override
        {
            const float scale = static_cast<float>(std::max(multiplier, 0.0));
            mHost.runOnMainThread([scale] {
                if (OFBase::World* world = OFBase::Environment::get().getWorld())
                    world->getTimeManager()->setSimulationTimeScale(scale);
            });
        }

        void setPackageDuration(FalloutScript::FormId actor, double hundredths) override
        {
            mHost.runOnMainThread([this, actor, hundredths] {
                const OFWorld::Ptr ptr = mHost.findReference(actor);
                if (!ptr.isEmpty())
                    OFBase::Environment::get().getMechanicsManager()->setFalloutPackageDuration(ptr, hundredths);
            });
        }

        bool padDisabled() override
        {
            // bDisable360Controller of the Interface section turns the pad of the box off
            const std::optional<int> setting = mHost.gameIniInteger("Interface", "bDisable360Controller");
            return setting.has_value() && *setting != 0;
        }

        bool goreDisabled() override
        {
            return FalloutScript::World::goreDisabledBy(mHost.gameIniInteger("General", "bDisableAllGore"));
        }

        bool padConnected() override
        {
            for (int index = 0; index < SDL_NumJoysticks(); ++index)
                if (SDL_IsGameController(index) == SDL_TRUE)
                    return true;
            return false;
        }

        int dlcCount() override
        {
            return static_cast<int>(std::count_if(mHost.mContentFiles.begin(), mHost.mContentFiles.end(), isAddOn));
        }

        std::uint8_t playerControlsDisabled() override
        {
            const unsigned off = mHost.mControlsOff.load();
            std::uint8_t bits = 0;
            for (std::size_t i = 0; i < FalloutScript::World::controlBits.size(); ++i)
                if ((off & (1u << i)) != 0)
                    bits |= FalloutScript::World::controlBits[i];
            return bits;
        }

        bool playerFirstPerson() override
        {
            if (!onMainThread())
                return true;
            const OFBase::World* world = OFBase::Environment::get().getWorld();
            return world == nullptr || world->isFirstPerson();
        }

        void exitGame() override
        {
            // The credits are asked for by the global PlayCredits, 2 (the other credits) for a player that has the perk
            // that the executable names in New Vegas, and 1 in Fallout 3
            const bool otherCredits = mHost.isNewVegas() && mHost.mProgress.perkRank(mHost.mPlayer, 0xED568) > 0;
            mHost.requestCredits(otherCredits ? 2 : 1);
            if (const FalloutScript::FormId global = mHost.findGlobal("PlayCredits"); global != 0)
                mHost.setGlobal(global, otherCredits ? 2.0 : 1.0);
            Log(Debug::Info) << "ExitGame: the game goes back to the main menu with PlayCredits "
                             << (otherCredits ? 2 : 1);
            mHost.runOnMainThread([] { OFBase::Environment::get().getStateManager()->endGame(); });
        }

        double randomUnit() override
        {
            const std::lock_guard lock(mMutex);
            return std::uniform_real_distribution<double>(0.0, 1.0)(mRandom);
        }

        void forceTerminalBack() override { mHost.terminalBack(); }

        void openTerminal(FalloutScript::FormId terminal) override
        {
            mHost.runOnMainThread([this, terminal] {
                const OFWorld::Ptr object = mHost.findReference(terminal);
                const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
                if (!object.isEmpty() && !player.isEmpty())
                    mHost.openTerminal(object, player);
            });
        }

        void showTutorial(FalloutScript::FormId message) override { mHost.showMessage(message, {}, 0); }

        void showSleepWaitMenu(bool sleeping) override
        {
            mHost.runOnMainThread([this, sleeping] { mHost.requestRest(sleeping, 0); });
        }

        void showAllMapMarkers(bool show, bool withTravel, bool keepHidden) override
        {
            for (const FalloutScripts::PlacedMarker& marker : mHost.placedMarkers())
                mHost.mMapMarkers.showOrHide(marker.mId, show, withTravel, keepHidden);
        }

        void playMovie(std::string_view file, bool allowSkip) override
        {
            // The player of a video holds the frame until it is over, which a window that is not shown never gets to
            // the end of
            mHost.runOnMainThread([name = std::string(file), allowSkip] {
                OFBase::WindowManager* windows = OFBase::Environment::get().getWindowManager();
                if (windows == nullptr || !windows->isWindowVisible())
                {
                    Log(Debug::Info) << "PlayBink " << name << ": there is no window to play it in";
                    return;
                }
                windows->playVideo(name, allowSkip);
            });
        }

        void showTexture(FalloutScript::FormId reference, std::string_view shape, std::string_view texture) override
        {
            mHost.swapTexture(reference, shape, texture);
        }

        void addCardToPlayer(FalloutScript::FormId card) override { mHost.givePlayerItem(card, 1); }

        void setGodMode(bool on) override
        {
            mHost.runOnMainThread([on] {
                if (OFBase::World* world = OFBase::Environment::get().getWorld())
                    if (world->getGodModeState() != on)
                        world->toggleGodMode();
            });
        }

        bool godMode() override
        {
            const OFBase::World* world = OFBase::Environment::get().getWorld();
            return world != nullptr && world->getGodModeState();
        }

        void note(std::string_view command, std::string_view what) override
        {
            // Once for each command
            if (mNoted.insert(std::string(command)).second)
                Log(Debug::Info) << command << " is accepted and does nothing: " << what;
        }

    private:
        bool onMainThread() const { return std::this_thread::get_id() == mHost.mMainThread; }

        std::optional<std::string> cellNameOfRecord(ESM::FormId id)
        {
            if (id.isZeroOrUnset())
                return std::nullopt;
            const ESM4::Cell* record = mHost.mStore.get<ESM4::Cell>().search(id);
            if (record == nullptr)
                return std::nullopt;
            return record->mFullName;
        }

        static FalloutScript::World::Pose poseOf(const ESM::Position& position)
        {
            FalloutScript::World::Pose pose;
            for (std::size_t i = 0; i < 3; ++i)
            {
                pose.mPosition[i] = position.pos[i];
                pose.mAngle[i] = position.rot[i];
            }
            return pose;
        }

        FalloutScripts& mHost;
        std::mutex mMutex;
        int mPersistent = -1;
        std::mt19937 mRandom{ std::random_device{}() };
        double mRayTolerance = 0;
        std::set<std::string> mNoted;
    };

    void FalloutScripts::installWorldRules()
    {
        mWorldHost = std::make_shared<WorldHost>(*this);
        FalloutScript::WorldMath::addMathCommands(mInterpreter);
        FalloutScript::addWorldCommands(mInterpreter, mWorldState, *mWorldHost, *this);
    }

    void FalloutScripts::updateWorldRules(float frametime) { mWorldState.update(frametime); }
}

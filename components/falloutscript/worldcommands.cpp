#include "worldcommands.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;

        std::string keyOf(const char* table, FormId id)
        {
            return std::string("World.") + table + "." + hex(id);
        }

        std::int32_t asSwitch(FormId id)
        {
            return static_cast<std::int32_t>(id);
        }

        /// The axis a script names with a letter (X, Y or Z in either case) or a number; -1 for anything else
        int axisOf(const CallContext& call, std::size_t index)
        {
            switch (intArgument(call, index))
            {
                case 'X':
                case 'x':
                case 0:
                    return 0;
                case 'Y':
                case 'y':
                case 1:
                    return 1;
                case 'Z':
                case 'z':
                case 2:
                    return 2;
                default:
                    return -1;
            }
        }

        /// The text a call was given for an argument (the letters of an axis or a file are text, not numbers, in a call
        /// that the interpreter makes from a condition)
        std::string_view textOf(const CallContext& call, std::size_t index)
        {
            return index < call.mArguments.size() && call.mArguments[index].mIsText
                ? std::string_view(call.mArguments[index].mText)
                : std::string_view();
        }

        /// A form id written in hexadecimal, 0 for text that is none (EnableNavMesh: sscanf "%x" of the executable)
        FormId parseHex(std::string_view text)
        {
            const std::string copy(text);
            char* end = nullptr;
            const unsigned long value = std::strtoul(copy.c_str(), &end, 16);
            return end == copy.c_str() ? 0 : static_cast<FormId>(value);
        }

        double wrapAngle(double radians)
        {
            const double turned = std::fmod(radians, 2 * pi);
            return turned < 0 ? turned + 2 * pi : turned;
        }
    }

    WorldState::WorldState(Switches& switches)
        : mSwitches(switches)
    {
    }

    void WorldState::clear()
    {
        mAllVisible = false;
        mDialogSubtitles = false;
        mLoadingMenu = false;
        mStarts.clear();
        mActions.clear();
        mSplatter.reset();
        const std::lock_guard lock(mMutex);
        mShudder = World::HudShudder();
        mExpressions.clear();
    }

    void WorldState::setNavMesh(FormId navMesh, bool enabled)
    {
        mSwitches.set(keyOf("NavMesh", navMesh), enabled ? 1 : -1);
    }

    int WorldState::navMesh(FormId navMesh) const
    {
        return mSwitches.get(keyOf("NavMesh", navMesh));
    }

    void WorldState::setDefaultOpen(FormId reference, bool open)
    {
        mSwitches.set(keyOf("DefaultOpen", reference), open ? 1 : -1);
    }

    int WorldState::defaultOpen(FormId reference) const
    {
        return mSwitches.get(keyOf("DefaultOpen", reference));
    }

    void WorldState::setEaten(FormId corpse, bool eaten)
    {
        mSwitches.set(keyOf("Eaten", corpse), eaten ? 1 : 0);
    }

    bool WorldState::eaten(FormId corpse) const
    {
        return mSwitches.get(keyOf("Eaten", corpse)) != 0;
    }

    void WorldState::setMinimalUse(FormId doorBase, bool minimal)
    {
        mSwitches.set(keyOf("MinimalUse", doorBase), minimal ? 1 : 0);
    }

    bool WorldState::minimalUse(FormId doorBase) const
    {
        return mSwitches.get(keyOf("MinimalUse", doorBase)) != 0;
    }

    void WorldState::setLastExteriorDoor(FormId door)
    {
        mSwitches.set("World.LastExteriorDoor", asSwitch(door));
    }

    FormId WorldState::lastExteriorDoor() const
    {
        return static_cast<FormId>(mSwitches.get("World.LastExteriorDoor"));
    }

    void WorldState::setAlwaysSubtitles(FormId actor, bool always)
    {
        mSwitches.set(keyOf("Subtitles", actor), always ? 1 : 0);
    }

    bool WorldState::alwaysSubtitles(FormId actor) const
    {
        return mSwitches.get(keyOf("Subtitles", actor)) != 0;
    }

    void WorldState::setDemigod(bool on)
    {
        mSwitches.set("World.Demigod", on ? 1 : 0);
    }

    bool WorldState::demigod() const
    {
        return mSwitches.get("World.Demigod") != 0;
    }

    bool WorldState::redeemCode(std::string_view code)
    {
        const std::string key = World::codeKey(code);
        if (key.empty())
            return false;
        mSwitches.set("World.Code." + key, 1);
        return true;
    }

    bool WorldState::revokeCode(std::string_view code)
    {
        const std::string key = World::codeKey(code);
        if (key.empty())
            return false;
        mSwitches.set("World.Code." + key, 0);
        return true;
    }

    bool WorldState::codeRedeemed(std::string_view code) const
    {
        const std::string key = World::codeKey(code);
        return !key.empty() && mSwitches.get("World.Code." + key) != 0;
    }

    void WorldState::remapWater(FormId from, FormId to)
    {
        // The executable (FalloutNV.exe 0x580680) takes the remap away when the new type is the old one
        mSwitches.set(keyOf("WaterRemap", from), from == to ? 0 : asSwitch(to));
    }

    FormId WorldState::waterRemappedTo(FormId from) const
    {
        return static_cast<FormId>(mSwitches.get(keyOf("WaterRemap", from)));
    }

    void WorldState::setLodApocalypse(bool on)
    {
        mSwitches.set("World.LodApocalypse", on ? 1 : 0);
    }

    bool WorldState::lodApocalypse() const
    {
        return mSwitches.get("World.LodApocalypse") != 0;
    }

    void WorldState::setImageSpace(FormId imageSpace)
    {
        mSwitches.set("World.ImageSpace", asSwitch(imageSpace));
    }

    FormId WorldState::imageSpace() const
    {
        return static_cast<FormId>(mSwitches.get("World.ImageSpace"));
    }

    void WorldState::startShudder(double durationMs, double amplitude, double frequency)
    {
        const std::lock_guard lock(mMutex);
        mShudder.start(durationMs, amplitude, frequency);
    }

    void WorldState::updateShudder(double milliseconds)
    {
        const std::lock_guard lock(mMutex);
        mShudder.update(milliseconds);
    }

    std::array<double, 2> WorldState::shudderOffset()
    {
        const std::lock_guard lock(mMutex);
        return mShudder.offset();
    }

    void WorldState::setExpression(FormId reference, std::string_view first, std::string_view second)
    {
        const std::lock_guard lock(mMutex);
        mExpressions[reference] = { std::string(first), std::string(second) };
    }

    std::optional<std::pair<std::string, std::string>> WorldState::expression(FormId reference) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mExpressions.find(reference);
        if (found == mExpressions.end())
            return std::nullopt;
        return found->second;
    }

    void WorldState::update(double seconds)
    {
        mSplatter.update(seconds);
        mActions.update(seconds);
        updateShudder(seconds * 1000.0);
    }

    void addWorldCommands(Interpreter& interpreter, WorldState& state, WorldWorld& world, PlacementWorld& placement)
    {
        // ---- Seeing and cells ----------------------------------------------------------------------------------------
        interpreter.setHandler("GetLineOfSight", [&state, &world](CallContext& call) {
            const FormId target = formArgument(call, 0);
            if (call.mReference == 0 || target == 0)
                return 0.0;
            World::SightFacts facts;
            if (!world.sightFacts(call.mReference, target, facts))
                return 0.0;
            facts.mAllVisible = state.allVisible();
            return World::seesTarget(facts,
                       [&world](const World::Vec3& from, const World::Vec3& to) { return world.rayReaches(from, to); })
                ? 1.0
                : 0.0;
        });
        // A prefix of names, not an identity (see World::cellNameMatches). The reference is the second argument, or the
        // one the command is called on.
        interpreter.setHandler("GetInCellParam", [&world](CallContext& call) {
            const FormId cell = formArgument(call, 0);
            FormId reference = formArgument(call, 1);
            if (reference == 0)
                reference = call.mReference;
            if (cell == 0 || reference == 0)
                return 0.0;
            const std::optional<std::string> name = world.cellNameOfReference(reference);
            const std::optional<std::string> asked = world.cellNameOfCell(cell);
            return name && asked && World::cellNameMatches(*name, *asked) ? 1.0 : 0.0;
        });
        interpreter.setHandler("IsInDangerousWater", [&world](CallContext& call) {
            return call.mReference != 0 && world.isActor(call.mReference) && world.inDangerousWater(call.mReference)
                ? 1.0
                : 0.0;
        });
        // The number of the form type of the base form (equal, no class is more than itself)
        interpreter.setHandler("GetIsObjectType", [&world](CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            const std::optional<std::uint32_t> type = world.baseRecordType(call.mReference);
            if (!type)
                return 0.0;
            return World::formTypeNumber(World::signatureOfRecordType(*type)) == intArgument(call, 0) ? 1.0 : 0.0;
        });
        // The same by the four letters of the record
        interpreter.setHandler("GetIsFormType", [&world](CallContext& call) {
            const std::string_view wanted = textOf(call, 0);
            if (call.mReference == 0 || wanted.empty())
                return 0.0;
            const std::optional<std::uint32_t> type = world.baseRecordType(call.mReference);
            if (!type)
                return 0.0;
            return World::formTypeNumber(wanted) == World::formTypeNumber(World::signatureOfRecordType(*type))
                    && World::formTypeNumber(wanted) >= 0
                ? 1.0
                : 0.0;
        });
        // The executable asks the reference's own function (eval 0x5A45C0, "IsWaterActivator" in its log): the activators
        // that have a water type
        interpreter.setHandler("IsWaterObject", [&world](CallContext& call) {
            return call.mReference != 0 && world.isWaterActivator(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetParentRef", [&world](CallContext& call) {
            return call.mReference == 0 ? 0.0 : static_cast<double>(world.enableParentOf(call.mReference));
        });
        // The pose of a reference when it was loaded; positions in units, angles in degrees
        interpreter.setHandler("GetStartingPos", [&state, &world](CallContext& call) {
            const int axis = axisOf(call, 0);
            if (call.mReference == 0 || axis < 0)
                return 0.0;
            const FormId reference = call.mReference;
            const std::optional<World::Pose> pose = state.starts().poseOf(
                reference, world.recordedPose(reference), [&world, reference] { return world.currentPose(reference); });
            return pose ? pose->mPosition[static_cast<std::size_t>(axis)] : 0.0;
        });
        interpreter.setHandler("GetStartingAngle", [&state, &world](CallContext& call) {
            const int axis = axisOf(call, 0);
            if (call.mReference == 0 || axis < 0)
                return 0.0;
            const FormId reference = call.mReference;
            const std::optional<World::Pose> pose = state.starts().poseOf(
                reference, world.recordedPose(reference), [&world, reference] { return world.currentPose(reference); });
            return pose ? World::degreesOf(pose->mAngle[static_cast<std::size_t>(axis)]) : 0.0;
        });
        // The start of the executable's own SetAtStart is the pose the reference has now
        interpreter.setHandler("SetAtStart", [&state, &world](CallContext& call) {
            if (call.mReference != 0)
                if (const std::optional<World::Pose> now = world.currentPose(call.mReference))
                    state.starts().setToNow(call.mReference, *now);
            return 0.0;
        });
        interpreter.setHandler("GetDefaultOpen", [&state, &world](CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            const int set = state.defaultOpen(call.mReference);
            return (set != 0 ? set > 0 : world.recordedDefaultOpen(call.mReference)) ? 1.0 : 0.0;
        });
        // A door that is set to open by default opens at once, and one that is not closes
        interpreter.setHandler("SetDefaultOpen", [&state, &world](CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            const bool open = intArgument(call, 0) != 0;
            state.setDefaultOpen(call.mReference, open);
            world.setDoorOpen(call.mReference, open);
            return 0.0;
        });
        interpreter.setHandler("SetSize", [&world](CallContext& call) {
            if (call.mReference != 0 && world.isActor(call.mReference))
                world.setActorSize(call.mReference, World::actorSizeResult(argument(call, 0)));
            return 0.0;
        });
        interpreter.setHandler("ModScale", [&placement](CallContext& call) {
            Placement where;
            if (call.mReference != 0 && placement.placement(call.mReference, where))
                placement.setScale(call.mReference, World::modScaleResult(where.mScale, argument(call, 0)));
            return 0.0;
        });
        interpreter.setHandler("SetRigidBodyMass", [&world](CallContext& call) {
            if (call.mReference != 0)
                world.setRigidBodyMass(call.mReference, argument(call, 0));
            return 0.0;
        });
        // The angle grows by the degrees a second for the seconds of the frame, and is kept from 0 to a full turn
        interpreter.setHandler("Rotate", [&world, &placement](CallContext& call) {
            const int axis = axisOf(call, 0);
            Placement where;
            if (call.mReference == 0 || axis < 0 || !placement.placement(call.mReference, where))
                return 0.0;
            const double turn = static_cast<double>(intArgument(call, 1)) * (pi / 180.0) * world.secondsPassed();
            placement.setAngle(call.mReference, axis, wrapAngle(where.mAngle[static_cast<std::size_t>(axis)] + turn));
            return 0.0;
        });
        interpreter.setHandler("Reset3DState", [&world](CallContext& call) {
            if (call.mReference != 0)
                world.reset3DState(call.mReference);
            return 0.0;
        });
        interpreter.setHandler("SetAllVisible", [&state](CallContext& call) {
            state.setAllVisible(intArgument(call, 0) != 0);
            return 0.0;
        });
        interpreter.setHandler("SetLastExtDoorActivated", [&state](CallContext& call) {
            state.setLastExteriorDoor(formArgument(call, 0));
            return 0.0;
        });
        // The navigation mesh is told by its form id written as hexadecimal text
        interpreter.setHandler("EnableNavMesh", [&state](CallContext& call) {
            if (const FormId navMesh = parseHex(textOf(call, 0)); navMesh != 0)
                state.setNavMesh(navMesh, true);
            return 0.0;
        });
        interpreter.setHandler("DisableNavMesh", [&state](CallContext& call) {
            if (const FormId navMesh = parseHex(textOf(call, 0)); navMesh != 0)
                state.setNavMesh(navMesh, false);
            return 0.0;
        });
        interpreter.setHandler("PersistentRefCount",
            [&world](CallContext&) { return static_cast<double>(world.persistentReferenceCount()); });

        // ---- The sky and the clock -----------------------------------------------------------------------------------
        interpreter.setHandler("GetIsCurrentWeather", [&world](CallContext& call) {
            return World::isCurrentWeather(world.sky(), formArgument(call, 0)) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetCurrentWeatherPercent",
            [&world](CallContext&) { return World::weatherPercent(world.sky()); });
        interpreter.setHandler("GetWindSpeed", [&world](CallContext&) { return World::windSpeed(world.sky()); });
        interpreter.setHandler("IsCloudy", [&world](CallContext&) { return World::classificationShare(world.sky(), 0x02); });
        interpreter.setHandler(
            "IsPleasant", [&world](CallContext&) { return World::classificationShare(world.sky(), 0x01); });
        interpreter.setHandler("IsSnowing", [&world](CallContext&) { return World::isSnowing(world.sky()) ? 1.0 : 0.0; });
        // Without the second argument the weather is the one the climate has from now on and the sky fades to it; with
        // it the weather is held over the climate's
        interpreter.setHandler("SetWeather", [&world](CallContext& call) {
            if (const FormId weather = formArgument(call, 0); weather != 0)
                world.setWeather(weather, intArgument(call, 1) != 0);
            return 0.0;
        });
        interpreter.setHandler("IsTimePassing", [&world](CallContext&) { return world.timePasses() ? 1.0 : 0.0; });
        // The multiplier of the time of the whole game: 1 when the script gives none
        interpreter.setHandler("SetGlobalTimeMultiplier", [&world](CallContext& call) {
            world.setTimeMultiplier(call.mArguments.empty() ? 1.0 : argument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("RemapWaterType", [&state](CallContext& call) {
            const FormId from = formArgument(call, 0);
            if (from != 0)
                state.remapWater(from, formArgument(call, 1));
            return 0.0;
        });
        interpreter.setHandler("TriggerLODApocalypse", [&state](CallContext&) {
            state.setLodApocalypse(true);
            return 0.0;
        });
        interpreter.setHandler("SetImageSpace", [&state](CallContext& call) {
            state.setImageSpace(formArgument(call, 0));
            return 0.0;
        });

        // ---- The machine ---------------------------------------------------------------------------------------------
        interpreter.setHandler("IsWin32",
            [&world](CallContext&) { return World::isWin32(world.padConnected(), world.padDisabled()) ? 1.0 : 0.0; });
        interpreter.setHandler("ExitGame", [&world](CallContext&) {
            world.exitGame();
            return 0.0;
        });
        interpreter.setHandler("GetDLCCount", [&world](CallContext&) { return static_cast<double>(world.dlcCount()); });
        interpreter.setHandler("IsGoreDisabled", [&world](CallContext&) { return world.goreDisabled() ? 1.0 : 0.0; });
        interpreter.setHandler("GetPlayerControlsDisabled", [&world](CallContext& call) {
            // Seven flags, each 1 when the call leaves it out
            std::array<bool, 7> chosen{};
            for (std::size_t i = 0; i < chosen.size(); ++i)
                chosen[i] = i >= call.mArguments.size() || argument(call, i) != 0;
            return (world.playerControlsDisabled() & World::controlMask(chosen)) != 0 ? 1.0 : 0.0;
        });
        interpreter.setHandler("IsPC1stPerson", [&world](CallContext&) { return world.playerFirstPerson() ? 1.0 : 0.0; });
        interpreter.setHandler("IsPlayerActionActive", [&state](CallContext& call) {
            return state.actions().active(intArgument(call, 0)) ? 1.0 : 0.0;
        });
        interpreter.setHandler(
            "GetLastPlayerAction", [&state](CallContext&) { return static_cast<double>(state.actions().last()); });
        // PlayBink: the file with its extension, an allowed skip (the second argument, 0 when it is left out)
        interpreter.setHandler("PlayBink", [&world](CallContext& call) {
            if (const std::string_view file = textOf(call, 0); !file.empty())
                world.playMovie(file, argument(call, 1) != 0);
            return 0.0;
        });

        // ---- Screens -------------------------------------------------------------------------------------------------
        interpreter.setHandler("ForceTerminalBack", [&world](CallContext&) {
            world.forceTerminalBack();
            return 0.0;
        });
        interpreter.setHandler("AlwaysShowActorSubtitles", [&state, &world](CallContext& call) {
            if (call.mReference != 0 && world.isActor(call.mReference))
                state.setAlwaysSubtitles(call.mReference, intArgument(call, 0) > 0);
            return 0.0;
        });
        interpreter.setHandler("EnableLoadingMenu", [&state](CallContext& call) {
            state.setLoadingMenu(intArgument(call, 0) > 0);
            return 0.0;
        });
        interpreter.setHandler("IsLoadingMenuEnabled", [&state](CallContext&) { return state.loadingMenu() ? 1.0 : 0.0; });
        interpreter.setHandler("ShowDialogSubtitles", [&state](CallContext& call) {
            state.setDialogSubtitles(argument(call, 0) != 0);
            return 0.0;
        });
        // No code of the executables ever gives it a value: it is -1 whenever it is asked (FalloutNV.exe 0x119F344)
        interpreter.setHandler("WhichServiceMenu", [](CallContext&) { return -1.0; });
        interpreter.setHandler("ShowTutorialMenu", [&world](CallContext& call) {
            world.showTutorial(formArgument(call, 0));
            return 0.0;
        });
        // The first argument 1 goes another way in the executable (0x969FA0, on an object of the player's that nothing here
        // has); otherwise the menu of sleep (the second argument is 1) or of wait opens (0x7054F0, FalloutNV.exe 0x5E0090)
        interpreter.setHandler("ShowSleepWaitMenu", [&world](CallContext& call) {
            if (intArgument(call, 0) != 1)
                world.showSleepWaitMenu(intArgument(call, 1) == 1);
            return 0.0;
        });
        interpreter.setHandler("ShowAllMapMarkers", [&world](CallContext& call) {
            // The first flag shows (or hides) them; the second makes the shown ones places to travel to and the third
            // leaves alone the ones the record marks to stay hidden: both are 1 when the call leaves them out
            const bool withTravel = call.mArguments.size() < 2 || argument(call, 1) != 0;
            const bool keepHidden = call.mArguments.size() < 3 || argument(call, 2) != 0;
            world.showAllMapMarkers(argument(call, 0) != 0, withTravel, keepHidden);
            return 0.0;
        });
        interpreter.setHandler("ShowComputersInterface", [&world](CallContext& call) {
            if (call.mReference != 0)
                world.openTerminal(call.mReference);
            return 0.0;
        });
        interpreter.setHandler("ShowHackingMiniGame", [&world](CallContext& call) {
            if (call.mReference != 0)
                world.openTerminal(call.mReference);
            return 0.0;
        });
        interpreter.setHandler("ShowLockpickMenu", [&world](CallContext&) {
            world.note("ShowLockpickMenu", "the lock screen is for a lock that a reference has, and this has none");
            return 0.0;
        });
        interpreter.setHandler("ShowLockpickMenuDebug", [&world](CallContext&) {
            world.note("ShowLockpickMenuDebug", "the lock screen is for a lock that a reference has, and this has none");
            return 0.0;
        });
        interpreter.setHandler("ShowViewerStrings", [&world](CallContext&) {
            world.note("ShowViewerStrings", "a debug command that prints strings of the loaded model");
            return 0.0;
        });
        // The plain menus of the casino games have no casino to open: the executable's own call reads a casino that the
        // command does not give (FalloutNV.exe 0x5CF040, 0x5CF0F0, 0x5CF1A0)
        interpreter.setHandler("ShowSlotMachineMenu", [&world](CallContext&) {
            world.note("ShowSlotMachineMenu", "the command has no casino to open the table for");
            return 0.0;
        });
        interpreter.setHandler("ShowBlackJackMenu", [&world](CallContext&) {
            world.note("ShowBlackJackMenu", "the command has no casino to open the table for");
            return 0.0;
        });
        interpreter.setHandler("ShowRouletteMenu", [&world](CallContext&) {
            world.note("ShowRouletteMenu", "the command has no casino to open the table for");
            return 0.0;
        });

        // ---- Blood on the screen and the shake of the interface ------------------------------------------------------
        interpreter.setHandler("TriggerScreenSplatter", [&state, &world](CallContext& call) {
            state.splatter().triggerCustom(intArgument(call, 0), argument(call, 1), argument(call, 2), argument(call, 3),
                [&world] { return world.randomUnit(); });
            return 0.0;
        });
        interpreter.setHandler("TriggerScreenBlood", [&state, &world](CallContext& call) {
            state.splatter().triggerBlood(intArgument(call, 0), [&world] { return world.randomUnit(); });
            return 0.0;
        });
        interpreter.setHandler("SetScreenSplatterFade", [&state](CallContext& call) {
            state.splatter().setFade(argument(call, 0), intArgument(call, 1));
            return 0.0;
        });
        interpreter.setHandler("SetScreenSplatterTexture", [&state](CallContext& call) {
            state.splatter().setTexture(intArgument(call, 1), textOf(call, 0));
            return 0.0;
        });
        interpreter.setHandler("ClearScreenSplatter", [&state](CallContext&) {
            state.splatter().clear();
            return 0.0;
        });
        interpreter.setHandler("TriggerHUDShudder", [&state](CallContext& call) {
            // The numbers are optional; the fourth is read by the executable and not used
            state.startShudder(call.mArguments.size() > 0 ? argument(call, 0) : World::HudShudder::defaultDuration,
                call.mArguments.size() > 1 ? argument(call, 1) : World::HudShudder::defaultAmplitude,
                call.mArguments.size() > 2 ? argument(call, 2) : World::HudShudder::defaultFrequency);
            return 0.0;
        });

        // ---- Odd corners ---------------------------------------------------------------------------------------------
        interpreter.setHandler("SetSecuritronExpression", [&state, &world](CallContext& call) {
            const FormId reference = formArgument(call, 0) != 0 ? formArgument(call, 0) : call.mReference;
            const std::string_view first = textOf(call, 1);
            const std::string_view second = textOf(call, 2);
            if (reference == 0)
                return 0.0;
            state.setExpression(reference, first, second);
            world.showTexture(reference, World::securitronScreenShape(), World::securitronTexture(first, second));
            return 0.0;
        });
        interpreter.setHandler("SetPackDuration", [&world](CallContext& call) {
            if (call.mReference != 0 && world.isActor(call.mReference))
                world.setPackageDuration(call.mReference, argument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("HasBeenEaten", [&state, &world](CallContext& call) {
            return call.mReference != 0 && world.isActor(call.mReference) && world.isDeadActor(call.mReference)
                    && state.eaten(call.mReference)
                ? 1.0
                : 0.0;
        });
        // The door's own record is changed, so every reference to that door is (FalloutNV.exe 0x5DC5E0)
        interpreter.setHandler("SetMinimalUse", [&state, &world](CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            const std::optional<std::uint32_t> type = world.baseRecordType(call.mReference);
            if (type && World::signatureOfRecordType(*type) == "DOOR")
                if (const FormId base = world.baseFormOf(call.mReference); base != 0)
                    state.setMinimalUse(base, intArgument(call, 0) != 0);
            return 0.0;
        });
        // Both flags go to the opposite of the demigod one (0x5B7000)
        interpreter.setHandler("ToggleDemigodMode", [&state, &world](CallContext&) {
            const bool on = !state.demigod();
            state.setDemigod(on);
            world.setGodMode(on);
            return 0.0;
        });
        interpreter.setHandler("AddCardToPlayer", [&world](CallContext& call) {
            if (call.mReference != 0)
                if (const FormId card = world.baseFormOf(call.mReference); card != 0)
                    world.addCardToPlayer(card);
            return 0.0;
        });
        interpreter.setHandler("RedeemCode", [&state](CallContext& call) {
            return state.redeemCode(textOf(call, 0)) ? 1.0 : 0.0;
        });
        interpreter.setHandler("IsCodeRedeemed", [&state](CallContext& call) {
            return state.codeRedeemed(textOf(call, 0)) ? 1.0 : 0.0;
        });
        interpreter.setHandler("RevokeCode", [&state](CallContext& call) {
            state.revokeCode(textOf(call, 0));
            return 1.0;
        });

        // ---- The commands that are a dummy in the executables ---------------------------------------------------------
        // (their function is `mov al, 1; ret`: FalloutNV.exe 0x5D4A40, Fallout3.exe 0x52A5E0)
        interpreter.setHandler("EnableLinkedPathPoints", [](CallContext&) { return 0.0; });
        interpreter.setHandler("DisableLinkedPathPoints", [](CallContext&) { return 0.0; });
        interpreter.setHandler("DumpMasterFileSeekData", [](CallContext&) { return 0.0; });
        interpreter.setHandler("StartMasterFileSeekData", [](CallContext&) { return 0.0; });
        interpreter.setHandler("PrintTrackedCells", [](CallContext&) { return 0.0; });
        interpreter.setHandler("EnableCellTracking", [](CallContext&) { return 0.0; });
        interpreter.setHandler("ClearTrackedCells", [](CallContext&) { return 0.0; });
        interpreter.setHandler("ShowClassMenu", [](CallContext&) { return 0.0; });
        interpreter.setHandler("ShowSurgeryMenu", [](CallContext&) { return 0.0; });
        interpreter.setHandler("TrapUpdate", [](CallContext&) { return 0.0; });
        interpreter.setHandler("SetCasinoCheatLevel", [](CallContext&) { return 0.0; });
        // Not a dummy but as good as one: the executable refuses to turn the threaded audio on unless the setting
        // bMultiThreadAudio of the audio is (it is 0 in the default file), and turning it off when it is off changes
        // nothing (FalloutNV.exe 0xAD7230)
        interpreter.setHandler("SetAudioMultithreading", [](CallContext&) { return 0.0; });
        // Fixed 0 and nothing else (0x5DFDE0)
        interpreter.setHandler("IsNoHumanGoreExe", [](CallContext&) { return 0.0; });
    }
}

#include "scriptbuilder.hpp"

#include <components/falloutscript/vanillacommands.hpp>
#include <components/falloutscript/worldcommands.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::Value;
    namespace World = FalloutScript::World;

    constexpr FormId door = 0x01000100;
    constexpr FormId doorBase = 0x01000101;
    constexpr FormId guard = 0x01000200;
    constexpr FormId corpse = 0x01000201;
    constexpr FormId weatherA = 0x01000300;
    constexpr FormId weatherB = 0x01000301;
    constexpr FormId cellA = 0x01000400;

    Value number(double value)
    {
        Value result;
        result.mNumber = value;
        return result;
    }

    Value text(const std::string& value)
    {
        Value result;
        result.mText = value;
        result.mIsText = true;
        return result;
    }

    std::uint32_t signature(const char (&letters)[5])
    {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(letters[0]))
            | (static_cast<std::uint32_t>(static_cast<unsigned char>(letters[1])) << 8)
            | (static_cast<std::uint32_t>(static_cast<unsigned char>(letters[2])) << 16)
            | (static_cast<std::uint32_t>(static_cast<unsigned char>(letters[3])) << 24);
    }

    class FakeWorld : public FalloutScript::WorldWorld
    {
    public:
        bool mSightKnown = true;
        World::SightFacts mFacts;
        bool mRayClear = true;
        std::vector<std::pair<World::Vec3, World::Vec3>> mRays;
        std::map<FormId, std::string> mCellOfReference;
        std::map<FormId, std::string> mCellNames;
        std::map<FormId, std::uint32_t> mTypes;
        std::map<FormId, FormId> mBases;
        std::map<FormId, FormId> mParents;
        std::map<FormId, World::Pose> mRecorded;
        std::map<FormId, World::Pose> mNow;
        bool mWater = false;
        int mPersistent = 0;
        std::set<FormId> mActors;
        std::set<FormId> mDead;
        std::set<FormId> mWaterActivators;
        std::set<FormId> mRecordedOpen;
        std::vector<std::pair<FormId, bool>> mDoors;
        std::vector<std::pair<FormId, double>> mSizes;
        double mSeconds = 0.5;
        World::SkyView mSky;
        std::vector<std::pair<FormId, bool>> mWeathers;
        bool mTimePasses = false;
        std::vector<double> mMultipliers;
        bool mPad = false;
        bool mPadOff = false;
        bool mGore = false;
        int mDlc = 0;
        std::uint8_t mControls = 0;
        bool mFirst = true;
        int mExits = 0;
        int mBacks = 0;
        std::vector<FormId> mTerminals;
        std::vector<FormId> mTutorials;
        std::vector<bool> mRests;
        struct Markers
        {
            bool mShow;
            bool mTravel;
            bool mKeep;
        };
        std::vector<Markers> mMarkers;
        std::vector<std::pair<std::string, bool>> mMovies;
        struct Texture
        {
            FormId mReference;
            std::string mShape;
            std::string mFile;
        };
        std::vector<Texture> mTextures;
        std::vector<std::pair<FormId, double>> mDurations;
        std::vector<FormId> mCards;
        bool mGod = false;
        std::vector<std::string> mNotes;
        double mRandom = 0.5;

        bool sightFacts(FormId, FormId, World::SightFacts& facts) override
        {
            facts = mFacts;
            return mSightKnown;
        }
        bool rayReaches(const World::Vec3& from, const World::Vec3& to) override
        {
            mRays.emplace_back(from, to);
            return mRayClear;
        }
        std::optional<std::string> cellNameOfReference(FormId reference) override
        {
            const auto found = mCellOfReference.find(reference);
            return found == mCellOfReference.end() ? std::nullopt : std::optional<std::string>(found->second);
        }
        std::optional<std::string> cellNameOfCell(FormId cell) override
        {
            const auto found = mCellNames.find(cell);
            return found == mCellNames.end() ? std::nullopt : std::optional<std::string>(found->second);
        }
        std::optional<std::uint32_t> baseRecordType(FormId reference) override
        {
            const auto found = mTypes.find(reference);
            return found == mTypes.end() ? std::nullopt : std::optional<std::uint32_t>(found->second);
        }
        FormId baseFormOf(FormId reference) override
        {
            const auto found = mBases.find(reference);
            return found == mBases.end() ? 0 : found->second;
        }
        FormId enableParentOf(FormId reference) override
        {
            const auto found = mParents.find(reference);
            return found == mParents.end() ? 0 : found->second;
        }
        std::optional<World::Pose> recordedPose(FormId reference) override
        {
            const auto found = mRecorded.find(reference);
            return found == mRecorded.end() ? std::nullopt : std::optional<World::Pose>(found->second);
        }
        std::optional<World::Pose> currentPose(FormId reference) override
        {
            const auto found = mNow.find(reference);
            return found == mNow.end() ? std::nullopt : std::optional<World::Pose>(found->second);
        }
        bool inDangerousWater(FormId) override { return mWater; }
        int persistentReferenceCount() override { return mPersistent; }
        bool isActor(FormId reference) override { return mActors.count(reference) != 0; }
        bool isDeadActor(FormId reference) override { return mDead.count(reference) != 0; }
        bool isWaterActivator(FormId reference) override { return mWaterActivators.count(reference) != 0; }
        bool recordedDefaultOpen(FormId reference) override { return mRecordedOpen.count(reference) != 0; }
        void setDoorOpen(FormId reference, bool open) override { mDoors.emplace_back(reference, open); }
        void setActorSize(FormId actor, double size) override { mSizes.emplace_back(actor, size); }
        double secondsPassed() override { return mSeconds; }
        World::SkyView sky() override { return mSky; }
        void setWeather(FormId weather, bool hold) override { mWeathers.emplace_back(weather, hold); }
        bool timePasses() override { return mTimePasses; }
        void setTimeMultiplier(double multiplier) override { mMultipliers.push_back(multiplier); }
        bool padConnected() override { return mPad; }
        bool padDisabled() override { return mPadOff; }
        bool goreDisabled() override { return mGore; }
        int dlcCount() override { return mDlc; }
        std::uint8_t playerControlsDisabled() override { return mControls; }
        bool playerFirstPerson() override { return mFirst; }
        void exitGame() override { ++mExits; }
        double randomUnit() override { return mRandom; }
        void forceTerminalBack() override { ++mBacks; }
        void openTerminal(FormId terminal) override { mTerminals.push_back(terminal); }
        void showTutorial(FormId message) override { mTutorials.push_back(message); }
        void showSleepWaitMenu(bool sleeping) override { mRests.push_back(sleeping); }
        void showAllMapMarkers(bool show, bool withTravel, bool keepHidden) override
        {
            mMarkers.push_back({ show, withTravel, keepHidden });
        }
        void playMovie(std::string_view file, bool allowSkip) override { mMovies.emplace_back(std::string(file), allowSkip); }
        void showTexture(FormId reference, std::string_view shape, std::string_view texture) override
        {
            mTextures.push_back({ reference, std::string(shape), std::string(texture) });
        }
        void setPackageDuration(FormId actor, double hundredths) override { mDurations.emplace_back(actor, hundredths); }
        void addCardToPlayer(FormId card) override { mCards.push_back(card); }
        void setGodMode(bool on) override { mGod = on; }
        bool godMode() override { return mGod; }
        void note(std::string_view command, std::string_view) override { mNotes.emplace_back(command); }
    };

    class FakePlacements : public FalloutScript::PlacementWorld
    {
    public:
        std::map<FormId, FalloutScript::Placement> mPlaces;
        std::vector<std::pair<FormId, double>> mScales;
        std::vector<std::tuple<FormId, int, double>> mAngles;

        bool placement(FormId reference, FalloutScript::Placement& result) override
        {
            const auto found = mPlaces.find(reference);
            if (found == mPlaces.end())
                return false;
            result = found->second;
            return true;
        }
        bool isActor(FormId) override { return false; }
        void moveTo(FormId, FormId, const std::array<double, 3>&) override {}
        void setPosition(FormId, int, double) override {}
        void setAngle(FormId reference, int axis, double radians) override { mAngles.emplace_back(reference, axis, radians); }
        void setScale(FormId reference, double scale) override { mScales.emplace_back(reference, scale); }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FalloutScript::Switches mSwitches;
        FalloutScript::WorldState mState{ mSwitches };
        FakeWorld mWorld;
        FakePlacements mPlacements;

        Runner() { FalloutScript::addWorldCommands(mInterpreter, mState, mWorld, mPlacements); }

        double call(const char* name, FormId reference, std::vector<Value> arguments = {})
        {
            const FalloutScript::CommandInfo* info = mCommands.find(name);
            EXPECT_NE(info, nullptr) << name;
            return info == nullptr ? 0.0
                                   : mInterpreter.call(info->mOpcode, reference, reference != 0, std::move(arguments));
        }
        double call(const char* name, std::vector<Value> arguments = {}) { return call(name, 0, std::move(arguments)); }
    };

    World::SkyView::Weather weather(FormId form, std::uint8_t classification, std::uint8_t wind = 0)
    {
        World::SkyView::Weather result;
        result.mPresent = true;
        result.mForm = form;
        result.mClassification = classification;
        result.mWindSpeed = wind;
        return result;
    }

    // ---- Seeing

    TEST(FalloutScriptWorldCommandsTest, a_line_of_sight_is_asked_of_the_host_and_the_rule)
    {
        Runner runner;
        runner.mWorld.mFacts.mTargetLoaded = true;
        runner.mWorld.mFacts.mSameSpace = true;
        runner.mWorld.mFacts.mDistance = 500;
        runner.mWorld.mFacts.mTarget = { 0, 500, 0 };
        runner.mWorld.mFacts.mTargetHeight = 120;
        EXPECT_EQ(runner.call("GetLineOfSight", guard, { number(door) }), 1.0);
        EXPECT_EQ(runner.mWorld.mRays.size(), 1u);
        runner.mWorld.mRayClear = false;
        EXPECT_EQ(runner.call("GetLineOfSight", guard, { number(door) }), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, a_line_of_sight_needs_both_references_and_a_host_that_knows_them)
    {
        Runner runner;
        runner.mWorld.mFacts.mTargetLoaded = true;
        runner.mWorld.mFacts.mSameSpace = true;
        runner.mWorld.mFacts.mDistance = 500;
        runner.mWorld.mFacts.mTarget = { 0, 500, 0 };
        EXPECT_EQ(runner.call("GetLineOfSight", { number(door) }), 0.0);
        EXPECT_EQ(runner.call("GetLineOfSight", guard, { number(0) }), 0.0);
        runner.mWorld.mSightKnown = false;
        EXPECT_EQ(runner.call("GetLineOfSight", guard, { number(door) }), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, with_everything_visible_the_cone_is_enough)
    {
        Runner runner;
        runner.mWorld.mFacts.mTargetLoaded = true;
        runner.mWorld.mFacts.mSameSpace = true;
        runner.mWorld.mFacts.mDistance = 500;
        runner.mWorld.mFacts.mTarget = { 0, 500, 0 };
        runner.mWorld.mRayClear = false;
        EXPECT_EQ(runner.call("GetLineOfSight", guard, { number(door) }), 0.0);
        runner.call("SetAllVisible", { number(1) });
        EXPECT_EQ(runner.call("GetLineOfSight", guard, { number(door) }), 1.0);
        runner.call("SetAllVisible", { number(0) });
        EXPECT_EQ(runner.call("GetLineOfSight", guard, { number(door) }), 0.0);
    }

    // ---- Cells and forms

    TEST(FalloutScriptWorldCommandsTest, the_cell_of_a_reference_is_matched_by_the_start_of_its_name)
    {
        Runner runner;
        runner.mWorld.mCellNames[cellA] = "Goodsprings";
        runner.mWorld.mCellOfReference[guard] = "goodsprings saloon";
        EXPECT_EQ(runner.call("GetInCellParam", guard, { number(cellA), number(guard) }), 1.0);
        EXPECT_EQ(runner.call("GetInCellParam", guard, { number(cellA) }), 1.0);
        runner.mWorld.mCellOfReference[guard] = "Novac";
        EXPECT_EQ(runner.call("GetInCellParam", guard, { number(cellA) }), 0.0);
        EXPECT_EQ(runner.call("GetInCellParam", guard, { number(0) }), 0.0);
        EXPECT_EQ(runner.call("GetInCellParam", { number(cellA) }), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, the_form_type_of_the_base_is_asked_by_number_and_by_letters)
    {
        Runner runner;
        runner.mWorld.mTypes[door] = signature("DOOR");
        EXPECT_EQ(runner.call("GetIsObjectType", door, { number(0x1C) }), 1.0);
        EXPECT_EQ(runner.call("GetIsObjectType", door, { number(0x2A) }), 0.0);
        EXPECT_EQ(runner.call("GetIsFormType", door, { text("DOOR") }), 1.0);
        EXPECT_EQ(runner.call("GetIsFormType", door, { text("door") }), 1.0);
        EXPECT_EQ(runner.call("GetIsFormType", door, { text("NPC_") }), 0.0);
        EXPECT_EQ(runner.call("GetIsFormType", door, { text("XXXX") }), 0.0);
        EXPECT_EQ(runner.call("GetIsFormType", door, { text("") }), 0.0);
        EXPECT_EQ(runner.call("GetIsObjectType", guard, { number(0x2A) }), 0.0);
        EXPECT_EQ(runner.call("GetIsObjectType", { number(0x1C) }), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, the_parent_of_a_reference_is_the_one_its_enable_state_follows)
    {
        Runner runner;
        runner.mWorld.mParents[door] = guard;
        EXPECT_EQ(runner.call("GetParentRef", door), static_cast<double>(guard));
        EXPECT_EQ(runner.call("GetParentRef", guard), 0.0);
        EXPECT_EQ(runner.call("GetParentRef"), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, a_reference_starts_in_the_pose_of_its_record)
    {
        Runner runner;
        World::Pose pose;
        pose.mPosition = { 10, 20, 30 };
        pose.mAngle = { 0, 0, std::numbers::pi / 2 };
        runner.mWorld.mRecorded[door] = pose;
        EXPECT_EQ(runner.call("GetStartingPos", door, { number('Y') }), 20.0);
        EXPECT_EQ(runner.call("GetStartingPos", door, { number(2) }), 30.0);
        EXPECT_NEAR(runner.call("GetStartingAngle", door, { number('Z') }), 90.0, 1e-4);
        EXPECT_EQ(runner.call("GetStartingPos", door, { number('Q') }), 0.0);
        EXPECT_EQ(runner.call("GetStartingPos", guard, { number('X') }), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, set_at_start_makes_the_pose_it_has_now_the_start)
    {
        Runner runner;
        World::Pose old;
        old.mPosition = { 1, 1, 1 };
        World::Pose now;
        now.mPosition = { 7, 8, 9 };
        runner.mWorld.mRecorded[door] = old;
        runner.mWorld.mNow[door] = now;
        EXPECT_EQ(runner.call("GetStartingPos", door, { number('X') }), 1.0);
        runner.call("SetAtStart", door);
        EXPECT_EQ(runner.call("GetStartingPos", door, { number('X') }), 7.0);
        // A reference that is nowhere keeps no start
        runner.call("SetAtStart", guard);
        EXPECT_EQ(runner.call("GetStartingPos", guard, { number('X') }), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, a_door_is_open_by_default_as_its_record_or_a_script_says)
    {
        Runner runner;
        runner.mWorld.mRecordedOpen.insert(door);
        EXPECT_EQ(runner.call("GetDefaultOpen", door), 1.0);
        EXPECT_EQ(runner.call("GetDefaultOpen", guard), 0.0);
        runner.call("SetDefaultOpen", door, { number(0) });
        EXPECT_EQ(runner.call("GetDefaultOpen", door), 0.0);
        runner.call("SetDefaultOpen", guard, { number(1) });
        EXPECT_EQ(runner.call("GetDefaultOpen", guard), 1.0);
        ASSERT_EQ(runner.mWorld.mDoors.size(), 2u);
        EXPECT_EQ(runner.mWorld.mDoors[0], std::make_pair(door, false));
        EXPECT_EQ(runner.mWorld.mDoors[1], std::make_pair(guard, true));
        EXPECT_EQ(runner.call("GetDefaultOpen"), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, the_size_of_an_actor_is_set_for_actors_only)
    {
        Runner runner;
        runner.mWorld.mActors.insert(guard);
        runner.call("SetSize", guard, { number(3) });
        runner.call("SetSize", guard, { number(0) });
        runner.call("SetSize", door, { number(3) });
        ASSERT_EQ(runner.mWorld.mSizes.size(), 2u);
        EXPECT_EQ(runner.mWorld.mSizes[0].second, 3.0);
        EXPECT_EQ(runner.mWorld.mSizes[1].second, 2.0);
    }

    TEST(FalloutScriptWorldCommandsTest, the_scale_is_changed_by_mod_scale_within_its_limits)
    {
        Runner runner;
        FalloutScript::Placement where;
        where.mScale = 1.9;
        runner.mPlacements.mPlaces[door] = where;
        runner.call("ModScale", door, { number(0.5) });
        ASSERT_EQ(runner.mPlacements.mScales.size(), 1u);
        EXPECT_DOUBLE_EQ(runner.mPlacements.mScales[0].second, 2.0);
        runner.call("ModScale", guard, { number(0.5) });
        EXPECT_EQ(runner.mPlacements.mScales.size(), 1u);
    }

    TEST(FalloutScriptWorldCommandsTest, a_reference_turns_by_the_degrees_a_second_for_the_seconds_of_the_frame)
    {
        Runner runner;
        FalloutScript::Placement where;
        where.mAngle = { 0, 0, 6.0 };
        runner.mPlacements.mPlaces[door] = where;
        runner.mWorld.mSeconds = 0.5;
        runner.call("Rotate", door, { number('Z'), number(90) });
        ASSERT_EQ(runner.mPlacements.mAngles.size(), 1u);
        const double expected = std::fmod(6.0 + std::numbers::pi / 4, 2 * std::numbers::pi);
        EXPECT_EQ(std::get<1>(runner.mPlacements.mAngles[0]), 2);
        EXPECT_NEAR(std::get<2>(runner.mPlacements.mAngles[0]), expected, 1e-9);
        runner.call("Rotate", door, { number('Q'), number(90) });
        EXPECT_EQ(runner.mPlacements.mAngles.size(), 1u);
    }

    TEST(FalloutScriptWorldCommandsTest, a_negative_turn_is_kept_from_zero_to_a_full_turn)
    {
        Runner runner;
        FalloutScript::Placement where;
        runner.mPlacements.mPlaces[door] = where;
        runner.mWorld.mSeconds = 1.0;
        runner.call("Rotate", door, { number('X'), number(-90) });
        ASSERT_EQ(runner.mPlacements.mAngles.size(), 1u);
        EXPECT_NEAR(std::get<2>(runner.mPlacements.mAngles[0]), 3 * std::numbers::pi / 2, 1e-9);
    }

    TEST(FalloutScriptWorldCommandsTest, the_navigation_mesh_is_told_by_hexadecimal_text)
    {
        Runner runner;
        runner.call("EnableNavMesh", { text("1000abc") });
        EXPECT_EQ(runner.mState.navMesh(0x1000abc), 1);
        runner.call("DisableNavMesh", { text("1000abc") });
        EXPECT_EQ(runner.mState.navMesh(0x1000abc), -1);
        runner.call("EnableNavMesh", { text("zzz") });
        EXPECT_EQ(runner.mState.navMesh(0), 0);
    }

    TEST(FalloutScriptWorldCommandsTest, the_persistent_references_are_counted_by_the_host)
    {
        Runner runner;
        runner.mWorld.mPersistent = 1234;
        EXPECT_EQ(runner.call("PersistentRefCount"), 1234.0);
    }

    TEST(FalloutScriptWorldCommandsTest, the_dangerous_water_is_for_actors_in_it)
    {
        Runner runner;
        runner.mWorld.mActors.insert(guard);
        EXPECT_EQ(runner.call("IsInDangerousWater", guard), 0.0);
        runner.mWorld.mWater = true;
        EXPECT_EQ(runner.call("IsInDangerousWater", guard), 1.0);
        EXPECT_EQ(runner.call("IsInDangerousWater", door), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, a_water_object_is_an_activator_with_a_water_type)
    {
        Runner runner;
        runner.mWorld.mWaterActivators.insert(door);
        EXPECT_EQ(runner.call("IsWaterObject", door), 1.0);
        EXPECT_EQ(runner.call("IsWaterObject", guard), 0.0);
        EXPECT_EQ(runner.call("IsWaterObject"), 0.0);
    }

    // ---- The sky and the clock

    TEST(FalloutScriptWorldCommandsTest, the_weather_questions_are_answered_from_the_sky)
    {
        Runner runner;
        runner.mWorld.mSky.mOutgoing = weather(weatherA, 0x01, 0);
        runner.mWorld.mSky.mCurrent = weather(weatherB, 0x02, 255);
        runner.mWorld.mSky.mPercent = 0.25;
        EXPECT_EQ(runner.call("GetIsCurrentWeather", { number(weatherB) }), 1.0);
        EXPECT_EQ(runner.call("GetIsCurrentWeather", { number(weatherA) }), 0.0);
        EXPECT_DOUBLE_EQ(runner.call("GetCurrentWeatherPercent"), 0.25);
        EXPECT_DOUBLE_EQ(runner.call("IsCloudy"), 0.25);
        EXPECT_DOUBLE_EQ(runner.call("IsPleasant"), 0.75);
        EXPECT_DOUBLE_EQ(runner.call("GetWindSpeed"), 0.25);
        EXPECT_EQ(runner.call("IsSnowing"), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, a_sky_with_no_weather_answers_with_zero)
    {
        Runner runner;
        EXPECT_EQ(runner.call("GetCurrentWeatherPercent"), 0.0);
        EXPECT_EQ(runner.call("IsCloudy"), 0.0);
        EXPECT_EQ(runner.call("GetWindSpeed"), 0.0);
        EXPECT_EQ(runner.call("GetIsCurrentWeather", { number(weatherA) }), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, a_new_weather_is_set_with_or_without_holding_it)
    {
        Runner runner;
        runner.call("SetWeather", { number(weatherA) });
        runner.call("SetWeather", { number(weatherB), number(1) });
        runner.call("SetWeather", { number(0) });
        ASSERT_EQ(runner.mWorld.mWeathers.size(), 2u);
        EXPECT_EQ(runner.mWorld.mWeathers[0], std::make_pair(weatherA, false));
        EXPECT_EQ(runner.mWorld.mWeathers[1], std::make_pair(weatherB, true));
    }

    TEST(FalloutScriptWorldCommandsTest, the_time_multiplier_is_one_when_the_call_gives_none)
    {
        Runner runner;
        runner.call("SetGlobalTimeMultiplier", { number(0.25) });
        runner.call("SetGlobalTimeMultiplier");
        ASSERT_EQ(runner.mWorld.mMultipliers.size(), 2u);
        EXPECT_EQ(runner.mWorld.mMultipliers[0], 0.25);
        EXPECT_EQ(runner.mWorld.mMultipliers[1], 1.0);
    }

    TEST(FalloutScriptWorldCommandsTest, time_passes_while_the_host_says_a_rest_goes_on)
    {
        Runner runner;
        EXPECT_EQ(runner.call("IsTimePassing"), 0.0);
        runner.mWorld.mTimePasses = true;
        EXPECT_EQ(runner.call("IsTimePassing"), 1.0);
    }

    TEST(FalloutScriptWorldCommandsTest, the_water_remap_the_image_space_and_the_lod_are_kept)
    {
        Runner runner;
        runner.call("RemapWaterType", { number(weatherA), number(weatherB) });
        EXPECT_EQ(runner.mState.waterRemappedTo(weatherA), weatherB);
        // To itself takes the remap away
        runner.call("RemapWaterType", { number(weatherA), number(weatherA) });
        EXPECT_EQ(runner.mState.waterRemappedTo(weatherA), 0u);
        runner.call("SetImageSpace", { number(0x4242) });
        EXPECT_EQ(runner.mState.imageSpace(), 0x4242u);
        EXPECT_FALSE(runner.mState.lodApocalypse());
        runner.call("TriggerLODApocalypse");
        EXPECT_TRUE(runner.mState.lodApocalypse());
        runner.call("SetLastExtDoorActivated", { number(door) });
        EXPECT_EQ(runner.mState.lastExteriorDoor(), door);
    }

    // ---- The machine

    TEST(FalloutScriptWorldCommandsTest, the_machine_is_a_pc_unless_a_pad_is_in_use)
    {
        Runner runner;
        EXPECT_EQ(runner.call("IsWin32"), 1.0);
        runner.mWorld.mPad = true;
        EXPECT_EQ(runner.call("IsWin32"), 0.0);
        runner.mWorld.mPadOff = true;
        EXPECT_EQ(runner.call("IsWin32"), 1.0);
    }

    TEST(FalloutScriptWorldCommandsTest, the_small_answers_come_from_the_host)
    {
        Runner runner;
        runner.mWorld.mDlc = 3;
        runner.mWorld.mGore = true;
        runner.mWorld.mFirst = false;
        EXPECT_EQ(runner.call("GetDLCCount"), 3.0);
        EXPECT_EQ(runner.call("IsGoreDisabled"), 1.0);
        EXPECT_EQ(runner.call("IsPC1stPerson"), 0.0);
        EXPECT_EQ(runner.call("IsNoHumanGoreExe"), 0.0);
        EXPECT_EQ(runner.call("WhichServiceMenu"), -1.0);
    }

    TEST(FalloutScriptWorldCommandsTest, the_disabled_controls_are_asked_by_group)
    {
        Runner runner;
        runner.mWorld.mControls = 0x04;
        EXPECT_EQ(runner.call("GetPlayerControlsDisabled"), 1.0);
        EXPECT_EQ(runner.call("GetPlayerControlsDisabled", { number(1), number(0), number(0) }), 0.0);
        EXPECT_EQ(runner.call("GetPlayerControlsDisabled", { number(0), number(1) }), 1.0);
        runner.mWorld.mControls = 0;
        EXPECT_EQ(runner.call("GetPlayerControlsDisabled"), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, the_actions_of_the_player_are_asked_from_the_state)
    {
        Runner runner;
        runner.mState.actions().raise(9, 2.0);
        EXPECT_EQ(runner.call("IsPlayerActionActive", { number(9) }), 1.0);
        EXPECT_EQ(runner.call("IsPlayerActionActive", { number(2) }), 0.0);
        EXPECT_EQ(runner.call("GetLastPlayerAction"), 9.0);
        runner.mState.update(3.0);
        EXPECT_EQ(runner.call("GetLastPlayerAction"), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, a_video_is_played_with_the_skip_the_script_allows)
    {
        Runner runner;
        runner.call("PlayBink", { text("intro.bik") });
        runner.call("PlayBink", { text("outro.bik"), number(1) });
        runner.call("PlayBink", { text("") });
        ASSERT_EQ(runner.mWorld.mMovies.size(), 2u);
        EXPECT_EQ(runner.mWorld.mMovies[0], std::make_pair(std::string("intro.bik"), false));
        EXPECT_EQ(runner.mWorld.mMovies[1], std::make_pair(std::string("outro.bik"), true));
    }

    TEST(FalloutScriptWorldCommandsTest, leaving_the_game_goes_to_the_host)
    {
        Runner runner;
        runner.call("ExitGame");
        EXPECT_EQ(runner.mWorld.mExits, 1);
    }

    // ---- Screens

    TEST(FalloutScriptWorldCommandsTest, a_terminal_goes_back_and_a_terminal_opens_by_the_command)
    {
        Runner runner;
        runner.call("ForceTerminalBack");
        EXPECT_EQ(runner.mWorld.mBacks, 1);
        runner.call("ShowComputersInterface", door);
        runner.call("ShowHackingMiniGame", guard);
        runner.call("ShowComputersInterface");
        EXPECT_EQ(runner.mWorld.mTerminals, (std::vector<FormId>{ door, guard }));
    }

    TEST(FalloutScriptWorldCommandsTest, the_subtitles_of_a_character_and_of_the_dialogue_are_kept)
    {
        Runner runner;
        runner.mWorld.mActors.insert(guard);
        runner.call("AlwaysShowActorSubtitles", guard, { number(1) });
        EXPECT_TRUE(runner.mState.alwaysSubtitles(guard));
        runner.call("AlwaysShowActorSubtitles", guard, { number(0) });
        EXPECT_FALSE(runner.mState.alwaysSubtitles(guard));
        runner.call("AlwaysShowActorSubtitles", door, { number(1) });
        EXPECT_FALSE(runner.mState.alwaysSubtitles(door));
        runner.call("ShowDialogSubtitles", { number(1) });
        EXPECT_TRUE(runner.mState.dialogSubtitles());
    }

    TEST(FalloutScriptWorldCommandsTest, the_map_markers_flags_are_one_when_the_call_leaves_them_out)
    {
        Runner runner;
        runner.call("ShowAllMapMarkers", { number(1) });
        runner.call("ShowAllMapMarkers", { number(0), number(0), number(0) });
        ASSERT_EQ(runner.mWorld.mMarkers.size(), 2u);
        EXPECT_TRUE(runner.mWorld.mMarkers[0].mShow);
        EXPECT_TRUE(runner.mWorld.mMarkers[0].mTravel);
        EXPECT_TRUE(runner.mWorld.mMarkers[0].mKeep);
        EXPECT_FALSE(runner.mWorld.mMarkers[1].mShow);
        EXPECT_FALSE(runner.mWorld.mMarkers[1].mTravel);
        EXPECT_FALSE(runner.mWorld.mMarkers[1].mKeep);
    }

    TEST(FalloutScriptWorldCommandsTest, a_tutorial_message_is_shown)
    {
        Runner runner;
        runner.call("ShowTutorialMenu", { number(0x777) });
        EXPECT_EQ(runner.mWorld.mTutorials, (std::vector<FormId>{ 0x777 }));
    }

    TEST(FalloutScriptWorldCommandsTest, the_sleep_or_wait_menu_opens_unless_the_first_argument_is_one)
    {
        Runner runner;
        runner.call("ShowSleepWaitMenu", { number(0), number(1) });
        runner.call("ShowSleepWaitMenu", { number(0), number(0) });
        runner.call("ShowSleepWaitMenu", { number(1), number(1) });
        EXPECT_EQ(runner.mWorld.mRests, (std::vector<bool>{ true, false }));
    }

    TEST(FalloutScriptWorldCommandsTest, the_menus_with_no_table_say_so_once_and_do_nothing)
    {
        Runner runner;
        runner.call("ShowSlotMachineMenu");
        runner.call("ShowBlackJackMenu");
        runner.call("ShowRouletteMenu");
        runner.call("ShowLockpickMenu");
        runner.call("ShowViewerStrings");
        EXPECT_EQ(runner.mWorld.mNotes.size(), 5u);
    }

    // ---- Splatters and the shudder

    TEST(FalloutScriptWorldCommandsTest, the_splatters_of_a_script_are_kept_until_their_time_is_over)
    {
        Runner runner;
        runner.call("TriggerScreenSplatter", { number(3), number(1.0), number(1.0), number(1.0) });
        EXPECT_EQ(runner.mState.splatter().quadCount(), 3);
        runner.call("TriggerScreenBlood", { number(2) });
        EXPECT_EQ(runner.mState.splatter().quadCount(), 5);
        runner.mState.update(1.5);
        EXPECT_EQ(runner.mState.splatter().quadCount(), 2);
        runner.call("ClearScreenSplatter");
        EXPECT_EQ(runner.mState.splatter().quadCount(), 0);
    }

    TEST(FalloutScriptWorldCommandsTest, the_texture_and_the_fade_of_the_splatters_are_set)
    {
        Runner runner;
        runner.call("SetScreenSplatterTexture", { text("blood.dds"), number(1) });
        runner.call("SetScreenSplatterTexture", { text("none.dds"), number(7) });
        EXPECT_EQ(runner.mState.splatter().texture(1), "blood.dds");
        runner.call("SetScreenSplatterFade", { number(0.5), number(1) });
        EXPECT_EQ(runner.mState.splatter().fade(), 0.5);
        EXPECT_EQ(runner.mState.splatter().fadeMode(), 1);
    }

    TEST(FalloutScriptWorldCommandsTest, the_shudder_takes_the_defaults_of_the_executable)
    {
        Runner runner;
        runner.call("TriggerHUDShudder");
        runner.mState.update(0.01);
        const double offset = runner.mState.shudderOffset()[0];
        EXPECT_NEAR(offset, std::sin(10 * 0.05) * 0.1 * (1.0 - 10.0 / 500.0), 1e-9);
        runner.call("TriggerHUDShudder", { number(100), number(1.0), number(0.1) });
        runner.mState.update(0.01);
        EXPECT_NEAR(runner.mState.shudderOffset()[0], std::sin(10 * 0.1) * 1.0 * (1.0 - 10.0 / 100.0), 1e-9);
        runner.mState.update(1.0);
        EXPECT_EQ(runner.mState.shudderOffset()[0], 0.0);
    }

    // ---- Odd corners

    TEST(FalloutScriptWorldCommandsTest, a_securitron_shows_the_picture_its_two_words_name)
    {
        Runner runner;
        runner.call("SetSecuritronExpression", door, { number(0), text("Happy"), text("Eyes") });
        ASSERT_EQ(runner.mWorld.mTextures.size(), 1u);
        EXPECT_EQ(runner.mWorld.mTextures[0].mReference, door);
        EXPECT_EQ(runner.mWorld.mTextures[0].mShape, "Screen01:0");
        EXPECT_EQ(runner.mWorld.mTextures[0].mFile, "Textures\\creatures\\Securitron\\Happy_Eyes.dds");
        const auto shown = runner.mState.expression(door);
        ASSERT_TRUE(shown);
        EXPECT_EQ(shown->first, "Happy");
        EXPECT_EQ(shown->second, "Eyes");
        EXPECT_FALSE(runner.mState.expression(guard));
    }

    TEST(FalloutScriptWorldCommandsTest, a_package_duration_is_set_for_actors)
    {
        Runner runner;
        runner.mWorld.mActors.insert(guard);
        runner.call("SetPackDuration", guard, { number(150) });
        runner.call("SetPackDuration", door, { number(150) });
        ASSERT_EQ(runner.mWorld.mDurations.size(), 1u);
        EXPECT_EQ(runner.mWorld.mDurations[0], std::make_pair(guard, 150.0));
    }

    TEST(FalloutScriptWorldCommandsTest, a_corpse_has_been_eaten_when_it_is_a_dead_actor_marked_so)
    {
        Runner runner;
        runner.mWorld.mActors.insert(corpse);
        EXPECT_EQ(runner.call("HasBeenEaten", corpse), 0.0);
        runner.mState.setEaten(corpse, true);
        EXPECT_EQ(runner.call("HasBeenEaten", corpse), 0.0);
        runner.mWorld.mDead.insert(corpse);
        EXPECT_EQ(runner.call("HasBeenEaten", corpse), 1.0);
    }

    TEST(FalloutScriptWorldCommandsTest, minimal_use_is_kept_for_the_base_of_a_door)
    {
        Runner runner;
        runner.mWorld.mTypes[door] = signature("DOOR");
        runner.mWorld.mBases[door] = doorBase;
        runner.call("SetMinimalUse", door, { number(1) });
        EXPECT_TRUE(runner.mState.minimalUse(doorBase));
        runner.call("SetMinimalUse", door, { number(0) });
        EXPECT_FALSE(runner.mState.minimalUse(doorBase));
        // Not for anything that is no door
        runner.mWorld.mTypes[guard] = signature("NPC_");
        runner.mWorld.mBases[guard] = 0x55;
        runner.call("SetMinimalUse", guard, { number(1) });
        EXPECT_FALSE(runner.mState.minimalUse(0x55));
    }

    TEST(FalloutScriptWorldCommandsTest, the_demigod_mode_toggles_the_state_and_the_world)
    {
        Runner runner;
        runner.call("ToggleDemigodMode");
        EXPECT_TRUE(runner.mState.demigod());
        EXPECT_TRUE(runner.mWorld.mGod);
        runner.call("ToggleDemigodMode");
        EXPECT_FALSE(runner.mState.demigod());
        EXPECT_FALSE(runner.mWorld.mGod);
    }

    TEST(FalloutScriptWorldCommandsTest, a_card_is_given_by_the_base_of_the_reference_it_is_called_on)
    {
        Runner runner;
        runner.mWorld.mBases[door] = 0x9999;
        runner.call("AddCardToPlayer", door);
        runner.call("AddCardToPlayer", guard);
        runner.call("AddCardToPlayer");
        EXPECT_EQ(runner.mWorld.mCards, (std::vector<FormId>{ 0x9999 }));
    }

    TEST(FalloutScriptWorldCommandsTest, a_code_is_redeemed_checked_and_revoked_without_regard_to_case)
    {
        Runner runner;
        EXPECT_EQ(runner.call("IsCodeRedeemed", { text("ABC-1") }), 0.0);
        EXPECT_EQ(runner.call("RedeemCode", { text("ABC-1") }), 1.0);
        EXPECT_EQ(runner.call("IsCodeRedeemed", { text("abc-1") }), 1.0);
        EXPECT_EQ(runner.call("IsCodeRedeemed", { text("other") }), 0.0);
        EXPECT_EQ(runner.call("RevokeCode", { text("Abc-1") }), 1.0);
        EXPECT_EQ(runner.call("IsCodeRedeemed", { text("abc-1") }), 0.0);
        EXPECT_EQ(runner.call("RedeemCode", { text("") }), 0.0);
    }

    TEST(FalloutScriptWorldCommandsTest, the_dummy_commands_do_nothing_and_give_zero)
    {
        Runner runner;
        for (const char* name : { "EnableLinkedPathPoints", "DisableLinkedPathPoints", "DumpMasterFileSeekData",
                 "StartMasterFileSeekData", "PrintTrackedCells", "EnableCellTracking", "ClearTrackedCells",
                 "ShowClassMenu", "ShowSurgeryMenu", "TrapUpdate", "SetCasinoCheatLevel", "SetAudioMultithreading" })
        {
            const FalloutScript::CommandInfo* info = runner.mCommands.find(name);
            ASSERT_NE(info, nullptr) << name;
            EXPECT_TRUE(runner.mInterpreter.hasHandler(info->mOpcode)) << name;
            EXPECT_EQ(runner.call(name), 0.0) << name;
        }
    }

    TEST(FalloutScriptWorldCommandsTest, a_new_game_forgets_what_does_not_outlast_one)
    {
        Runner runner;
        runner.call("SetAllVisible", { number(1) });
        runner.call("ShowDialogSubtitles", { number(1) });
        runner.mState.actions().raise(3, 10);
        runner.call("TriggerScreenSplatter", { number(2), number(5), number(1), number(1) });
        runner.call("TriggerHUDShudder");
        runner.mState.setExpression(door, "a", "b");
        runner.mState.clear();
        EXPECT_FALSE(runner.mState.allVisible());
        EXPECT_FALSE(runner.mState.dialogSubtitles());
        EXPECT_EQ(runner.mState.actions().last(), 0);
        EXPECT_EQ(runner.mState.splatter().quadCount(), 0);
        EXPECT_EQ(runner.mState.shudderOffset()[0], 0.0);
        EXPECT_FALSE(runner.mState.expression(door));
    }

    TEST(FalloutScriptWorldCommandsTest, the_loading_menu_is_enabled_by_a_number_above_zero_and_disabled_by_any_other)
    {
        Runner runner;
        EXPECT_EQ(runner.call("IsLoadingMenuEnabled"), 0.0);
        runner.call("EnableLoadingMenu", { number(1) });
        EXPECT_EQ(runner.call("IsLoadingMenuEnabled"), 1.0);
        runner.call("EnableLoadingMenu", { number(0) });
        EXPECT_EQ(runner.call("IsLoadingMenuEnabled"), 0.0);
        runner.call("EnableLoadingMenu", { number(3) });
        runner.call("EnableLoadingMenu", { number(-1) });
        EXPECT_EQ(runner.call("IsLoadingMenuEnabled"), 0.0);
    }
}

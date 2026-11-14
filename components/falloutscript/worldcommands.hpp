#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_WORLDCOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_WORLDCOMMANDS_H

#include <atomic>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "interpreter.hpp"
#include "placementcommands.hpp"
#include "switches.hpp"
#include "worldrules.hpp"

namespace FalloutScript
{
    /// What the commands about the world itself ask of the game (M16 "world"): the sky, the cells, the objects and
    /// the machine. A reference is told by its form id. Most functions have an answer for a host that has nothing to
    /// say, so that a fake in a test needs only what it tests.
    class WorldWorld
    {
    public:
        virtual ~WorldWorld() = default;

        // ---- Seeing
        /// What GetLineOfSight needs to know of the viewer and the target; false when either is not in the world now (or
        /// the host may not look at it on this thread)
        virtual bool sightFacts(FormId /*viewer*/, FormId /*target*/, World::SightFacts& /*facts*/) { return false; }
        /// Whether nothing of the world is between the two places
        virtual bool rayReaches(const World::Vec3& /*from*/, const World::Vec3& /*to*/) { return false; }

        // ---- Cells and forms
        /// The name of the cell the reference stands in; none for a reference that is nowhere
        virtual std::optional<std::string> cellNameOfReference(FormId /*reference*/) { return std::nullopt; }
        /// The name of the cell record (what GetInCellParam compares with)
        virtual std::optional<std::string> cellNameOfCell(FormId /*cell*/) { return std::nullopt; }
        /// The record type of the base form of the reference (its four letters as a number), none when unknown
        virtual std::optional<std::uint32_t> baseRecordType(FormId /*reference*/) { return std::nullopt; }
        /// The base form of the reference, 0 when unknown
        virtual FormId baseFormOf(FormId /*reference*/) { return 0; }
        /// The reference its enable state follows (the XESP of the record), 0 for none
        virtual FormId enableParentOf(FormId /*reference*/) { return 0; }
        /// The pose that the reference's record gives it, and the pose it has now
        virtual std::optional<World::Pose> recordedPose(FormId /*reference*/) { return std::nullopt; }
        virtual std::optional<World::Pose> currentPose(FormId /*reference*/) { return std::nullopt; }
        /// Whether the actor is in the water of its cell and that water hurts
        virtual bool inDangerousWater(FormId /*actor*/) { return false; }
        /// How many references the loaded plugins mark as persistent
        virtual int persistentReferenceCount() { return 0; }
        virtual bool isActor(FormId /*reference*/) { return false; }
        virtual bool isDeadActor(FormId /*reference*/) { return false; }
        /// Whether the base of the reference is an activator that has a water type (a sink, a toilet, a fountain)
        virtual bool isWaterActivator(FormId /*reference*/) { return false; }

        // ---- Objects
        /// Whether the reference's record has it open by default
        virtual bool recordedDefaultOpen(FormId /*reference*/) { return false; }
        /// A door (or anything that opens) goes to open or closed at once
        virtual void setDoorOpen(FormId /*reference*/, bool /*open*/) {}
        virtual void setActorSize(FormId /*actor*/, double /*size*/) {}
        virtual void setRigidBodyMass(FormId /*reference*/, double /*mass*/) {}
        virtual void reset3DState(FormId /*reference*/) {}
        /// The seconds the script's frame covers (GetSecondsPassed)
        virtual double secondsPassed() { return 0; }

        // ---- The sky and the clock
        virtual World::SkyView sky() { return World::SkyView(); }
        /// The sky changes to the weather; with `override` it keeps it over what the climate says
        virtual void setWeather(FormId /*weather*/, bool /*override*/) {}
        /// A rest or a wait has hours left
        virtual bool timePasses() { return false; }
        virtual void setTimeMultiplier(double /*multiplier*/) {}

        // ---- The machine
        virtual bool padConnected() { return false; }
        /// The setting bDisable360Controller of the interface
        virtual bool padDisabled() { return false; }
        virtual bool goreDisabled() { return false; }
        virtual int dlcCount() { return 0; }
        /// The byte of the player's disabled controls (the bits of World::controlBits)
        virtual std::uint8_t playerControlsDisabled() { return 0; }
        virtual bool playerFirstPerson() { return true; }
        /// ExitGame: the global PlayCredits is set (2 when the player has the perk the executable names in New Vegas
        /// and always 1 in Fallout 3) and the game goes back to the main menu
        virtual void exitGame() {}
        virtual double randomUnit() { return 0.5; }

        // ---- Screens
        virtual void forceTerminalBack() {}
        virtual void openTerminal(FormId /*terminal*/) {}
        virtual void showTutorial(FormId /*message*/) {}
        /// ShowSleepWaitMenu: the screen that asks how long to sleep (or wait), for no bed in particular
        virtual void showSleepWaitMenu(bool /*sleeping*/) {}
        /// ShowAllMapMarkers: every marker is shown (or hidden); `withTravel` makes the shown ones places to travel to,
        /// and `keepHidden` leaves alone the ones that the record marks to stay hidden
        virtual void showAllMapMarkers(bool /*show*/, bool /*withTravel*/, bool /*keepHidden*/) {}
        virtual void playMovie(std::string_view /*file*/, bool /*allowSkip*/) {}

        // ---- Odd corners
        /// The screen of a securitron shows the picture (SetSecuritronExpression)
        virtual void showTexture(FormId /*reference*/, std::string_view /*shape*/, std::string_view /*texture*/) {}
        /// The package the actor follows is over when `hundredths` of an hour have gone since now
        virtual void setPackageDuration(FormId /*actor*/, double /*hundredths*/) {}
        virtual void addCardToPlayer(FormId /*card*/) {}
        /// ToggleDemigodMode: the player is invulnerable (or not)
        virtual void setGodMode(bool /*on*/) {}
        virtual bool godMode() { return false; }
        /// A message to the log that a command was taken and did nothing, once
        virtual void note(std::string_view /*command*/, std::string_view /*what*/) {}
    };

    /// What the world commands keep: the switches and tables that a saved game holds (they live in `Switches`, which
    /// the game's save already writes) and the state that does not outlast a game (the splatters on the screen, the
    /// list of the player's actions). It can be used from any thread.
    class WorldState
    {
    public:
        explicit WorldState(Switches& switches);

        /// Forgets everything that is not kept in the switches, as for a new game (the switches are cleared by their own
        /// owner)
        void clear();

        // The tables kept in the switches
        void setNavMesh(FormId navMesh, bool enabled);
        /// 1 enabled, -1 disabled, 0 never set
        int navMesh(FormId navMesh) const;
        void setDefaultOpen(FormId reference, bool open);
        /// 1 set to open, -1 set to closed, 0 never set
        int defaultOpen(FormId reference) const;
        void setEaten(FormId corpse, bool eaten);
        bool eaten(FormId corpse) const;
        void setMinimalUse(FormId doorBase, bool minimal);
        bool minimalUse(FormId doorBase) const;
        void setLastExteriorDoor(FormId door);
        FormId lastExteriorDoor() const;
        void setAlwaysSubtitles(FormId actor, bool always);
        bool alwaysSubtitles(FormId actor) const;
        void setDemigod(bool on);
        bool demigod() const;
        bool redeemCode(std::string_view code);
        bool revokeCode(std::string_view code);
        bool codeRedeemed(std::string_view code) const;
        void remapWater(FormId from, FormId to);
        FormId waterRemappedTo(FormId from) const;
        void setLodApocalypse(bool on);
        bool lodApocalypse() const;
        void setImageSpace(FormId imageSpace);
        FormId imageSpace() const;

        // State that does not outlast a game
        void setAllVisible(bool on) { mAllVisible = on; }
        bool allVisible() const { return mAllVisible; }
        void setDialogSubtitles(bool on) { mDialogSubtitles = on; }
        bool dialogSubtitles() const { return mDialogSubtitles; }
        /// EnableLoadingMenu (FalloutNV.exe 0x5DFBA0) shows the loading menu for a number above 0 and hides it for any other;
        /// IsLoadingMenuEnabled (0x5DFC10) is whether the menu's object exists (the pointer at 0x11DA0C0 is not null).
        /// Only the flag is kept here: the command does not show or hide a menu (a stand-in), and IsLoadingMenuEnabled
        /// answers from the flag.
        void setLoadingMenu(bool on) { mLoadingMenu = on; }
        bool loadingMenu() const { return mLoadingMenu; }
        World::StartingPoses& starts() { return mStarts; }
        World::PlayerActions& actions() { return mActions; }
        World::ScreenSplatter& splatter() { return mSplatter; }
        /// The shudder of the interface; the host asks for its offset every frame
        void startShudder(double durationMs, double amplitude, double frequency);
        void updateShudder(double milliseconds);
        std::array<double, 2> shudderOffset();
        /// What a securitron shows: the two words of its expression
        void setExpression(FormId reference, std::string_view first, std::string_view second);
        std::optional<std::pair<std::string, std::string>> expression(FormId reference) const;
        /// A frame: the splatters age, the shudder moves (the milliseconds of the frame) and the player's actions run down
        void update(double seconds);

    private:
        Switches& mSwitches;
        std::atomic<bool> mAllVisible{ false };
        std::atomic<bool> mDialogSubtitles{ false };
        std::atomic<bool> mLoadingMenu{ false };
        World::StartingPoses mStarts;
        World::PlayerActions mActions;
        World::ScreenSplatter mSplatter;
        mutable std::mutex mMutex;
        World::HudShudder mShudder;
        std::map<FormId, std::pair<std::string, std::string>> mExpressions;
    };

    /// Gives the interpreter the commands of the world: GetLineOfSight, GetInCellParam, IsInDangerousWater,
    /// GetIsObjectType, GetIsFormType, GetParentRef, GetStartingPos, GetStartingAngle, SetAtStart, GetDefaultOpen,
    /// SetDefaultOpen, SetSize, ModScale, SetRigidBodyMass, Rotate, Reset3DState, SetAllVisible, SetLastExtDoorActivated,
    /// EnableNavMesh, DisableNavMesh, SetWeather and the questions about the sky (GetIsCurrentWeather,
    /// GetCurrentWeatherPercent, GetWindSpeed, IsCloudy, IsPleasant, IsSnowing), IsTimePassing,
    /// SetGlobalTimeMultiplier, RemapWaterType, TriggerLODApocalypse, SetImageSpace, IsWin32, ExitGame, GetDLCCount,
    /// IsNoHumanGoreExe, IsGoreDisabled, SetAudioMultithreading, PersistentRefCount, GetPlayerControlsDisabled,
    /// IsPC1stPerson, IsPlayerActionActive, GetLastPlayerAction, ForceTerminalBack, AlwaysShowActorSubtitles,
    /// ShowDialogSubtitles, WhichServiceMenu, ShowTutorialMenu, ShowAllMapMarkers, the splatters of the screen and the
    /// shudder of the interface, ShowComputersInterface, ShowHackingMiniGame, SetSecuritronExpression, SetPackDuration,
    /// HasBeenEaten, SetMinimalUse, ToggleDemigodMode, AddCardToPlayer, RedeemCode, IsCodeRedeemed, RevokeCode, PlayBink
    /// and the commands that the executables leave as a dummy that does nothing (the ones about cell tracking and seek
    /// data, linked path points, the class and surgery menus, TrapUpdate and SetCasinoCheatLevel). Those that the
    /// command table does not have are left out. The maths commands are WorldMath::addMathCommands.
    void addWorldCommands(Interpreter& interpreter, WorldState& state, WorldWorld& world, PlacementWorld& placement);
}

#endif

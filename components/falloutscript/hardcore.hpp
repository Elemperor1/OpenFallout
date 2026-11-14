#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HARDCORE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HARDCORE_H

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "interpreter.hpp"
#include "savestate.hpp"
#include "script.hpp"

namespace FalloutScript
{
    class ActorValues;
    class Magic;

    /// The needs of the hardcore mode of New Vegas: dehydration, hunger and sleep deprivation. Each is a level of the
    /// player (an actor value that goes up as game time passes and down when the player drinks, eats or sleeps) and
    /// each stage of it, from a threshold on, puts a spell on the player (the stage spells of the DEHY, HUNG and SLPD
    /// records); the last stage is the one that kills.
    ///
    /// The rates are the executables'. Time is counted in game minutes: a need goes up by one point each time that
    /// rate x time scale / 60 minutes have gone by (a rate is an interval: a larger one is slower), by as many points
    /// as whole intervals have passed, and the rest of the time is dropped. While the player sleeps no sleep
    /// deprivation is added. It can be used from any thread.
    class HardcoreNeeds
    {
    public:
        enum Need : int
        {
            Dehydration = 0,
            Hunger = 1,
            Sleep = 2,
        };
        static constexpr int needCount = 3;
        /// The actor values of the needs (Dehydration, Hunger and SleepDeprevation of the executables)
        static constexpr int firstActorValue = 73;
        static int actorValueOf(int need) { return firstActorValue + need; }

        struct Stage
        {
            /// The level of the need from which the stage is in force
            int mThreshold = 0;
            /// The spell it puts on the player
            FormId mSpell = 0;
        };
        /// The stages of a need (the records say them); they are put in order of the threshold here
        using StageFinder = std::function<std::vector<Stage>(int need)>;

        /// A need changed its stage: the executable tells the player so (the actor value change callbacks 0x8C5610,
        /// 0x8C5890 and 0x8C5B10)
        struct Notice
        {
            int mNeed = 0;
            /// The level went up (else it went down)
            bool mIncreased = false;
            /// The spell of the stage the need is in now, 0 for none
            FormId mStage = 0;
        };

        /// A line of what the player is told: the game setting that holds the text, whether the name of the stage spell
        /// follows it (the text of sRadiationSick, a space, then the name), and the face of the Vault Boy that goes with
        /// it (1 very happy, 3 in pain: the kinds of the message line of the display)
        struct NoticeLine
        {
            std::string mSetting;
            bool mStageName = false;
            int mFace = 0;
        };

        /// The lines of a notice, in the order the executable prints them: the word for the level going up (with the
        /// face in pain) or down (very happy), and then either the text of sRadiationSick with the name of the new stage
        /// (in pain) or, when the need left every stage, the word for being no longer sick (very happy)
        static std::vector<NoticeLine> noticeLines(const Notice& notice);

        /// The part of the names of the settings of a need: Dehydration, Hunger, SleepDeprevation
        static const char* settingName(int need);

        HardcoreNeeds(StageFinder stages, ActorValues& values, Magic& magic);

        /// The intervals of the settings (fHCDehydrationRate, fHCStarvationRate, fHCSleepDeprivationRate)
        void setRates(double dehydration, double hunger, double sleep);
        /// Told after the stage spells of the character were changed, which a fatal stage uses to take its health
        void setStageListener(std::function<void(FormId actor)> listener);
        /// Told when a need that went from one stage to another (or out of every stage), as the player is told
        void setNoticeListener(std::function<void(const Notice&)> listener);

        /// Lets the needs of the player go on up to the game minute (counted from any start): the first call, or one
        /// for a time that went backwards (a game that was loaded), only sets where counting starts from
        void update(FormId player, double minutes, double timeScale, bool sleeping);
        /// An hour of sleep goes by: the sleep deprivation gets lower by the amount, or by what there is of it
        void sleepHour(FormId player, double amount);
        /// The hardcore mode is over: the stages come off the player and counting starts over
        void end(FormId player);
        /// The mode is switched off with SetHardcore: the three levels go to nothing (0x969E90), each change telling the
        /// stage it made, and counting starts over
        void reset(FormId player);
        /// A level was changed by something other than the passing of time (food, drink, a script): the stage it is in
        /// now is put on the player, and a change of stage is told
        void refresh(FormId player);
        /// Forgets where counting started from, as for a new game or a game that is loaded
        void clear();

    private:
        /// Puts the spell of the stage the level is in on the player and takes those of the other stages off
        void chooseStage(FormId player, int need);

        StageFinder mStages;
        ActorValues& mValues;
        Magic& mMagic;
        mutable std::mutex mMutex;
        std::array<double, needCount> mRates{ 10.0, 25.0, 50.0 };
        /// The game minute each need was last counted up to, 0 for none yet
        std::array<double, needCount> mStamps{};
        /// The level that the stage was last chosen for, none yet below 0
        std::array<double, needCount> mChosen{ -1.0, -1.0, -1.0 };
        std::function<void(FormId)> mListener;
        std::function<void(const Notice&)> mNotices;
    };

    /// Whether the hardcore mode is on, and whether it has been on all along. The state of the player the executable
    /// keeps in two places: an integer (IsHardcore says 1 for exactly 1, offset 0x7BC of the player) and a byte that
    /// says the mode was never turned off since the game began (IsAlwaysHardcore, offset 0xE38). Both are saved with
    /// the player (0x9590F0 writes them, 0x95A3B0 reads them back). It can be used from any thread.
    class HardcoreMode
    {
    public:
        /// The stored integer (the executable keeps whatever it is given: only 1 is hardcore)
        int value() const { return mValue; }
        bool on() const { return mValue == 1; }
        /// IsAlwaysHardcore: 1 for a new game, 0 after the mode was turned off or the tracking was disabled
        bool always() const { return mTracking; }

        /// 0x969E90: stores the value. A value of 0 given by something other than a load turns the tracking off. The
        /// result tells whether the three needs are to be zeroed (a 0 that is not a load).
        bool set(int value, bool loading = false);

        /// SetHardcore (0x5DE920): only 0 and 1 are taken (the command says "invalid player" for the rest, and so does
        /// it when there is no player); anything but 1 turns the tracking off. The result is false for a value that was
        /// not taken. `zero` tells whether the needs are to be zeroed.
        bool command(int value, bool& zero);

        /// DisableHardcoreTracking (0x5DE9D0): IsAlwaysHardcore answers 0 from then on
        void disableTracking() { mTracking = false; }

        /// A new game (the constructor of the player, 0x938180): the mode is off and the tracking is on
        void reset();

        /// What a saved game holds: the integer and the tracking byte (0x9590F0, 0x95A3B0: the load sets the mode with
        /// the loading flag and turns the tracking off when the byte is 0)
        constexpr static std::uint32_t savedSectionVersion = 1;
        void write(SaveWriter& out) const;
        void read(SaveReader& in, std::uint32_t version);

    private:
        std::atomic<int> mValue{ 0 };
        std::atomic<bool> mTracking{ true };
    };

    /// SetHardcore, IsHardcore, IsAlwaysHardcore and DisableHardcoreTracking. `zeroNeeds` is called when SetHardcore
    /// turns the mode off and the needs are to be zeroed.
    void addHardcoreCommands(Interpreter& interpreter, HardcoreMode& mode, std::function<void()> zeroNeeds);

    /// What the head-up display draws for a need (the row that follows the health, FalloutNV.exe 0x7739F0 and the
    /// style function 0x773E90): the label of each need, in the colour of the display (1) or the alternate one (2, red),
    /// at an alpha that grows with the level. The levels are those the thresholds of the settings fHCHalfAlphaTextThreshold
    /// (200), fHCFullAlphaTextThreshold (400), fHCRedTextThreshold (600) and fHCFlashTextThreshold (800) say.
    class NeedsDisplay
    {
    public:
        struct Tile
        {
            int mColor = 1;
            int mAlpha = 0;
        };

        struct Thresholds
        {
            double mHalfAlpha = 200;
            double mFullAlpha = 400;
            double mRed = 600;
            double mFlash = 800;
        };

        /// The alpha the half-alpha level shows at (0x7C), and the one of the levels from the full one up
        static constexpr int halfAlpha = 0x7C;
        static constexpr int fullAlpha = 0xFF;
        /// The flash goes between 128 and 255 at half a unit for each millisecond (0x1016248, 0x102E430, 0x101E568)
        static constexpr double flashLow = 128.0;
        static constexpr double flashHigh = 255.0;
        static constexpr double flashPerMillisecond = 0.5;

        void setThresholds(const Thresholds& thresholds) { mThresholds = thresholds; }
        const Thresholds& thresholds() const { return mThresholds; }

        /// Moves the flash on to the time (in milliseconds): called once for each update of the display
        void advance(double milliseconds);

        /// The tile for a level: red and flashing from the flash threshold, red from the red one, then the colour of the
        /// display at full alpha and at half alpha, and nothing below
        Tile tile(double level) const;

        /// The level that stands for the crippled parts of the body in the tile of the limbs: nothing for none, and the
        /// level of the half-alpha, full-alpha, red and flash thresholds plus one for one, two, three and four or more
        /// parts that are crippled (0x7739F0)
        double limbLevel(int crippledParts) const;

        /// The flashing alpha now
        double flash() const { return mFlash; }

    private:
        Thresholds mThresholds;
        double mFlash = flashHigh;
        bool mFalling = true;
        double mLast = -1;
    };

    /// The weight ammunition adds to what the player carries (0x48EBC0): its own weight in the hardcore mode of New
    /// Vegas, nothing otherwise
    constexpr double carriedAmmoWeight(bool hardcore, double weight)
    {
        return hardcore ? weight : 0.0;
    }

    /// Whether a companion cannot die (0x87F3D0): the characters that follow the player are essential unless the mode is
    /// on, and the others are as their record and their script say
    constexpr bool companionIsEssential(bool teammate, bool hardcore)
    {
        return teammate && !hardcore;
    }
}

#endif

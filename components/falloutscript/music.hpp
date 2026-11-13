#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MUSIC_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MUSIC_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace FalloutScript::Music
{
    /// What the games' music is made of, as far as it was read from the executables (FalloutNV.exe and Fallout3.exe, the
    /// addresses are in docs/openfallout/m15-notes.md) or from the data. No sound is made here: the engine asks these
    /// rules what to play and when.

    // ---- Combat music (FalloutNV.exe 0x992D90) ----

    /// The game settings that the combat music reads (the defaults are the ones the executables register)
    struct CombatSettings
    {
        float mUpdateTime = 1.f; // fCombatMusicUpdateTime: the seconds between two looks at the fights
        float mStopTime = 5.f; // fCombatMusicStopTime: the seconds the music goes on after the fight seems over
        float mNearInner = 2500.f; // fCombatMusicNearCombatInnerRadius
        float mNearOuter = 3500.f; // fCombatMusicNearCombatOuterRadius
        float mTargetedThreatRatio = 2.f; // fCombatMusicPlayerTargetedThreatRatio
        float mNearStrengthMult = 1.f; // fCombatMusicPlayerNearStrengthMult
    };

    /// A character that is in a fight, as the music sees it
    struct Fighter
    {
        /// It fights the player and has not lost track of it (the executable asks whether the time since it lost the
        /// player is under fCombatDetectionLostTime)
        bool mHuntsPlayer = false;
        /// It fights someone (the player or not)
        bool mInCombat = false;
        /// How far it is from the player, in units
        float mDistance = 0.f;
        /// How much it counts for (the executable's combat strength of the character; the port uses its health)
        float mStrength = 0.f;
        /// A friend of the player is among those it fights with
        bool mAllyNear = false;
    };

    /// Whether the music of a fight plays. Every `mUpdateTime` seconds the fighters are looked at:
    /// - Off: it starts when a fighter that hunts the player has an enemy group (all the fighters that hunt it) whose
    ///   strength is more than the player's divided by `mTargetedThreatRatio` (a weak enemy does not start it), or when
    ///   characters nearer than `mNearInner` fight with a friend of the player among them and their strength is more
    ///   than the player's times `mNearStrengthMult`.
    /// - On: it goes on while a fighter hunts the player or one fights nearer than `mNearOuter`.
    /// When the fight seems over a timer of `mStopTime` seconds starts, and when it has run out at the next look the
    /// music stops.
    class CombatMusic
    {
    public:
        explicit CombatMusic(const CombatSettings& settings = {})
            : mSettings(settings)
        {
        }

        /// Looks at the fights when it is time to, and says whether the music is on now. `seconds` is the time since the
        /// last call, and `playerStrength` how much the player counts for.
        bool update(float seconds, float playerStrength, std::span<const Fighter> fighters);
        bool on() const { return mOn; }
        /// Off again, as for a new game
        void reset();

    private:
        CombatSettings mSettings;
        bool mOn = false;
        float mUntilLook = 0.f;
        std::optional<float> mStopLeft;
    };

    // ---- The track of a folder (FalloutNV.exe 0x591A40) ----

    /// The random numbers the picker uses: an integer in the range, the ends included
    using Random = std::function<int(int, int)>;

    /// Where the pick of a folder starts: a random track (0x591524)
    std::size_t firstTrack(std::size_t count, const Random& random);

    /// The next track of a folder of `count` tracks: a random one that is not the one played last (`last` is the track
    /// played last, and is replaced by the pick). The executable's rule: a track equal to the last one (other than 0)
    /// becomes the first, and a first track that follows the first becomes the second. A folder of one track plays
    /// it, and an empty one none.
    std::optional<std::size_t> nextTrack(std::size_t count, std::size_t& last, const Random& random);

    // ---- The set a media location controller picks (FalloutNV.exe 0x595560) ----

    /// Picks one of the sets of a controller's list and moves it to the end of the list (0x905330 takes it out of the list,
    /// 0x905820 appends it). The pick is the executable's: the first set of the list is picked, and each one after it takes
    /// over with a chance of one in four (a random number from 0 to 3 that is 0). So the set that played last, now the last
    /// of the list, is picked a quarter of the times, the one before it three sixteenths, and so on, and the one that has
    /// waited longest the rest. A list of one set picks it again and again. `list` holds the form ids of the sets, in the
    /// order the controller keeps them; an empty list picks nothing (0).
    std::uint32_t pickSet(std::vector<std::uint32_t>& list, const Random& random);

    /// How a controller's sets are played (the looping options of its `NAM1`: 0 loops, 1 random, 2 retrigger, 3 none;
    /// FalloutNV.exe 0x598000, read by the updates of the location sets and of the dungeon sets): a controller whose
    /// option is 0 loops the tracks of the set, any other plays them once and picks a set again when they are over, after the
    /// controller's own delay (`NAM4`, seconds).
    constexpr bool loopsSets(std::uint8_t loopingOptions)
    {
        return (loopingOptions & 3) == 0;
    }

    // ---- The volume and loop of a music record (FalloutNV.exe 0x830050, 0x830080) ----

    struct Attenuation
    {
        float mDb = 0.f; // never above 0
        bool mLoop = false;
    };
    /// A music record's ANAM: its size in decibels is how much quieter the track is, and a positive one makes the
    /// track loop
    Attenuation attenuationOf(float anam);
    float gainOfDb(float db);

    // ---- Media sets (New Vegas) ----

    /// Whether it is day: the day starts at `dayStart` and the night at `nightStart`, both in 256ths of a day (the
    /// record of a media location controller; with `useDefaults` they are 64 and 255: 6:00 and 23:54)
    bool isDay(float hour, std::uint32_t dayStart, std::uint32_t nightStart, bool useDefaults);

    /// The layer of a location set that plays at a place: the innermost one that is switched on (`enabled`, three
    /// bits: outer, middle, inner) and whose boundary (percent of the marker's radius, from the record) holds
    /// `fraction` (the distance from the marker divided by its radius). 0 outer, 1 middle, 2 inner, none when the
    /// place is outside all of them.
    std::optional<int> layerAt(float fraction, std::uint8_t enabled, const std::array<float, 3>& boundaryPercent);

    // ---- The stingers of an incidental media set (FalloutNV.exe 0x598890) ----

    /// The times of an incidental set, in seconds: DNAM and FNAM are the shortest and the longest wait after the open sound
    /// (heard by day), ENAM and GNAM after the close sound (heard by night)
    struct IncidentalTimes
    {
        float mDayShortest = 0.f;
        float mNightShortest = 0.f;
        float mDayLongest = 0.f;
        float mNightLongest = 0.f;
    };

    /// When the stingers of the incidental set of a region play: the first as soon as there is one, the open sound of the
    /// set by day and the close sound by night, and the next after a random wait between the times of the set for that
    /// half of the day. The wait runs only while the stingers are asked for (the executable keeps one time for all the
    /// sets).
    class Incidental
    {
    public:
        enum class Sound : std::uint8_t
        {
            None,
            Open,
            Close,
        };
        /// `unit` is a random number in [0, 1)
        Sound update(float seconds, bool day, const IncidentalTimes& times, const std::function<double()>& unit);
        void reset() { mWait = 0.f; }
        float wait() const { return mWait; }
        /// The shortest wait the port keeps between two stingers, in seconds: a set whose times are zero would play its
        /// sound again in every frame (the sound is cut off by itself), so it plays the stinger once and then rests
        static constexpr float minimumWait = 1.f;

    private:
        float mWait = 0.f;
    };

    // ---- When the music of a place plays (the pacing the port chose) ----

    /// The seconds the port waits and fades. The fade of a change of music is what the executable registers
    /// (iMusicTransitionInMS, 1000); the battle numbers are the ones New Vegas's battle media sets carry (a fade of 3
    /// seconds, a wait of 10); how long the music of a place rests between two tracks is not in anything that was
    /// read, so the port chose it.
    struct Pacing
    {
        float mTransition = 1.f;
        float mBattleFade = 3.f;
        float mBattleWait = 10.f;
        float mStartDelay = 8.f;
        float mSilenceMin = 60.f;
        float mSilenceMax = 180.f;
    };

    struct DirectorInput
    {
        float mSeconds = 0.f;
        /// The music record of the place the player is in (0: none, as for a place of no music)
        std::uint32_t mExplore = 0;
        /// The record of the music of a fight (0: none)
        std::uint32_t mBattle = 0;
        bool mCombat = false;
        /// A track that the director started is still playing
        bool mPlaying = false;
        /// Something else has the music (a script plays a track, a screen shows)
        bool mBlocked = false;
        /// What the last track was: whether it loops, for the one that is playing or has just ended
        bool mLoops = false;
    };

    struct DirectorAction
    {
        enum class Kind : std::uint8_t
        {
            None,
            Start, // start the track of mMusic with a fade in of mFade seconds
            FadeOut, // fade the track out over mFade seconds
        };
        Kind mKind = Kind::None;
        std::uint32_t mMusic = 0;
        float mFade = 0.f;
    };

    /// Chooses what plays: the music of the place, or the music of a fight over it. A change of place fades the track out
    /// and starts the next after a delay; a track that ends is followed by a rest of random length (a track that
    /// loops, or a battle track while the fight goes on, by none); a fight starts its music at once and gives the
    /// music of the place back after the battle wait.
    class Director
    {
    public:
        explicit Director(const Pacing& pacing = {})
            : mPacing(pacing)
        {
        }

        /// `unit` is a random number in [0, 1)
        DirectorAction update(const DirectorInput& input, const std::function<double()>& unit);
        void reset();
        /// The record being played, 0 when none
        std::uint32_t current() const { return mCurrent; }

    private:
        Pacing mPacing;
        std::uint32_t mCurrent = 0;
        bool mCurrentBattle = false;
        bool mFading = false;
        bool mGrace = false; // a track was started in the last call
        std::uint32_t mLastTarget = 0;
        float mWait = -1.f; // the seconds to wait before a start; negative: not decided yet
    };
}

#endif

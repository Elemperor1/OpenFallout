#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CRIME_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CRIME_H

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include "interpreter.hpp"
#include "objectstate.hpp"
#include "saveaccess.hpp"
#include "savestate.hpp"
#include "vanillacommands.hpp"

namespace FalloutScript
{
    /// The kinds of crime of the executables' crime lists (the five lists of the process lists, in this order). A crime
    /// of the kind Trespass is never made as a crime in either executable: trespass goes by the package of the
    /// watcher (see Law::trespass).
    enum class CrimeKind : int
    {
        Steal = 0,
        Pickpocket = 1,
        Trespass = 2,
        Attack = 3,
        Murder = 4,
    };

    /// What a character says to the player about a crime (the topics of the default dialogue list that the executables
    /// say, category 2: the line of the crime, and the line of the same kind of crime that nobody minds)
    enum class CrimeLine : int
    {
        Crime,
        NoCrime,
        GuardTrespass,
    };

    /// The numbers that the crime rules go by, with what the executables start them with. The two executables agree on
    /// all of them but the karma that a murder costs (see forGame); a game's records change them through the game
    /// settings of the same names.
    struct LawSettings
    {
        /// iStealWarnings and iPickPocketWarnings (2 in both executables): the counts of warnings that a theft is below
        /// to be a warning and nothing else (the count itself is never raised by the executables' code, which this keeps)
        int mStealWarnings = 2;
        int mPickPocketWarnings = 2;
        /// fAITrespassWarningTimer (10 seconds in both executables): the time between the warnings of a trespass
        double mTrespassWarningSeconds = 10.0;
        /// How many warnings the package of a trespass gives before the alarm: constant of the code of both
        /// executables; a cell that is off limits gives none
        int mTrespassWarnings = 3;
        /// fReputationMinorCrimeNeg and fReputationMajorCrimeNeg (New Vegas): what a crime adds to the infamy that the
        /// reputation of the faction of the victim holds
        double mMinorCrimeInfamy = 2.0;
        double mMajorCrimeInfamy = 30.0;
        /// The karma of the player: iKarmaMin, iKarmaMax and iKarmaChangeThreshold (a change smaller than the threshold
        /// is a minor one)
        int mKarmaMin = -1000;
        int mKarmaMax = 1000;
        int mKarmaChangeThreshold = 250;
        /// fKarmaModStealing (-5): what taking from someone who is not evil costs
        double mKarmaStealing = -5.0;
        /// What a murder costs: the victim is not evil (fKarmaModMurderingNonEvilNPC, which is 0 in New Vegas and -100
        /// in Fallout 3), a creature that is not evil (fKarmaModMurderingNonEvilCreature: 0 and -25), and in New Vegas
        /// by how good the victim is (fKarmaModMurderingGoodNPC -50, fKarmaModMurderingVeryGoodNPC -100)
        double mKarmaMurderNonEvilNpc = 0.0;
        double mKarmaMurderNonEvilCreature = 0.0;
        double mKarmaMurderGoodNpc = -50.0;
        double mKarmaMurderVeryGoodNpc = -100.0;
        /// What killing an evil one pays (fKarmaModKillingEvilActor, fKarmaModKillingVeryEvilActor)
        double mKarmaKillingEvil = 1.0;
        double mKarmaKillingVeryEvil = 2.0;
        /// The alarm reaches characters up to this far (iCrimeAlarmRecDistance, 10000 in both executables)
        double mAlarmDistance = 10000.0;
        /// fCrimeDispAttack (-10 in both executables): what hitting a character that is fighting already does to what it
        /// thinks of the player (the only one of the fCrimeDisp settings that the executables read)
        double mCrimeDispAttack = -10.0;
        /// Whether the executable keeps reputations (New Vegas)
        bool mReputations = true;
        /// Whether the murder of someone not evil has the grades of New Vegas (a good or a very good victim costs more)
        bool mKarmaGrades = true;

        using Lookup = std::function<std::optional<double>(std::string_view)>;

        /// The values the executable of the game starts with
        static LawSettings forGame(Game game);
        /// The same, with the game settings that the lookup (a name, as in the records) has
        static LawSettings fromGameSettings(Game game, const Lookup& lookup);
    };

    /// What the records of the game say about the factions that the rules look at
    class LawRecords
    {
    public:
        virtual ~LawRecords() = default;

        /// The reference of the player, who is the criminal of every crime
        virtual FormId player() = 0;
        virtual LawSettings lawSettings() = 0;
        /// Whether the faction tracks crime (the flag of its record that makes the members care about what is done to
        /// the others): the executables keep it at 0x100 of the flags of a loaded faction (0x40 of the record)
        virtual bool tracksCrime(FormId faction) = 0;
        /// Whether the faction is evil (0x02 of its record)
        virtual bool evil(FormId faction) = 0;
        /// The reputation that the record of the faction names, 0 for none (New Vegas)
        virtual FormId reputationOf(FormId faction) = 0;
    };

    /// The flags that a faction keeps about the player (the executables keep them in the flags of the loaded faction)
    enum class FactionFlag : int
    {
        /// 0x10: the members count the player as an enemy; set when a crime is seen by one of them, cleared by the
        /// commands that clear it. This is the flag that SetPCEnemyofFaction sets; the state lives in ObjectState.
        PlayerEnemy,
        /// 0x08: the player was expelled from the faction
        PlayerExpelled,
        /// 0x20 and 0x40: the flags that scripts set to tell that the player attacked or murdered one of the faction
        /// (nothing but scripts sets them in the executables)
        PlayerAttack,
        PlayerMurder,
    };

    /// One of the characters of the world that can have seen the crime, for the rule to decide what it does
    struct Witness
    {
        FormId mActor = 0;
        /// The character notices the criminal (its level of detection is above nothing), is alive and does not ignore
        /// crime
        bool mWatching = false;
        /// The character is a friend or ally of the victim (it joins a fight for it even when no crime is its concern)
        bool mFriendOfVictim = false;
        /// The factions of the character
        std::vector<FormId> mFactions;
    };

    /// What the rule tells the world to do after a crime or an alarm
    struct Reaction
    {
        enum class Kind : std::uint8_t
        {
            /// Says the line of the crime to the criminal
            Say,
            /// Says the line of the same crime that nobody minds
            SayNoCrime,
            /// Fights the criminal
            Attack,
            /// Walks up to the criminal and says the line of a warning, which is the line of the crime
            Warn,
        };

        FormId mActor = 0;
        Kind mKind = Kind::Say;
        CrimeKind mCrime = CrimeKind::Attack;
    };

    /// The reputation of a faction takes infamy
    struct Infamy
    {
        FormId mReputation = 0;
        double mAmount = 0;
    };

    /// What came of a crime, a theft or an alarm
    struct Outcome
    {
        /// A crime was recorded (its number), 0 when none was
        int mCrime = 0;
        std::vector<Reaction> mReactions;
        /// The factions that count the player as an enemy now
        std::vector<FormId> mEnemies;
        /// The infamy that the reputations take (New Vegas)
        std::vector<Infamy> mInfamy;
        /// The change of the karma of the player, as asked (the clamping to the limits is Law::karma's)
        int mKarma = 0;
        /// The player became a murderer (IsPCAMurderer is true from now on)
        bool mMurderer = false;
        /// What the victim thinks of the player changes by this (the pair modifier of the disposition)
        double mVictimDisposition = 0;
    };

    /// The crime and the law of the games, as the executables work them, in one place: what is recorded when the player
    /// hits, kills, steals from, picks the pocket of or trespasses on someone, who sees it and cares, which factions
    /// turn on the player, how the warnings go, how the counts and the marks that scripts ask about stand, and how the
    /// flags are cleared. The world gives the rule the characters that can have seen (who they are, whether they
    /// notice the player) and carries out what it says (who says what, who fights, what the reputations take); the
    /// rule has no engine types. It can be used from any thread.
    class Law
    {
        template <class T>
        friend struct SaveAccess;

    public:
        Law(LawRecords& records, ObjectState& objects);

        /// Forgets everything, as for a new game
        void clear();

        // ---- the flags of the factions and the counts (the commands that ask and set) ----

        bool flag(FormId faction, FactionFlag which);
        void setFlag(FormId faction, FactionFlag which, bool on);
        /// Clears the player-enemy flag of every one of the factions
        void clearEnemyFlags(const std::vector<FormId>& factions);
        /// Whether any of the factions counts the player as an enemy
        bool anyEnemy(const std::vector<FormId>& factions);

        /// The crimes that the factions have seen done to them (a character's counts are the sums over its factions)
        int minorCrimes(const std::vector<FormId>& factions);
        int majorCrimes(const std::vector<FormId>& factions);
        /// The counts of the player itself: the number of minor and major crimes
        int playerMinorCrimes();
        int playerMajorCrimes();
        /// What one character counts as its own minor crimes (a theft or a pickpocket counts one for the victim)
        int actorMinorCrimes(FormId actor);

        /// Whether the player is a murderer (a murder was seen), and the flag that scripts clear
        bool murderer();
        void setMurderer(bool on);

        /// The value of the goods that were taken from others, by the kind of crime
        std::int64_t valueOf(CrimeKind kind);

        /// The value of stolen goods that the player sold, which barter counts
        double amountSoldStolen();
        void modAmountSoldStolen(double change);

        // ---- the crimes ----

        /// Whether a crime of the kind (any kind with -1) by the criminal is held, with the victim given (0 stands for
        /// any) and the witness given (0 for any), 0x972600
        bool hasCrime(FormId victim, FormId criminal, int kind, FormId witness = 0, int number = -1);
        /// The number of crimes held
        std::size_t crimeCount();
        /// StopCombatAlarmOnActor (0x972840): forgets the crimes that the character is the victim of or a witness to,
        /// and clears the player-enemy flags that no crime that is left holds. The factions that were cleared are given.
        std::vector<FormId> stopAlarm(FormId actor);
        /// GetActorCrimePlayerEnemy (0x5A4930): 1 when the list of crimes that the character holds is not empty and 0 for
        /// anyone else (a faction's flag has no part in it). The crimes that the character is the victim of or a witness
        /// to are those (that a witness is put in the list was not read, the victim's entry was: 0x8C05C8 and 0x972134).
        bool crimeListed(FormId actor);

        /// The clock of the crimes, in seconds of play (the executables keep a clock that the frames add their length
        /// to, FalloutNV.exe 0x11F1BF0 with the adder at 0x9915E0, and stamp each crime with it in the constructor,
        /// 0x9EB5E9 to 0x9EB600). It is not saved: a crime that was loaded counts as made at the moment of the load.
        void advance(double seconds);
        /// The routine at 0x971FD0, which the combat's yield (0x967DA0) calls with `fCombatYieldTime` (10 seconds in the
        /// executables, no plugin sets it): forgets the crimes that are no older than that (the age is the clock minus the
        /// stamp, 0x435E00, and a crime is kept when the limit is below the age, 0x972099) and clears the player-enemy
        /// flags that no crime that is left holds (0x9722E0, 0x47EB90). The factions that were cleared are given.
        std::vector<FormId> forgetRecent(double seconds);

        // ---- the events ----

        struct Blow
        {
            /// The one that was hit and its factions
            FormId mVictim = 0;
            std::vector<FormId> mVictimFactions;
            /// A weapon of the minor crime kind (a flag of its record): the blow is a minor crime
            bool mMinor = false;
            /// The victim is fighting already (0x8A6650 and the target at +0x428 of the victim, 0x898D6A to 0x898DC5): the
            /// blow is no crime, and the victim only thinks less of the player (fCrimeDispAttack)
            bool mFighting = false;
        };
        /// The player hit a character that is not tolerated and alive (the creator 0x8C0460 called at 0x898E26)
        Outcome attack(const Blow& blow, const std::vector<Witness>& watchers);

        /// What the hit routine (0x898D6A to 0x898E5B) makes of a blow of the player that nobody lets pass: a victim that
        /// fights already (its combat target at +0x428, or 0x8A6650) is wronged by nothing (the pair modifier only); a
        /// creature (the virtual at +0x21C) makes no attack crime and only counts when it has an owner, whose property
        /// it is (the steal creator 0x8BFA40); any other victim is a character and the attack crime is made.
        enum class BlowKind
        {
            Nothing,
            Attack,
            Property,
        };
        static BlowKind judgeBlow(bool victimFights, bool victimIsCreature, bool victimHasOwner);

        struct Killing
        {
            FormId mVictim = 0;
            std::vector<FormId> mVictimFactions;
            /// The death counts as a crime: false for a friend or ally of the player that does not mind friendly hits,
            /// and for a death that a script asked to count for nothing (0x89E1CB, the flag at 0xC4 of the victim)
            bool mCrimeOnDeath = true;
            /// The karma of a kill counts (0x89E30D: only a death that a script did not ask to count for nothing)
            bool mKarmaCounts = true;
            /// The victim is a character and not a creature
            bool mCharacter = true;
            /// The victim is evil (IsActorEvil), and the class of its karma (see Standing::karmaClass): 0 good, 1 neutral,
            /// 2 evil, 3 very good, 4 very evil
            bool mEvil = false;
            int mKarmaClass = 1;
        };
        /// The player killed a character (the crime 0x8C09E0 called at 0x8B0381 from the death 0x8B01C0, the karma and the
        /// murderer flag at 0x89E242 to 0x89E3FC)
        Outcome murder(const Killing& killing, const std::vector<Witness>& watchers);

        struct Theft
        {
            /// The one that was robbed and its factions; 0 when no character of the owner is near enough
            FormId mVictim = 0;
            std::vector<FormId> mVictimFactions;
            /// Whether the character notices the player and is alive
            bool mVictimWatching = false;
            /// The value of what was taken (0 for a lock that was picked)
            std::int64_t mValue = 0;
            /// Whether the owner is neither evil nor of an evil faction (the karma of the theft is lost)
            bool mOwnerNotEvil = true;
            /// The day of the month, for the warning that goes for a day
            int mDay = 1;
        };
        /// The player took something that is someone's (0x8BFA40), or opened a lock that is someone's
        Outcome steal(const Theft& theft);

        /// The player was caught picking a pocket (0x8C00E0)
        Outcome pickpocket(const Theft& theft);

        struct Alarm
        {
            /// The characters that notice the player and answer to the alarm (they are of the faction that sounds it, or
            /// share one with the one that does)
            std::vector<Witness> mResponders;
        };
        /// SendTrespassAlarm (0x8C0EC0): every character that answers fights the player; the first one marks a minor
        /// crime for its factions and the player counts one minor crime if anyone answered
        Outcome alarm(const Alarm& alarm);

        /// SendAssaultAlarm: a crime of attack against the victim that is a major one, and the victim fights
        Outcome assaultAlarm(FormId victim, const std::vector<FormId>& victimFactions, const std::vector<Witness>& watchers);

        // ---- trespass ----

        enum class TrespassStep : std::uint8_t
        {
            /// Nothing to do yet
            Wait,
            /// Says the line of a warning (the level is higher by one)
            Warn,
            /// The warnings are given: the alarm
            Alarm,
        };
        /// The package of a trespass for the watcher, which the player trespasses in front of: `seconds` have passed. The
        /// first step is at once, the next ones after the time between warnings.
        TrespassStep trespass(FormId watcher, double seconds, bool offLimits);
        /// The watcher's package ends (the player left, or the trespass was allowed)
        void endTrespass(FormId watcher);
        /// What GetTrespassWarningLevel tells for the watcher: -1 when it has no trespass package
        int trespassLevel(FormId watcher);
        /// Whether the watcher has a trespass package
        bool trespassing(FormId watcher);
        /// Whether any watcher has one: the player is trespassing (the query of the trespasser and not of a watcher)
        bool anyTrespass();
        /// Every watcher that has a trespass package, including the ones that a save brought back
        std::vector<FormId> trespassWatchers();

        // ---- the karma ----

        struct KarmaChange
        {
            /// The change that is made (the asked one limited so that the karma stays within its limits)
            int mApplied = 0;
            /// Whether the change is a major one (at least the threshold) and its direction, for the message
            bool mMajor = false;
        };
        /// What a change of karma comes to, with the karma now and the limits of the settings
        static KarmaChange karma(const LawSettings& settings, int current, int change);

        /// The victim of the crime whose line a character is saying, which IsActorAVictim tells (the executables keep it in
        /// the thread's state while the line runs, 0x8BFF30); 0 when no such line runs
        void setLineVictim(FormId victim);
        FormId lineVictim();

        /// The test that every taking goes by (0x5785E0): a thing may be taken by the character when it has no owner, when
        /// the character's form is the owner, or when the owner is a faction that the character is in
        static bool mayTake(FormId owner, FormId characterForm, const std::vector<FormId>& characterFactions);

        /// The line that the default dialogue list has for the kind of crime (the form of its topic, in the file of the game
        /// itself), 0 for none
        static FormId lineFor(CrimeKind kind, CrimeLine line);

    private:
        struct FactionRecord
        {
            bool mExpelled = false;
            bool mAttack = false;
            bool mMurder = false;
            int mMajor = 0;
            int mMinor = 0;
        };

        struct Crime
        {
            int mNumber = 0;
            CrimeKind mKind = CrimeKind::Attack;
            FormId mVictim = 0;
            FormId mCriminal = 0;
            std::int64_t mValue = 0;
            std::vector<FormId> mWitnesses;
            std::vector<FormId> mFactions;
            /// The clock when the crime was made
            double mTime = 0;
        };

        struct Trespass
        {
            int mLevel = 0;
            double mTimer = 0;
            bool mAlarmed = false;
        };

        /// Makes the factions of the watcher that track crime and have the victim in them count the player as an enemy,
        /// as the executables do for a watcher that cares (0x8B8360); true when any did. The factions are added to the
        /// crime's and to the enemies that the outcome lists.
        bool cares(const std::vector<FormId>& watcherFactions, const std::vector<FormId>& victimFactions,
            std::size_t crime, Outcome& outcome);
        std::size_t record(CrimeKind kind, FormId victim, std::int64_t value);
        /// The count of the factions of a victim that track crime (the minor or major one), with the infamy that their
        /// reputations take when it is asked for
        void factionCrime(const std::vector<FormId>& victimFactions, bool major, bool infamy, const LawSettings& settings,
            Outcome& outcome);
        void addWitness(std::size_t crime, FormId actor);
        /// The warnings and the attack of a theft or a pickpocket that the victim noticed
        void ladder(CrimeKind kind, FormId victim, bool warnOnly, int day, Outcome& outcome);
        /// Whether any of the factions tracks crime
        bool anyTracking(const std::vector<FormId>& factions);

        LawRecords& mRecords;
        ObjectState& mObjects;
        std::mutex mMutex;
        std::map<FormId, FactionRecord> mFactions;
        std::map<FormId, int> mActorMinor;
        int mPlayerMinor = 0;
        int mPlayerMajor = 0;
        std::array<std::int64_t, 5> mValues{};
        int mStealCount = 0;
        int mPickpocketCount = 0;
        /// The player was warned today (the flag and the day that the executables keep for stealing and for picking
        /// pockets together)
        bool mWarned = false;
        int mWarnedDay = 0;
        bool mMurderer = false;
        double mSoldStolen = 0;
        std::vector<Crime> mCrimes;
        int mNextCrime = 1;
        double mClock = 0;
        /// Forgets the crimes that `drop` picks and gives the factions that no crime holds any more (0x9722E0)
        std::vector<FormId> forget(const std::function<bool(const Crime&)>& drop);
        std::map<FormId, Trespass> mTrespass;
        FormId mLineVictim = 0;
    };

    /// The section of a saved game that holds the law
    constexpr std::uint32_t lawSectionVersion = 1;
    void writeLaw(Law& law, SaveWriter& out);
    void readLaw(Law& law, SaveReader& in, std::uint32_t version);

    /// What the commands that ask about the law need to ask of the world
    class LawWorld
    {
    public:
        virtual ~LawWorld() = default;

        /// The factions of the character, without their ranks: nothing for a form that is no character
        virtual std::vector<FormId> factionsOf(FormId actor) = 0;
        /// The form that is an owner for the character reference: its base
        virtual FormId ownerForm(FormId form) = 0;
        /// The cell that the reference is in, 0 when it is in none
        virtual FormId cellOf(FormId reference) = 0;
        /// Whether the base of the character is a guard
        virtual bool isGuard(FormId actor) = 0;
        /// How much the watcher notices the target: 0 when it does not
        virtual int detectionLevel(FormId watcher, FormId target) = 0;
        /// Whether the character runs the package of the alarm
        virtual bool alarmed(FormId actor) = 0;
        /// Whether anyone that is not fighting the character has it in plain sight (0x973460, IsActorDetected)
        virtual bool detected(FormId actor) = 0;
        /// Whether the character has noticed that the player trespasses (0x5A1900)
        virtual bool trespassingNow(FormId actor) = 0;
        /// The characters that notice the player and answer an alarm that the caller sounds (those of the caller's factions,
        /// or the character named)
        virtual std::vector<Witness> responders(FormId caller, FormId subject) = 0;
        /// The characters that can have seen what was done to the victim
        virtual std::vector<Witness> watchers(FormId victim) = 0;
        /// Carries out what the rule decided: the lines, the fights, the reputations, the karma
        virtual void carryOut(const Outcome& outcome, FormId victim) = 0;
    };

    /// Gives the commands about crime and the law to the interpreter: GetMinorCrimeCount, GetMajorCrimeCount, GetCrime,
    /// GetCrimeKnown, IsGuard, IsPCAMurderer, GetPCExpelled, SetPCExpelled, GetPCFactionAttack, SetPCFactionAttack,
    /// GetPCFactionMurder, SetPCFactionMurder, GetAlarmed, GetDetectionLevel, IsActorDetected, GetAmountSoldStolen,
    /// ModAmountSoldStolen, SendAssaultAlarm, SendTrespassAlarm, ClearFactionPlayerEnemyFlag,
    /// ClearActorsFactionsPlayerEnemyFlag, GetActorFactionPlayerEnemy, GetActorCrimePlayerEnemy,
    /// GetTrespassWarningLevel, IsTrespassing, IsInMyOwnedCell, IsCellOwner, IsActorAVictim and ClearOwnership. Those that the command
    /// table does not have are left out. (SetPCEnemyofFaction and GetPCEnemyofFaction are the commands of ObjectState,
    /// which holds the flag.)
    void addLawCommands(
        Interpreter& interpreter, Law& law, ObjectState& objects, LawRecords& records, LawWorld& world);
}

#endif

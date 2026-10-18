#ifndef OPENFALLOUT_SCRIPT_FALLOUTSCRIPTS_H
#define OPENFALLOUT_SCRIPT_FALLOUTSCRIPTS_H

#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <components/esm/formid.hpp>
#include <components/esm/refid.hpp>
#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/actortraits.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/animationcommands.hpp>
#include <components/falloutscript/calendar.hpp>
#include <components/falloutscript/chargen.hpp>
#include <components/falloutscript/combatcommands.hpp>
#include <components/falloutscript/combatstate.hpp>
#include <components/falloutscript/commandtable.hpp>
#include <components/falloutscript/conditions.hpp>
#include <components/falloutscript/damage.hpp>
#include <components/falloutscript/derivedstats.hpp>
#include <components/falloutscript/dialogue.hpp>
#include <components/falloutscript/enableparents.hpp>
#include <components/falloutscript/equipment.hpp>
#include <components/falloutscript/formlists.hpp>
#include <components/falloutscript/furniture.hpp>
#include <components/falloutscript/interpreter.hpp>
#include <components/falloutscript/itemuse.hpp>
#include <components/falloutscript/messagecommands.hpp>
#include <components/falloutscript/objectcommands.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/packagecommands.hpp>
#include <components/falloutscript/perks.hpp>
#include <components/falloutscript/pipboy.hpp>
#include <components/falloutscript/placementcommands.hpp>
#include <components/falloutscript/playercontrols.hpp>
#include <components/falloutscript/playerprogress.hpp>
#include <components/falloutscript/quests.hpp>
#include <components/falloutscript/referencestate.hpp>
#include <components/falloutscript/triggervolumes.hpp>
#include <components/falloutscript/vanillacommands.hpp>
#include <components/falloutscript/voicefiles.hpp>

#include <osg/Vec3f>

#include "../mwworld/ptr.hpp"

namespace ESM4
{
    struct AIPackage;
}

namespace OFRender
{
    class ObjectAnimation;
}

namespace OFWorld
{
    class CellStore;
    class ESMStore;
}

namespace OFScript
{
    /// Which commands the compiled scripts of the loaded content use: Fallout 3 when only its master is loaded, New
    /// Vegas otherwise (its table has all the commands of the other and the same names, and Tale of Two Wastelands runs
    /// on the engine of New Vegas).
    FalloutScript::Game chooseGame(const std::vector<std::string>& contentFiles);

    /// The form id of the reference of the player in the load order of the content files: PlayerRef is form 0x14 of the
    /// first plugin (the scripts the engine adds come first in the list and are not plugins). Without a plugin it is
    /// 0x14.
    FalloutScript::FormId playerReferenceOf(const std::vector<std::string>& contentFiles);

    /// The compiled scripts of Fallout 3 and New Vegas at work: the global variables, the quests with their stages and
    /// the scripts of the quests, the scripts of the objects in the loaded cells, and the virtual machine that runs
    /// them.
    class FalloutScripts final : private FalloutScript::Host,
                                 private FalloutScript::QuestRecords,
                                 private FalloutScript::ObjectWorld,
                                 private FalloutScript::ActorRecords,
                                 private FalloutScript::ItemRecords,
                                 private FalloutScript::ActorTraitRecords,
                                 private FalloutScript::DialogueRecords,
                                 private FalloutScript::ActorValueRecords,
                                 private FalloutScript::ReferenceRecords,
                                 private FalloutScript::FormListRecords,
                                 private FalloutScript::PackageWorld,
                                 private FalloutScript::PlacementWorld,
                                 private FalloutScript::MessageWorld,
                                 private FalloutScript::ControlsWorld,
                                 private FalloutScript::CombatWorld,
                                 private FalloutScript::AnimationWorld,
                                 private FalloutScript::PerkRecords,
                                 private FalloutScript::UseRecords
    {
    public:
        FalloutScripts(const OFWorld::ESMStore& store, FalloutScript::Game game,
            FalloutScript::FormId player = FalloutScript::playerReference);

        FalloutScripts(const FalloutScripts&) = delete;
        FalloutScripts& operator=(const FalloutScripts&) = delete;
        ~FalloutScripts();

        /// Starts over, as for a new game: global variables have the values of their records, quests are at their
        /// beginning. What was loaded of the world is forgotten.
        void reset();

        /// The content files of the game in the order they are loaded, which IsDLCInstalled looks at
        void setContentFiles(std::vector<std::string> files);

        /// The quest of character creation, which the games start when the player picks New in the main menu: the
        /// executables name it in their ini file (CG00 in Fallout 3, VCG00 in New Vegas), and here it is found by its
        /// editor id. 0 when the content has none (a list of Morrowind format files). Call it after reset().
        FalloutScript::FormId newGameQuest() const;
        /// Starts a new game as the games do: the quest of character creation runs, and its script gets the first
        /// run of its menu mode block (the main menu is a menu), which sets its first stage; the quest takes the
        /// player to where the game starts from there. Does nothing without such a quest.
        void startNewGame();

        /// Lets a frame pass: what scripts asked of the world from another thread is done, and the scripts of the
        /// quests that run get their turn, and so do the scripts of the objects in the loaded cells. A game that is
        /// paused (the menus of the game) does the first only: the time of the game does not pass.
        void update(float frametime, bool paused = false);

        /// A cell is loaded: the objects in it that have a script get it (and a script that is already theirs, with
        /// its variables), and run their OnLoad blocks in the next frame. The objects that scripts have enabled or
        /// disabled are so in the cell, too.
        void cellLoaded(OFWorld::CellStore& cell);
        /// A cell is unloaded: its objects stop running their scripts, which they keep
        void cellUnloaded(OFWorld::CellStore& cell);
        /// An actor activates an object. Runs the OnActivate blocks of the script of the object, with the actor as the
        /// action reference, and tells whether it has one. An object whose script has one does not do what it does by
        /// default: its script asks for that with Activate.
        bool activated(const OFWorld::Ptr& object, const OFWorld::Ptr& actor);

        /// An actor picks an item up from the world (the default activation of an item): the item goes to its
        /// inventory, the reference is gone from its cell (disabled, as the games take a reference out of the world),
        /// and the script of the item runs its OnAdd block. False for an object that is no item, or when the actor
        /// has no number for scripts.
        bool pickUp(const OFWorld::Ptr& item, const OFWorld::Ptr& actor);

        /// What characters carry, hold and wear. The inventory of the player is what scripts and pick-ups gave it.
        FalloutScript::ActorState& actors() { return mActors; }
        FalloutScript::Equipment& equipment() { return mEquipment; }
        FalloutScript::ActorValues& actorValues() { return mActorValues; }
        /// The reference of the player
        FalloutScript::FormId playerReference() const { return mPlayer; }

        /// The number of a record of the first master from its local number (the master is the first plugin, whose
        /// place among the content files the player reference carries in its high byte)
        FalloutScript::FormId masterForm(FalloutScript::FormId local) const { return (mPlayer & 0xFF000000u) | local; }

        /// Who fights whom and how the characters stand to each other
        FalloutScript::CombatState& combat() { return mCombat; }
        /// What the player has earned and the perks of the characters
        FalloutScript::PlayerProgress& progress() { return mProgress; }
        FalloutScript::PerkEngine& perkEngine() { return mPerks; }
        FalloutScript::ItemUse& itemUse() { return mItemUse; }

        /// The character uses an item that is used up (an aid item, a book); a weapon or armor is for equipment().
        /// The player is told what it did. Any thread.
        FalloutScript::ItemUse::Result useItem(FalloutScript::FormId actor, FalloutScript::FormId item);
        /// The levels the player reached whose points have been handed out and that the interface has not shown yet
        std::vector<FalloutScript::PlayerProgress::LevelUp> takeLevelUps();
        /// The value of a game setting (GMST) by its editor id, none when the files have no such setting
        std::optional<double> gameSetting(std::string_view name);
        /// The text of a game setting, empty when the files have none
        std::string gameSettingText(std::string_view name);

        /// An item of an inventory, as the Pip-Boy lists it
        struct PipboyItem
        {
            FalloutScript::FormId mId = 0;
            std::string mName;
            int mCount = 0;
            double mWeight = 0;
            int mValue = 0;
            FalloutScript::Pipboy::ItemCategory mCategory = FalloutScript::Pipboy::ItemCategory::Misc;
            bool mEquipped = false;
            /// A weapon: the damage of a blow of the record; armor: its rating and damage threshold
            double mDamage = 0;
            double mRating = 0;
            double mThreshold = 0;
        };
        /// What a holder carries, the items the host knows by name, in the order the holder got them
        std::vector<PipboyItem> pipboyItems(FalloutScript::FormId holder);
        /// What the player's character is and has, for the status tab
        struct PipboyStats
        {
            int mLevel = 1;
            int mExperience = 0;
            int mLevelStart = 0;
            int mNextLevel = 0;
            int mKarma = 0;
            std::string mKarmaTitle;
            double mHealth = 0;
            double mMaxHealth = 0;
            double mActionPoints = 0;
            double mMaxActionPoints = 0;
            double mWeight = 0;
            double mMaxWeight = 0;
            int mCaps = 0;
            int mSkillPoints = 0;
            int mPerkPoints = 0;
            double mArmorRating = 0;
            double mThreshold = 0;
        };
        PipboyStats pipboyStats();
        /// The quests the player is following with what is left to do, for the data tab
        struct PipboyObjective
        {
            std::string mText;
            bool mCompleted = false;
        };
        struct PipboyQuest
        {
            FalloutScript::FormId mId = 0;
            std::string mName;
            bool mCompleted = false;
            std::vector<PipboyObjective> mObjectives;
        };
        std::vector<PipboyQuest> pipboyQuests();
        /// The perks of the player with their ranks, and those it can choose from when it has a perk to spend
        struct PipboyPerk
        {
            FalloutScript::FormId mId = 0;
            std::string mName;
            std::string mDescription;
            int mRank = 0;
            int mRanks = 1;
            /// The perks of the level-up list that cannot be chosen yet are shown dimmed
            bool mAvailable = true;
        };
        std::vector<PipboyPerk> pipboyPerks();
        std::vector<PipboyPerk> pipboyOfferedPerks();
        /// Spends skill points on a skill (the skill stays at most 100); the points spent, 0 when none could be
        int spendSkillPoints(int skill, int points);
        /// Takes skill points back from a skill (the screen of the level up does when the player takes back what it
        /// gave on that screen); the points given back, 0 when none could be
        int refundSkillPoints(int skill, int points);
        /// Spends the perk point on a perk the player is offered; false when it is not
        bool choosePerk(FalloutScript::FormId perk);
        /// The player does with the item what its kind asks (wear a weapon, armor or ammunition, or take it off again,
        /// use up an aid item or read a book); the words that tell what happened, empty when nothing did
        std::string useItemFromList(FalloutScript::FormId item);
        /// The player puts items on the floor in front of it
        int dropItem(FalloutScript::FormId item, int count);
        /// Items go from one holder to another (a container); the count that moved
        int transferItems(FalloutScript::FormId from, FalloutScript::FormId to, FalloutScript::FormId item, int count);
        /// The player opens or closes a container, for the scripts that hear of it (OnOpen, OnClose)
        void containerOpened(FalloutScript::FormId container, bool opened);
        /// The player activates a container or a character that is dead: its things can be taken. False when the
        /// object has nothing to open (the player is told when it is locked, and that opens nothing either).
        bool openContainer(const OFWorld::Ptr& object, const OFWorld::Ptr& actor);
        /// What the player has open to take things from: the holder, its name, and a number that is another for each
        /// time something is opened. Nothing when nothing is open.
        struct Loot
        {
            FalloutScript::FormId mHolder = 0;
            std::string mName;
            int mSerial = 0;
        };
        std::optional<Loot> loot();
        /// The player is done with what it had open
        void closeLoot();

        /// The 3D of an object that holds animations of its own is built (a door, a switch): an object that rests open
        /// starts so
        void objectInserted(const OFWorld::Ptr& object);
        /// Whether the object has its collision when it is put in the scene: a door that is open has none, unless its
        /// model has no way to show it open
        bool hasCollision(const OFWorld::Ptr& object);
        /// A door that is no teleport door is activated: it opens, and closes when it is open. The activation of a
        /// door that is a teleport door is no concern of this.
        void activateDoor(const OFWorld::Ptr& door);

        /// What a blow did
        struct StrikeResult
        {
            bool mHit = false; // false when the blow could not be struck (no victim, victim dead, a blow at itself)
            double mDamage = 0;
            bool mCritical = false;
            bool mKilled = false; // the victim is dead
            bool mDowned = false; // the victim is essential and fell unconscious
        };
        /// The attacker strikes the victim with the weapon (0 for the weapon the attacker holds, or its hands): the
        /// blow is worked out from the weapon, the skill and strength of the attacker and the armor of the victim, the
        /// health of the victim goes down, and the victim dies, or when it is essential falls unconscious. The victim
        /// fights back if it was at peace. The OnHit, OnHitWith and OnDeath blocks of the victim run in the next
        /// frame. `limb` is the part of the body that was hit, -1 when not known; `roll` (0 to 100) decides whether the
        /// blow is critical, a negative one is drawn at random. Any thread.
        StrikeResult strike(FalloutScript::FormId attacker, FalloutScript::FormId victim,
            FalloutScript::FormId weapon = 0, int limb = -1, double roll = -1);

        /// What the weapon that a character holds allows
        struct WeaponStats
        {
            FalloutScript::FormId mWeapon = 0; // 0 for the hands
            bool mMelee = true; // hands and melee weapons
            bool mAutomatic = false;
            double mRange = 0; // how far a blow or a shot reaches
            double mInterval = 0.5; // the least seconds between two blows or shots
            int mAmmoUse = 0; // rounds one attack uses, 0 for hands and melee weapons
            FalloutScript::FormId mAmmo = 0; // the ammunition it uses, 0 for none
            int mRounds = 0; // how many rounds the character has
        };
        /// Any thread.
        WeaponStats weaponStats(FalloutScript::FormId attacker);

        /// What an attack of the player or another character came to
        struct FireResult
        {
            bool mFired = false; // false while the weapon is not ready again and when there is no ammunition
            bool mOutOfAmmo = false;
            bool mHit = false; // something was struck
            StrikeResult mStrike; // what a character that was struck lost
        };
        /// The attacker attacks with the weapon it holds, at the target (empty when it struck nothing): a round is used
        /// up, the weapon is not ready again for a time, and the target is struck as `strike` does when it is a
        /// character or creature, and else only gets its OnHit and OnHitWith blocks run (a target that is an object has
        /// no health). Main thread.
        /// `chance` (0 to 1) is how likely the attack is to hit what it aims at: the player aims with the crosshair
        /// and always does, a character is not so sure of its aim.
        FireResult fire(FalloutScript::FormId attacker, const OFWorld::Ptr& target, int limb = -1, double chance = 1.0);

        /// What a character in a fight does about it, for the AI of the actors (main thread)
        struct FightStance
        {
            OFWorld::Ptr mTarget; // empty when the actor is at peace, or the fight ended (it is stopped then)
            bool mFlee = false; // the actor runs away: its health is below what its confidence allows
            bool mMelee = true; // the weapon it holds is a melee weapon or its hands
            double mEngage = 0; // how far from the target it attacks from
        };
        /// What the actor does in the fight it is in: the target is the one the actor fights, or empty when it fights
        /// no one, cannot fight (down, restrained, dead) or the target is gone (dead, down, disabled or not in a cell
        /// that is loaded), and the fight is stopped then.
        FightStance fightStance(const OFWorld::Ptr& actor);
        /// A character that is about to fight takes the weapon that does the most damage of those it can use (a
        /// character with no weapon and no melee weapon in its inventory fights with its hands), and draws it
        void prepareFight(const OFWorld::Ptr& actor);
        /// Whether the character, which is at peace, goes for the player when it sees it: from its aggression and how
        /// its factions stand to the player. `radius` is how near the player has to be.
        bool attacksPlayerOnSight(const OFWorld::Ptr& actor, float& radius);
        /// The character goes for the player
        void fightPlayer(const OFWorld::Ptr& actor);
        /// The character attacks the target with the weapon it holds, as far as its skill allows it to hit. A
        /// weapon that has no ammunition left is put away and the character takes another.
        FireResult attack(const OFWorld::Ptr& attacker, const OFWorld::Ptr& target);
        /// The character does not fight any more (it ran far enough from its target, or lost it)
        void endFight(const OFWorld::Ptr& actor);
        /// Whether the character may go for the player on sight at all: its aggression is more than none
        bool isAggressive(const OFWorld::Ptr& actor);
        /// Whether the character has a target: it needs a mind to fight whatever its aggression is (a blow or a script
        /// can start a fight with a character that never starts one)
        bool fighting(const OFWorld::Ptr& actor);

        /// Whether a script killed the character or creature (and none brought it back). Nothing else in the game kills
        /// yet.
        bool isDead(const OFWorld::Ptr& actor) const;

        /// The actor sits on, lies on or leans against the furniture, which IsCurrentFurnitureRef and
        /// IsCurrentFurnitureObj tell, until it gets up or is moved away from it. With `place` the actor is put on the
        /// entry of the marker of the furniture that it uses, facing as the entry says (the player who activates a
        /// chair goes there at once, as in the games; there is no animation yet), or at the furniture when the model
        /// has no marker; a character that walked there is at it already. Does nothing for a furniture or an actor
        /// that scripts have no number for. False when the model has entries and none is free (nothing is done then),
        /// true otherwise.
        bool useFurniture(const OFWorld::Ptr& actor, const OFWorld::Ptr& furniture, bool place);
        /// What the default activation of a furniture does: a character, creature or the player that uses furniture
        /// already gets up (whichever furniture it activates), another one uses it as `useFurniture` does with `place`
        void activateFurniture(const OFWorld::Ptr& actor, const OFWorld::Ptr& furniture);
        /// The actor gets up from the furniture it uses (it is where it was: the games play an animation there)
        void leaveFurniture(const OFWorld::Ptr& actor);
        /// Where the actor would stand to use the furniture: the entry of its marker that the record enables, that no
        /// one else uses and that is the nearest to the actor, and the heading it faces there. False when there is
        /// none (the model has no marker).
        bool furnitureSeat(
            const OFWorld::Ptr& actor, const OFWorld::Ptr& furniture, osg::Vec3f& position, float& heading);
        /// Whether the actor uses some furniture now (a character does not leave it for another package that has
        /// become eligible, as the games keep a seated character in its chair until the package that sat it ends)
        bool usesFurniture(const OFWorld::Ptr& actor) const;

        /// The AI packages that scripts gave the character, the last given first (those that no content file has a
        /// record of are left out)
        std::vector<const ESM4::AIPackage*> scriptPackages(const OFWorld::Ptr& actor) const;
        /// Whether the conditions of the package hold for the character. A package that has none holds.
        bool packageConditionsHold(const ESM4::AIPackage& package, const OFWorld::Ptr& actor);

        /// What happens to an AI package of a character that scripts can see
        enum class PackageEvent
        {
            Start, // the character begins to follow it
            Done, // the character has done what it says (a Travel package: it got there)
            Change, // another package takes its place before it is done
        };
        /// A character's package begins, is done or is changed: the script of the package for the event runs on the
        /// character, and so do the blocks of the script of the character for the event (OnPackageStart, OnPackageDone,
        /// OnPackageChange) that name the package, or none. Runs scripts, so not in the middle of what the characters
        /// are doing in their frame.
        void packageEvent(const OFWorld::Ptr& actor, const ESM4::AIPackage& package, PackageEvent event);

        /// What the player is shown of the conversation that is open, with the name of the character who talks
        struct Conversation
        {
            bool mActive = false;
            FalloutScript::DialogueView mView;
            std::string mSpeakerName;
            /// The seconds that each line of the response takes to read, and their sum, which is how long the response
            /// stays before the next thing happens when there is no voice to wait for
            std::vector<float> mLineDurations;
            float mDuration = 0;
        };

        /// How the voice of a line of the conversation is doing
        enum class VoiceState
        {
            None, // no voice is asked for, there is none for the line, or the sound could not be played
            Pending, // asked for, the frame that plays it has not come
            Playing,
            Done, // played to the end, or stopped
        };

        /// Whether the loaded content has dialogue (topics of Fallout 3 or New Vegas)
        bool hasDialogue() const;
        /// Whether the loaded content has the weapons and characters of Fallout 3 or New Vegas, which are what the
        /// combat here is for (the content of Morrowind has none of them)
        bool hasCombat() const;
        /// The character talks to the player. The greeting is said (a topic given takes its place) and the conversation
        /// goes on from there, which the interface shows. False, with no conversation open, when the character has
        /// nothing to say and the player nothing to pick. A conversation that is open stays.
        bool startConversation(const OFWorld::Ptr& speaker, FalloutScript::FormId topic = 0);
        /// startConversation and the interface that shows it. For the thread of the frame.
        bool talkTo(const OFWorld::Ptr& speaker, FalloutScript::FormId topic = 0);
        /// What a character with a Dialogue package does once it is where it talks: a Say To package for the player
        /// says one response of its topic, a Conversation package for the player begins a conversation (the interface
        /// opens, which is for the thread of the frame), and a package for another character makes the exchange of the
        /// two (follow-ups and all). `listener` is the character it talks to, empty for the player. False when the
        /// character is to try again later, because the player is busy with a menu or another conversation (`busy`, if
        /// given, says with what); a character that has nothing to say is done all the same.
        bool speakPackage(const OFWorld::Ptr& speaker, const ESM4::AIPackage& package, const OFWorld::Ptr& listener,
            std::string* busy = nullptr);
        /// Whether the character is still saying lines that it was told to say outside of a conversation
        bool isSaying(const OFWorld::Ptr& speaker) const;
        /// The conversation as it is now (nothing active when none is open)
        Conversation conversation() const;
        /// The speaker says the line of the response that is said, with the voice of the data. False when the line has
        /// no voice file. The sound starts with the next frame.
        bool sayConversationLine(std::size_t index);
        VoiceState conversationVoice() const { return static_cast<VoiceState>(mConversationVoice.load()); }
        /// The voice of the conversation stops
        void stopConversationVoice();
        /// The player picks the topic (one of the options the conversation offers); the response is said
        void chooseOption(FalloutScript::FormId topic);
        /// The response that is said is over: its end script runs and the options are worked out
        void finishResponse();
        /// The conversation ends, also in the middle of a response
        void endConversation();

        /// The message box that is on the screen, as the player sees it: the text (the name of the message and its
        /// words), the buttons the player can pick from, and a number that is another for each box that opens
        struct OpenMessage
        {
            std::uint32_t mSerial = 0;
            std::string mText;
            std::vector<std::string> mButtons;
            /// The message record, as core.getFormId writes a form
            std::string mForm;
        };
        /// The box that is open, if any (any thread)
        std::optional<OpenMessage> openMessage() const;
        /// Picks a button of the box that is open, as a click on it would (counted from 0 in the order the box lists
        /// them). False when no box is open or it has no such button. The box closes in the next frame, and the script
        /// that showed it reads the button then.
        bool pressMessageButton(std::size_t button);

        /// The screens of the making of the character: name, sex and race, SPECIAL, tagged skills and traits. The
        /// commands that script call open them, and the interface (core.chargen) shows the one that is open and tells
        /// when the player is done with it.
        FalloutScript::Chargen& chargen() { return mChargen; }
        /// The player is done with the screen: what it picked becomes the character (the attributes and skills the
        /// player has, the traits as perks) and the next screen of the menu opens
        void finishChargenPage();
        /// The name and the description that the data has for an actor value (an attribute or a skill) by its editor
        /// id; the name given when it has none
        struct ActorValueText
        {
            std::string mName;
            std::string mDescription;
        };
        ActorValueText actorValueText(std::string_view editorId, std::string_view fallback) const;
        /// The races the player can be (playable and not children), by their form ids
        struct ChargenRace
        {
            FalloutScript::FormId mId = 0;
            std::string mName;
        };
        std::vector<ChargenRace> playableRaces() const;
        /// The traits of the data (New Vegas), by name
        struct ChargenTrait
        {
            FalloutScript::FormId mId = 0;
            std::string mName;
            std::string mDescription;
        };
        std::vector<ChargenTrait> chargenTraits() const;
        /// What the player is now: the sex and race the player picked or else those of its record
        bool playerTraits(FalloutScript::ActorTraits& traits);

        FalloutScript::QuestManager& quests() { return mQuests; }
        const FalloutScript::QuestManager& quests() const { return mQuests; }
        FalloutScript::ObjectScripts& objects() { return mObjects; }
        FalloutScript::Interpreter& interpreter() { return mInterpreter; }
        FalloutScript::ConditionEvaluator& conditions() { return mConditions; }

        /// The quest of that editor id or form id (0x followed by hex digits), 0 for none
        FalloutScript::FormId findQuest(std::string_view name) const;
        /// The global variable of that editor id or form id, 0 for none
        FalloutScript::FormId findGlobal(std::string_view name) const;
        /// The editor id of a quest or global variable, empty for a form that is not one
        std::string editorId(FalloutScript::FormId id) const;

        /// The value of a global variable, false for a form that is not one
        bool getGlobal(FalloutScript::FormId global, double& value) override;
        bool setGlobal(FalloutScript::FormId global, double value) override;

    private:
        FalloutScript::Instance* findInstance(FalloutScript::FormId owner) override;
        void log(std::string_view message) override;
        FalloutScript::FormId player() override { return mPlayer; }

        const ESM4::Script* findScript(FalloutScript::FormId id) const override;
        void forEachQuest(const std::function<void(const ESM4::Quest&)>& function) const override;

        bool isDisabled(FalloutScript::FormId reference) override;
        void setDisabled(FalloutScript::FormId reference, bool disabled) override;
        bool distance(FalloutScript::FormId first, FalloutScript::FormId second, double& result) override;
        FalloutScript::FormId cellOf(FalloutScript::FormId reference) override;
        bool inCell(FalloutScript::FormId reference, FalloutScript::FormId cell) override;
        void activate(FalloutScript::FormId target, FalloutScript::FormId activator, bool runOnActivate) override;

        void addScriptPackage(FalloutScript::FormId actor, FalloutScript::FormId package) override;
        void removeScriptPackage(FalloutScript::FormId actor) override;
        void evaluatePackage(FalloutScript::FormId actor) override;
        void resetAI(FalloutScript::FormId actor) override;
        FalloutScript::FormId currentPackage(FalloutScript::FormId actor) override;

        bool placement(FalloutScript::FormId reference, FalloutScript::Placement& result) override;
        void moveTo(FalloutScript::FormId reference, FalloutScript::FormId target,
            const std::array<double, 3>& offset) override;
        void setPosition(FalloutScript::FormId reference, int axis, double value) override;
        void setAngle(FalloutScript::FormId reference, int axis, double radians) override;

        void showMessage(
            FalloutScript::FormId message, const std::vector<double>& values, FalloutScript::FormId owner) override;
        int takeButtonPressed(FalloutScript::FormId owner) override;
        bool menuOpen(int menu) override;

        void setPlayerControl(FalloutScript::PlayerControl control, bool enabled) override;
        /// Whether a MenuMode block runs now: one that names a menu this game has (the message box, the conversation,
        /// the screens of making a character) runs while that menu is open; one that names nothing, or a menu that
        /// the game does not have by that number, runs while any menu is
        bool menuBlockRuns(const FalloutScript::Block& block);

        bool hasGroup(FalloutScript::FormId reference, std::string_view group) override;
        bool playGroup(FalloutScript::FormId reference, std::string_view group, bool restart) override;
        bool isGroupPlaying(FalloutScript::FormId reference, std::string_view group) override;
        /// The animation of the object a reference stands for, when it is in the scene and is no character
        OFRender::ObjectAnimation* objectAnimationOf(FalloutScript::FormId reference);
        /// Gives the collision back to the doors that are closed again
        void updateDoors();
        /// A door that its record places open rests open, unless something set the state of the door before
        void settleDoor(const OFWorld::Ptr& door, FalloutScript::FormId reference);

        bool traitsOf(FalloutScript::FormId reference, FalloutScript::ActorTraits& traits) override;
        /// What the record of the reference says it is, which is all that anyone but the player has to go by
        bool traitsOfRecord(FalloutScript::FormId reference, FalloutScript::ActorTraits& traits);
        /// A screen of the making of the character begins: the attributes it starts from are those the player has
        void beginChargenPage();
        /// The skills the player has, from the attributes and the tags that the making of the character gave
        void applyChargenSkills();

        /// The character says a response of the topic outside of a conversation: what the response does is done, and
        /// the lines are told to the player: the voice of the data and, when subtitles are on, the text
        /// (the script can force the subtitles on whatever the setting says). With `followUps` the response is
        /// followed by its follow-ups, as in a conversation (StartConversation between two characters).
        bool sayTopic(FalloutScript::FormId reference, FalloutScript::FormId topic, FalloutScript::FormId listener = 0,
            bool forceSubtitles = false, bool followUps = false);
        /// The character begins an exchange with another character: the topic is said (the greeting when none is
        /// named) and the other answers, as in a conversation with the player
        void converse(FalloutScript::FormId speaker, FalloutScript::FormId target, FalloutScript::FormId topic);
        /// The topic of a package: none (0) is the greeting, as it is with the player
        FalloutScript::FormId packageTopic(FalloutScript::FormId topic);
        /// The voice file of the line that the speaker says, empty when the data has none
        std::string voiceFile(FalloutScript::FormId speaker, const FalloutScript::DialogueLine& line);
        const FalloutScript::VoiceIndex& voices();
        static float lineSeconds(const std::string& text);
        /// A line said to the player outside of a conversation, one after the other for a character
        struct SaidLine
        {
            std::string mVoice;
            std::string mText;
            float mSeconds = 0;
            /// The last line of what one Say or SayTo said: the script of the speaker is told when it is over
            bool mEnd = false;
            /// The topic that was said, which the script of the speaker is told of (Begin SayToDone Topic)
            FalloutScript::FormId mTopic = 0;
            /// The response that this is the last line of: its end script runs, and the topics it adds are given, when
            /// the line is over (and before the script of the speaker is told that it has said what it was told to)
            FalloutScript::FormId mEndInfo = 0;
            /// The record of the speaker, kept with the line: a speaker that was made while the game runs and has left
            /// the loaded cells has no reference to tell it by when the response ends
            FalloutScript::FormId mSpeakerBase = 0;
            /// The character that says the line when it is not the one the lines were given to (0 for that one): in an
            /// exchange of two characters they take turns
            FalloutScript::FormId mSpeaker = 0;
            /// The script asked for the text to be shown even with subtitles off
            bool mForceSubtitles = false;
        };
        struct Saying
        {
            FalloutScript::FormId mReference = 0;
            std::vector<SaidLine> mLines;
            std::size_t mNext = 0;
            float mWait = 0;
            bool mStarted = false;
        };
        void startSaying(FalloutScript::FormId reference, std::vector<SaidLine> lines);
        void updateVoices(float frametime);

        void forEachTopic(const std::function<void(const ESM4::Dialogue&)>& function) const override;
        void forEachInfo(const std::function<void(const ESM4::DialogInfo&)>& function) const override;

        bool initialItems(
            FalloutScript::FormId holder, std::vector<std::pair<FalloutScript::FormId, int>>& items) override;
        bool initialFactions(
            FalloutScript::FormId actor, std::vector<std::pair<FalloutScript::FormId, int>>& factions) override;
        bool itemInfo(FalloutScript::FormId item, FalloutScript::ItemInfo& info) override;
        bool startingArmor(FalloutScript::FormId actor, std::vector<FalloutScript::FormId>& armor) override;
        /// The script of an item record (0 for none)
        FalloutScript::FormId itemScript(FalloutScript::FormId item);
        /// Runs the OnAdd, OnEquip and OnUnequip blocks of the scripts of items for what happened to the inventories
        /// since the last frame. The script of an item runs for the item as a form (what GetContainer tells is the
        /// holder it was last added to or worn by): the games keep an instance for each item, which this does not.
        void raiseItemEvents();
        bool detects(FalloutScript::FormId viewer, FalloutScript::FormId target) override;
        bool essentialByRecord(FalloutScript::FormId actor) override;
        std::optional<FalloutScript::CombatState::Relation> recordedRelation(
            FalloutScript::FormId first, FalloutScript::FormId second) override;
        /// What a weapon does with a blow, for the attacker that holds it; the hands for a weapon that is none
        FalloutScript::WeaponDamage describeWeapon(FalloutScript::FormId weapon, FalloutScript::FormId attacker);
        const FalloutScript::DamageSettings& damageSettings();
        /// Runs the OnHit, OnHitWith, OnDeath and OnCombatEnd blocks for what happened in the fights since the last
        /// frame, wakes the essential characters that were down long enough, and lets the time of the fights pass
        void raiseCombatEvents(float frametime);
        /// A character whose health is gone: an essential one is put out of its fights and lies unconscious until it
        /// gets up (true), any other one dies (false, nothing is done). Any thread.
        bool fallsInsteadOfDying(FalloutScript::FormId victim);
        bool initialValues(FalloutScript::FormId actor, std::vector<std::pair<int, double>>& values) override;
        bool levelOf(FalloutScript::FormId actor, int playerLevel, int& level) override;
        std::optional<double> derivedValue(
            FalloutScript::FormId actor, int value, const std::function<double(int)>& permanent) override;
        /// The settings of the files for the actor values that are worked out, read when the first is asked for
        const FalloutScript::DerivedStatSettings& derivedSettings();
        bool initialLock(FalloutScript::FormId reference, bool& locked, int& level) override;
        bool formListEntries(FalloutScript::FormId list, std::vector<FalloutScript::FormId>& entries) override;
        bool perk(FalloutScript::FormId perk, FalloutScript::PerkInfo& info) override;
        std::vector<FalloutScript::FormId> perks() override;
        bool abilityEffects(FalloutScript::FormId ability, std::vector<FalloutScript::AbilityEffect>& effects) override;
        bool consumable(FalloutScript::FormId item, FalloutScript::Consumable& result) override;
        bool book(FalloutScript::FormId item, FalloutScript::BookInfo& result) override;
        /// Brings the rules of levelling up from the settings of the files, once they can be read
        void ensureLevellingRules();
        /// Hands out the points of the levels the player reached, brings the perks up to date and lets the effects of
        /// the items that last go on
        void updateProgress(float frametime);
        /// The experience of a kill: the player killed the character (or did enough of the damage that someone else
        /// killed it with)
        void awardKillExperience(FalloutScript::FormId victim, bool byPlayer);
        /// Experience is rewarded to the player; the perks that change what is gained apply
        void rewardExperience(int amount);
        bool baseFormOf(FalloutScript::FormId reference, FalloutScript::FormId& base) override;
        FalloutScript::FormId worldspaceOf(FalloutScript::FormId reference) override;
        FalloutScript::FormId linkedRef(FalloutScript::FormId reference, FalloutScript::FormId keyword) override;
        FalloutScript::FormId placeAtMe(FalloutScript::FormId reference, FalloutScript::FormId base, int count,
            double distance, int direction) override;
        double gameHour() override;
        double gameDaysPassed() override;
        FalloutScript::FormId currentFurniture(FalloutScript::FormId actor) override;
        int currentFurnitureKind(FalloutScript::FormId actor) override;

        /// The reference as scripts name it, 0 for an object that has no number a script can use: a reference of a
        /// content file, or an object that PlaceAtMe made
        FalloutScript::FormId referenceOf(const OFWorld::Ptr& ptr) const;
        /// The id the world knows the reference by: the same number for those of the content files, and the one that
        /// the world gave an object that PlaceAtMe made
        ESM::FormId worldIdOf(FalloutScript::FormId reference) const;
        /// The script an object runs: that of its record, or of the template that its record takes its script from
        FalloutScript::FormId scriptOf(const OFWorld::Ptr& ptr, FalloutScript::FormId reference);

        /// The record that the reference is of, by the records of the content files; a reference that they do not have
        /// is looked for among the objects of the loaded cells, which only the thread of the frame may do. False when
        /// it is not known (yet); the base is empty for a reference that has none.
        bool baseOf(FalloutScript::FormId reference, ESM::FormId& base);
        /// The record that the conditions about a reference are about: that of the character a talking activator speaks
        /// for (SetTalkingActivatorActor), and the record of the reference otherwise
        bool conditionBaseOf(FalloutScript::FormId reference, ESM::FormId& base);
        FalloutScript::FormId talkingActorOf(FalloutScript::FormId activator);
        /// The object of the reference when it is in a cell that is loaded, else an empty one
        OFWorld::Ptr findReference(FalloutScript::FormId reference);
        void runPending();
        /// Objects that were moved to cells that are not loaded stop running their scripts
        void dropObjectsThatLeft();
        /// Lets a pass of the trigger volumes go by: the actors in each run the OnTrigger blocks of its script
        void updateTriggers();
        /// An actor that has moved away from its furniture does not use it any more
        void updateFurniture();
        std::vector<FalloutScript::FurnitureEntry> furnitureEntries(const OFWorld::Ptr& furniture);
        std::optional<FalloutScript::FurnitureSeat> chooseSeat(
            FalloutScript::FormId user, const OFWorld::Ptr& furniture, const osg::Vec3f& from);
        /// The reference of the actor as scripts name it (the player has the one of the player)
        FalloutScript::FormId scriptReferenceOf(const OFWorld::Ptr& actor) const;
        /// Whether the object of a cell that was loaded is a trigger, and its volume if it is
        void registerTrigger(const OFWorld::Ptr& ptr, FalloutScript::FormId reference);

        /// A place in the world: the cell by its id, and the position and angles in it (radians)
        struct Place
        {
            ESM::RefId mCell;
            osg::Vec3f mPosition;
            osg::Vec3f mAngle;
        };
        /// A change of place that a script asked for. Only the parts it names are made when the world gets to it, so
        /// what the world does with the object in the meantime (a character walks on, turns) is not undone by a script
        /// that sets its heading.
        struct Move
        {
            enum Part : unsigned
            {
                PositionX = 1,
                PositionY = 2,
                PositionZ = 4,
                AngleX = 8,
                AngleY = 16,
                AngleZ = 32,
                Cell = 64,
                All = 127
            };
            Place mWhere;
            unsigned mParts = 0;

            /// The place with the parts of the move made
            Place applyTo(Place place) const
            {
                if ((mParts & Cell) != 0)
                    place.mCell = mWhere.mCell;
                for (int axis = 0; axis < 3; ++axis)
                {
                    if ((mParts & (PositionX << axis)) != 0)
                        place.mPosition[axis] = mWhere.mPosition[axis];
                    if ((mParts & (AngleX << axis)) != 0)
                        place.mAngle[axis] = mWhere.mAngle[axis];
                }
                return place;
            }
            /// A move that comes after this one: the parts of both, the later one where both name a part
            void add(const Move& later)
            {
                mWhere = later.applyTo(mWhere);
                mParts |= later.mParts;
            }
        };
        /// What PlaceAtMe asked for and the world has not made yet
        struct MadeObject
        {
            FalloutScript::FormId mBase = 0;
            FalloutScript::FormId mNear = 0; // the reference whose place it is made next to
            int mDirection = 0;
            float mDistance = 0;
            Place mWhere; // where that reference was, which is where scripts see the new object until it is made
        };
        /// Makes an object that PlaceAtMe asked for, at the start of the next frame
        void makeObject(FalloutScript::FormId id, const MadeObject& request);
        static void queueMove(
            std::map<FalloutScript::FormId, Move>& moves, FalloutScript::FormId reference, const Move& move);
        /// Where the reference is by the world: where it is in a loaded cell, else where a script put it while its cell
        /// was not loaded, else where its record puts it. False for a reference nobody knows.
        bool currentPlace(FalloutScript::FormId reference, Place& result);
        /// The cell that the place is in: the one it names, or for an exterior cell the one that the position is in.
        /// Null for a cell that is not known.
        OFWorld::CellStore* cellAt(const Place& place);
        /// Whether the two places are in the same cell, or in exterior cells of one worldspace (where positions are
        /// comparable)
        bool inOneSpace(const Place& a, const Place& b);
        /// Where the reference is: the place of the world with the moves that scripts have asked for and the world has
        /// not made yet. False for a reference nobody knows. A script that Lua starts asks this on the thread of Lua,
        /// which runs only while the thread of the frame draws the scene (between allowUpdate and finishUpdate): the
        /// maps and objects that this reads are changed by the frame before that, never at the same time.
        bool whereIs(FalloutScript::FormId reference, Place& result);
        /// Moves the object now. An object that is not in a loaded cell is moved when its cell is loaded.
        void applyMove(FalloutScript::FormId reference, const Move& move);
        /// Puts the object at the place at the start of the next frame, and until then the script that asked sees it
        /// there even if it runs on another thread
        void askForMove(FalloutScript::FormId reference, const Move& move);
        /// Whether the object is in a cell that is not loaded (it was moved out of the loaded ones)
        bool leftLoadedCells(FalloutScript::FormId reference);

        /// What changes the world (the scene, the interface, the characters) is for the thread that runs the frame:
        /// a script that Lua starts runs on the thread of Lua, which is busy at the same time as the scene is drawn.
        /// The action is done at once on the thread of the frame, and in the next frame on any other.
        void runOnMainThread(std::function<void()> action);
        void applyDisabled(FalloutScript::FormId reference, bool disabled);
        /// Enables or disables the object alone, where it is or when its cell is loaded
        void applyDisabledAlone(FalloutScript::FormId reference, bool disabled);
        /// Who has whom for an enable parent, read from the records the first time it is asked for
        const FalloutScript::EnableParents& enableParents();
        void tellPlayer(const std::string& text);

        /// A message with buttons that waits to be shown, or is on the screen. The buttons are the ones whose
        /// conditions hold, with the number each has in the record.
        struct MessageBox
        {
            /// What the interface shows (with the colour tags of its text escaped) and what Lua is told
            std::string mText;
            std::string mPlainText;
            std::vector<std::string> mButtons;
            std::vector<std::string> mPlainButtons;
            std::vector<int> mNumbers;
            std::string mForm;
            /// The one who showed it, who gets the button that is picked
            FalloutScript::FormId mOwner = 0;
        };
        /// Shows the next message box when none is open and takes what the player picked in the one that was (the
        /// thread of the frame only)
        void updateMessageBoxes();
        void buildNames() const;

        const OFWorld::ESMStore& mStore;
        FalloutScript::CommandTable mCommands;
        FalloutScript::Interpreter mInterpreter;
        FalloutScript::ConditionEvaluator mConditions;
        FalloutScript::QuestManager mQuests;
        FalloutScript::ObjectScripts mObjects;
        FalloutScript::ActorState mActors;
        FalloutScript::Equipment mEquipment;
        /// Where the script of an item found it last (GetContainer)
        std::map<FalloutScript::FormId, FalloutScript::FormId> mItemHolders;
        FalloutScript::ActorValues mActorValues;
        FalloutScript::ReferenceState mReferenceState;
        FalloutScript::FormLists mFormLists;
        FalloutScript::PlayerProgress mProgress;
        FalloutScript::PerkEngine mPerks;
        FalloutScript::ItemUse mItemUse;
        FalloutScript::Chargen mChargen;
        /// The screen the attributes were last started for
        std::uint32_t mChargenSeeded = 0;
        /// What the player has been told and what was said. Scripts that Lua starts, the interface and the frame all
        /// reach it, so one at a time.
        mutable std::recursive_mutex mDialogueMutex;
        FalloutScript::DialogueManager mDialogue;
        std::mt19937 mDialogueRandom;
        std::string mSpeakerName;
        /// The voice files of the data, listed when the first line is said (a long list: 240 thousand for a game)
        std::mutex mVoiceMutex;
        FalloutScript::VoiceIndex mVoiceIndex;
        bool mVoiceIndexBuilt = false;
        /// The voice of the conversation: the file asked for, the speaker, and how it goes (the frame of the game
        /// plays it and reports here, the interface reads)
        std::string mConversationVoiceFile;
        FalloutScript::FormId mConversationVoiceSpeaker = 0;
        std::atomic<int> mConversationVoice{ 0 };
        /// The characters that say lines outside of a conversation (the thread of the frame only)
        std::vector<Saying> mSayings;
        /// The speakers and topics that sayTopic has logged (the first time is logged), under mDialogueMutex
        std::set<std::pair<FalloutScript::FormId, FalloutScript::FormId>> mSayLogged;
        FalloutScript::FormId mPlayer;
        FalloutScript::Game mGame;
        /// The characters whose perks have been brought up to date (to take back the effects of those they lost)
        std::set<FalloutScript::FormId> mPerkOwners;
        /// Whether the rules of levelling up have been read from the settings of the files
        std::atomic<bool> mLevellingRulesSet{ false };
        /// The levels that were reached and not shown yet
        std::mutex mLevelUpMutex;
        std::deque<FalloutScript::PlayerProgress::LevelUp> mLevelUps;
        /// How much damage the player did to each character it fought, which the experience of a kill depends on
        std::mutex mPlayerDamageMutex;
        std::map<FalloutScript::FormId, double> mPlayerDamage;
        std::thread::id mMainThread;
        /// What other threads asked the world for, and the state of the objects among it that scripts must see at once
        std::mutex mDeferredMutex;
        std::vector<std::function<void()>> mDeferred;
        /// The messages with buttons that wait their turn, and the numbers of the buttons of the one that is open
        std::deque<MessageBox> mMessageQueue;
        bool mMessageOpen = false;
        std::uint64_t mOpenBoxId
            = 0; // the box of the window manager that shows it, to tell it from one that replaced it
        std::vector<int> mOpenNumbers;
        FalloutScript::FormId mOpenOwner = 0;
        /// The box on the screen, for Lua to see and answer
        mutable std::mutex mOpenMessageMutex;
        std::optional<OpenMessage> mOpenMessage;
        std::uint32_t mMessageSerial = 0;
        /// The controls the player has been refused (bit i for the control i of FalloutScript::PlayerControl)
        std::atomic<unsigned> mControlsOff{ 0 };
        /// How many boxes wait or are open (scripts ask from their own thread)
        std::atomic<int> mMessageBoxCount{ 0 };
        /// The answer to the last message that was shown, and who may read it
        FalloutScript::MessageAnswer mAnswer;
        std::map<FalloutScript::FormId, bool> mDeferredDisabled;
        /// Where other threads asked to put objects, for the same reason: the next command of the script starts from
        /// there
        std::map<FalloutScript::FormId, Move> mDeferredPlaces;
        /// Whether scripts disabled (true) or enabled (false) objects in cells that were not loaded at the time, which
        /// holds until the cell is loaded
        std::map<FalloutScript::FormId, bool> mDisabled;
        /// The characters that talking activators speak for, by the reference of the activator (scripts ask from their
        /// own thread, the conversations from the thread of the frame)
        std::mutex mTalkingActorMutex;
        std::map<FalloutScript::FormId, FalloutScript::FormId> mTalkingActors;
        /// The objects that follow the state of another one (their enable parent)
        FalloutScript::EnableParents mEnableParents;
        bool mEnableParentsRead = false;
        /// The objects of the loaded cells by their references: the world only knows those that were put in the scene,
        /// which a disabled one never was
        std::unordered_map<FalloutScript::FormId, OFWorld::Ptr> mReferences;
        /// The trigger volumes of the loaded cells (the objects whose scripts have an OnTrigger block and whose
        /// reference has a volume): their shapes by reference, from the records; where they stand is read from the
        /// objects on every pass. The actors that can set them off are those of the loaded cells.
        FalloutScript::TriggerVolumes mTriggers;
        std::map<FalloutScript::FormId, FalloutScript::Volume> mTriggerShapes;
        std::set<FalloutScript::FormId> mTriggerActors;
        /// What a cell that was just loaded waits for until the next frame: objects to enable or disable, and objects
        /// whose scripts run OnLoad
        std::vector<std::pair<FalloutScript::FormId, bool>> mPendingToggle;
        std::vector<FalloutScript::FormId> mPendingLoad;
        /// Where scripts put objects: those that are waiting for the start of the next frame (the world is not changed
        /// in the middle of a script), and those whose cell was not loaded at the time, which hold until it is
        std::map<FalloutScript::FormId, Move> mPendingMoves;
        std::map<FalloutScript::FormId, Move> mRelocations;
        /// The objects PlaceAtMe made. Scripts name one by a number of its own (0xFF and a counter, as the games do),
        /// which is not the id the world gave it. An object that is asked for waits for the start of the next frame.
        std::map<FalloutScript::FormId, MadeObject> mPendingMade;
        std::map<FalloutScript::FormId, ESM::FormId> mMade;
        std::map<ESM::FormId, FalloutScript::FormId> mMadeIds;
        /// The counter of the numbers and the objects that are asked for, which a script that Lua starts uses from the
        /// thread of Lua: the rest of the script has to find the object it was told the number of at once
        std::mutex mMadeMutex;
        FalloutScript::FormId mMadeCount = 0;
        /// The base each number was asked for with, kept after the world makes the object: the records say what the
        /// object is from any thread, but the world only to the thread of the frame (under mMadeMutex)
        std::map<FalloutScript::FormId, FalloutScript::FormId> mMadeBases;
        /// The packages that scripts gave characters, the last given at the end. They stay while the character's cell
        /// is not loaded.
        std::map<FalloutScript::FormId, std::vector<FalloutScript::FormId>> mScriptPackages;
        /// The furniture that an actor uses, and where the actor was put on it: it stops using it when it is more than
        /// a few steps from there. Scripts ask from other threads too.
        struct FurnitureUse
        {
            FalloutScript::FormId mFurniture = 0;
            osg::Vec3f mPlace;
            /// The entry of the marker that it uses, -1 for none
            int mEntry = -1;
            /// The kind of use the entry says (sleeping 1 to 9, sitting 10 to 20), sitting for furniture with no marker
            int mKind = 10;
            /// Where the furniture was when it was used: it is not used any more when it is moved from there
            osg::Vec3f mFurniturePlace;
        };
        std::map<FalloutScript::FormId, FurnitureUse> mFurniture;
        mutable std::mutex mFurnitureMutex;
        /// The entries of the marker of each furniture, read from its model once (by the number of its record)
        std::map<FalloutScript::FormId, std::vector<FalloutScript::FurnitureEntry>> mFurnitureEntries;
        std::mt19937 mRandom;
        std::vector<std::string> mContentFiles;
        /// Seconds of play since the commands that scripts reached without a handler were last written to the log
        float mMissingTimer = 0;
        /// Writes the commands that scripts reached without a handler, with how often, to the log (nothing when there
        /// were none)
        void reportMissingCommands();
        /// The values the global variables have now, for those that were set (the others have the value of the record)
        std::map<FalloutScript::FormId, double> mGlobals;
        std::mutex mDerivedSettingsMutex;
        std::optional<FalloutScript::DerivedStatSettings> mDerivedSettings;
        std::optional<FalloutScript::DamageSettings> mDamageSettings;
        /// The settings of the files by their names in lower case, read when the first of them is asked for (the
        /// caller holds mDerivedSettingsMutex)
        const std::map<std::string, double>& gameSettingValues();
        /// The number a game setting has in the files (the names are not case sensitive), else the fallback
        double gameSetting(std::string_view name, double fallback);
        std::optional<std::map<std::string, double>> mGameSettingValues;
        /// The settings that are texts, by name (under mDerivedSettingsMutex as well)
        std::optional<std::map<std::string, std::string>> mGameSettingTexts;
        /// What the player has open to take things from, under mLootMutex
        std::mutex mLootMutex;
        std::optional<Loot> mLoot;
        int mLootSerial = 0;
        FalloutScript::CombatState mCombat;
        /// Whether the doors and switches are open, and the animations they play for it
        FalloutScript::OpenStates mOpenStates;
        /// The doors that are open or on the way to be closed: their collision is away until they are closed
        std::set<FalloutScript::FormId> mDoorsWithoutCollision;
        /// The blows are rolled for from any thread
        std::mutex mCombatRandomMutex;
        std::mt19937 mCombatRandom;
        /// The seconds until a character can attack again
        std::mutex mFireMutex;
        std::map<FalloutScript::FormId, float> mFireCooldowns;
        /// How a character behaves in a fight, from its AIDT (the record, or the template it takes it from)
        struct FightTraits
        {
            int mAggression = 0;
            int mConfidence = 2;
            float mRadius = 0; // how near the player has to be for it to go for it; 0 when the AIDT has no radius
        };
        const FightTraits& fightTraitsOf(const OFWorld::Ptr& actor);
        std::map<FalloutScript::FormId, FightTraits> mFightTraits;
        /// How far a character sees another one (fSneakMaxDistance of the files, which is where the games stop looking
        /// for a sneaking one; the light and the sneak skill of the target are not taken into account)
        double sightDistance();
        /// One blow at a time reaches the health and the fights of a character
        std::mutex mStrikeMutex;
        /// The date the world starts at (the initial values of GameYear, GameMonth and GameDay), read when the first
        /// date global is asked for
        struct CalendarStart
        {
            FalloutScript::GameDate mDate;
        };
        std::mutex mCalendarMutex;
        std::optional<CalendarStart> mCalendarStart;
        /// The days that scripts moved the date by when they set GameYear, GameMonth or GameDay
        long long mCalendarShift = 0;
        /// The date of the world now, false when the files do not have the globals of the date
        bool currentDate(FalloutScript::GameDate& date);
        /// The caller holds mCalendarMutex
        bool dateNow(FalloutScript::GameDate& date);
        /// A script sets one of the three date globals (0 the year, 1 the month, 2 the day): the date moves to it and
        /// goes on from there. False when the files have no date.
        bool setDate(int part, double value);
        /// Editor ids in lower case, made when first asked for
        mutable bool mNamesBuilt = false;
        mutable std::map<std::string, FalloutScript::FormId> mQuestNames;
        mutable std::map<std::string, FalloutScript::FormId> mGlobalNames;
        /// The names and descriptions of the actor values by their editor ids in lower case, made when first asked for
        mutable bool mActorValueTextBuilt = false;
        mutable std::map<std::string, ActorValueText> mActorValueText;
    };
}

#endif

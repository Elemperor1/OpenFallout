#ifndef OPENFALLOUT_SCRIPT_FALLOUTSCRIPTS_H
#define OPENFALLOUT_SCRIPT_FALLOUTSCRIPTS_H

#include <array>
#include <atomic>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <components/esm/formid.hpp>
#include <components/esm/refid.hpp>
#include <components/esm4/effectlist.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/actortraits.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/aimemory.hpp>
#include <components/falloutscript/animationcommands.hpp>
#include <components/falloutscript/barter.hpp>
#include <components/falloutscript/blackjack.hpp>
#include <components/falloutscript/calendar.hpp>
#include <components/falloutscript/caravan.hpp>
#include <components/falloutscript/casino.hpp>
#include <components/falloutscript/chargen.hpp>
#include <components/falloutscript/combatcommands.hpp>
#include <components/falloutscript/combatstate.hpp>
#include <components/falloutscript/commandtable.hpp>
#include <components/falloutscript/conditions.hpp>
#include <components/falloutscript/crafting.hpp>
#include <components/falloutscript/damage.hpp>
#include <components/falloutscript/derivedstats.hpp>
#include <components/falloutscript/detection.hpp>
#include <components/falloutscript/dialogue.hpp>
#include <components/falloutscript/enableparents.hpp>
#include <components/falloutscript/equipment.hpp>
#include <components/falloutscript/facecontrolfile.hpp>
#include <components/falloutscript/facelook.hpp>
#include <components/falloutscript/formlists.hpp>
#include <components/falloutscript/furniture.hpp>
#include <components/falloutscript/hacking.hpp>
#include <components/falloutscript/hotkeys.hpp>
#include <components/falloutscript/hud.hpp>
#include <components/falloutscript/interpreter.hpp>
#include <components/falloutscript/itemmods.hpp>
#include <components/falloutscript/itemuse.hpp>
#include <components/falloutscript/limbs.hpp>
#include <components/falloutscript/magic.hpp>
#include <components/falloutscript/mapmarkers.hpp>
#include <components/falloutscript/messagecommands.hpp>
#include <components/falloutscript/objectcommands.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/objectstate.hpp>
#include <components/falloutscript/packagecommands.hpp>
#include <components/falloutscript/perks.hpp>
#include <components/falloutscript/pipboy.hpp>
#include <components/falloutscript/placementcommands.hpp>
#include <components/falloutscript/playercontrols.hpp>
#include <components/falloutscript/playerprogress.hpp>
#include <components/falloutscript/presentation.hpp>
#include <components/falloutscript/queries.hpp>
#include <components/falloutscript/quests.hpp>
#include <components/falloutscript/referencestate.hpp>
#include <components/falloutscript/regionline.hpp>
#include <components/falloutscript/repair.hpp>
#include <components/falloutscript/reputation.hpp>
#include <components/falloutscript/roulette.hpp>
#include <components/falloutscript/saveaccess.hpp>
#include <components/falloutscript/slots.hpp>
#include <components/falloutscript/switches.hpp>
#include <components/falloutscript/triggervolumes.hpp>
#include <components/falloutscript/vanillacommands.hpp>
#include <components/falloutscript/vats.hpp>
#include <components/falloutscript/voicefiles.hpp>
#include <components/falloutscript/wear.hpp>

#include <osg/Vec3f>

#include "../mwworld/ptr.hpp"

namespace ESM
{
    class ESMReader;
    class ESMWriter;
}

namespace ESM4
{
    struct AIPackage;
    struct Armor;
    struct IdleAnimation;
    struct Weapon;
}

namespace FalloutScript
{
    struct SaveSection;
}

namespace Loading
{
    class Listener;
}

namespace ToUTF8
{
    class StatelessUtf8Encoder;
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
    /// What a loaded game says that waits for a cell or for the next frame (falloutsavepending.hpp)
    struct PendingSave;

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
                                 private FalloutScript::ReputationRecords,
                                 private FalloutScript::ObjectStateRecords,
                                 private FalloutScript::MapMarkerRecords,
                                 private FalloutScript::PresentationWorld,
                                 private FalloutScript::MagicRecords,
                                 private FalloutScript::FormListRecords,
                                 private FalloutScript::PackageWorld,
                                 private FalloutScript::PlacementWorld,
                                 private FalloutScript::QueryWorld,
                                 private FalloutScript::MessageWorld,
                                 private FalloutScript::ControlsWorld,
                                 private FalloutScript::CombatWorld,
                                 private FalloutScript::AnimationWorld,
                                 private FalloutScript::PerkRecords,
                                 private FalloutScript::UseRecords,
                                 private FalloutScript::RecipeRecords,
                                 private FalloutScript::LimbRecords
    {
        template <class T>
        friend struct FalloutScript::SaveAccess;

    public:
        /// Whether the game that is played is New Vegas (Tale of Two Wastelands is played with its engine)
        bool isNewVegas() const { return mGame == FalloutScript::Game::NewVegas; }

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

        /// Read the default texts of the game's string settings from its executable, which is next to the `Data`
        /// directory of one of these directories (the plugins hold only the settings that were changed from their
        /// defaults, and the labels of the screens, the texts of the terminals and the counters of the level-up screen
        /// are defaults). The executable of the game that is played is read first, then the other one. A game without
        /// its executable next to the data is left with the port's own words for the labels. The texts are made UTF-8
        /// with `encoder`, the code page that is set for the data of the game (Windows-1252 when there is none).
        void loadExecutableTexts(
            const std::vector<std::filesystem::path>& dataDirs, const ToUTF8::StatelessUtf8Encoder* encoder = nullptr);

        /// A saved game holds what the games keep (the stages of the quests, the variables of scripts, what characters
        /// carry, who is dead ...) as sections, one record each (REC_FOSV), after those of the world. A save that has
        /// none (made before the state of Fallout was saved) leaves the game as reset() made it.
        std::size_t countSavedGameRecords();
        void write(ESM::ESMWriter& writer, Loading::Listener& progress);
        /// Reads the section of the record that the reader is at. A save that was made by a newer version, or that is
        /// damaged, throws.
        void readRecord(ESM::ESMReader& reader, std::uint32_t type);
        /// What a saved game is made of: the sections, in the order they are written
        std::vector<FalloutScript::SaveSection> saveSections();
        /// What the list of saved games says of the player: the level and the health the games work out (the stats of
        /// Morrowind that the header would take them from are not the ones that count). `mKnown` is false when the
        /// player is not a character of the games (Morrowind's player, with or without plugins of Fallout), where the
        /// stats of Morrowind are the right ones.
        struct SavedPlayer
        {
            bool mKnown = false;
            int mLevel = 1;
            double mHealth = 0;
            double mMaxHealth = 0;
        };
        SavedPlayer savedPlayer();

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

        /// The player is given items out of nothing, as the AddItem of a script does (a tool of developers, to try out
        /// what needs a weapon, ammunition or armor in hand). False for a form that is no item.
        bool givePlayerItem(FalloutScript::FormId item, int count);

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
        /// The player is done with the screen of the level that was handed out last: the next level (if the experience
        /// reached one) is handed out after it
        void finishLevelUp();
        /// The value of a game setting (GMST) by its editor id, none when the files have no such setting
        std::optional<double> gameSetting(std::string_view name) override;
        /// The text of a game setting, empty when the files have none
        std::string gameSettingText(std::string_view name);
        /// What a token `&name;` of a message stands for: the keys and buttons of the controls (`&sUActnVats;` is the
        /// key the player presses for V.A.T.S., `&sKBSpace;` the name of that key) and the name, race and sex of the
        /// player. Gives nothing for a name that is none of them.
        std::optional<std::string> messageToken(std::string_view name);

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
            /// A weapon or a piece of armor has a condition (tenths of a percent of its health). Copies with another
            /// condition are rows of their own (mStack: the row is the copies at exactly that condition, and its id
            /// tells so)
            bool mHasCondition = false;
            bool mStack = false;
            int mCondition = 1000;
            /// The mods installed in the weapon (a mask of its slots); a copy with other mods is a row of its own
            int mMods = 0;
            /// A weapon: the damage of a blow of the record; armor: its rating and damage threshold
            double mDamage = 0;
            double mRating = 0;
            double mThreshold = 0;
            /// A key is listed on the keyring and under no tab; a book is read, not used up by use
            bool mKey = false;
            bool mBook = false;
            /// Armor that the files mark as not for the player: no list of the player shows it
            bool mHidden = false;
            /// Whether the player can use it now: ammunition only while the weapon in hand takes it
            bool mUsable = true;
            /// What the card of the item tells: the weight of one (a perk can lighten small things), for a weapon the
            /// damage of a shot with the perks of the player and how many pellets a shot flies, and the ammunition
            /// of the record with the rounds that are loaded and those in reserve
            double mCardWeight = 0;
            double mCardDamage = 0;
            int mPellets = 1;
            bool mHasAmmo = false;
            std::string mAmmoName;
            int mClip = 0;
            int mLoaded = 0;
            int mReserve = 0;
            /// The picture of the item in the inventory (the record's ICON), as the file names it; empty when it has
            /// none
            std::string mIcon;
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
            /// New Vegas shows a damage threshold and the weight and value of a whole stack; Fallout 3 neither
            bool mNewVegas = false;
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
        /// A condition of a perk, as the level-up screen writes the list of what the perk asks for. It writes a line
        /// for two kinds (an actor value at a number, a perk the player has), and nothing for the others (the level,
        /// the sex, a quest), but they count in how it writes the rest.
        struct PipboyRequirement
        {
            enum class Kind
            {
                Other,
                ActorValue,
                Perk,
            };
            Kind mKind = Kind::Other;
            int mActorValue = 0;
            /// The name of the actor value (an attribute or a skill; empty for any other) or of the perk (empty when
            /// the condition names none)
            std::string mName;
            /// The comparison of the condition: 0 equal, 1 not equal, 2 greater, 3 greater or equal, 4 less, 5 less or
            /// equal
            int mComparison = 3;
            /// The number compared with (an actor value), or the rank asked for (a perk; 0: any)
            int mNumber = 0;
            /// The condition is joined to the one after it by "or" (and not by "and")
            bool mOr = false;
        };
        /// The perks of the player with their ranks, and those it can choose from when it has a perk to spend
        struct PipboyPerk
        {
            FalloutScript::FormId mId = 0;
            std::string mName;
            std::string mDescription;
            /// The picture of the perk's record, and the least level it asks (the level-up screen shows both)
            std::string mIcon;
            int mMinimumLevel = 0;
            int mRank = 0;
            int mRanks = 1;
            /// The perks of the level-up list that cannot be chosen yet are shown dimmed
            bool mAvailable = true;
            std::vector<PipboyRequirement> mRequirements;
        };
        std::vector<PipboyPerk> pipboyPerks();
        std::vector<PipboyPerk> pipboyOfferedPerks();
        PipboyPerk pipboyPerkRow(const FalloutScript::PerkInfo& info, int rank);
        /// Spends skill points on a skill (the skill stays at most 100); the points spent, 0 when none could be
        int spendSkillPoints(int skill, int points);
        /// Takes skill points back from a skill (the screen of the level up does when the player takes back what it
        /// gave on that screen); the points given back, 0 when none could be
        int refundSkillPoints(int skill, int points);
        /// Spends the perk point on a perk the player is offered; false when it is not
        bool choosePerk(FalloutScript::FormId perk);
        /// The player does with the item what its kind asks (wear a weapon, armor or ammunition, or take it off again,
        /// use up an aid item or read a book); the words that tell what happened, empty when nothing did. A weapon or a
        /// piece of armor is the copy at that condition (tenths of a percent of its health, as the list gives it).
        /// A negative condition names no copy: the one that is on, else the best.
        std::string useItemFromList(FalloutScript::FormId item, int condition = -1);
        /// The player puts items on the floor in front of it (the copies at that condition, the best when none is
        /// named)
        int dropItem(FalloutScript::FormId item, int count, int condition = -1);
        /// The most objects one drop makes (each is an object of its own in the world)
        static constexpr int sDropLimit = 10;
        /// Items go from one holder to another (a container); the count that moved. The copies at that condition go; a
        /// negative condition moves copies in the order the holder gives them up (the full ones first), each with the
        /// condition it has.
        int transferItems(FalloutScript::FormId from, FalloutScript::FormId to, FalloutScript::FormId item, int count,
            int condition = -1);
        /// The player opens or closes a container, for the scripts that hear of it (OnOpen, OnClose)
        void containerOpened(FalloutScript::FormId container, bool opened);
        /// The player activates a container or a character that is dead: its things can be taken. False when the
        /// object has nothing to open (the player is told when it is locked, and that opens nothing either).
        bool openContainer(const OFWorld::Ptr& object, const OFWorld::Ptr& actor);
        /// The things a companion carries are open for the player (the result of the wheel's trade topic calls the
        /// command OpenTeammateContainer on the companion). Main thread.
        bool openTeammateContainer(FalloutScript::FormId teammate);
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

        // ---- The games of the casinos (M12) ----

        enum class CasinoGame
        {
            Blackjack,
            Roulette,
            Slots,
        };
        /// A game of a casino that the player has open (ShowBlackJackMenu, ShowRouletteMenu, ShowSlotMachineMenu):
        /// which, where, what the table takes and the chips the player sat down with. A number that is another for each
        /// game that is opened tells the screen that it is a new one.
        struct CasinoScreen
        {
            CasinoGame mGame = CasinoGame::Roulette;
            FalloutScript::FormId mCasino = 0;
            std::string mName;
            int mMinBet = 0;
            int mMaxBet = 0;
            int mSerial = 0;
            /// What the player has won in this casino so far, and the chips it had when it sat down
            int mEarnings = 0;
            int mStartChips = 0;
        };
        /// The player is to play a game at the casino (the commands of the menus). Nothing opens, and the player is
        /// told why, when the casino does not play with it any more, when it has not the chips for the least bet or
        /// when it has won less than the script asks (`needed`, 0 for none). False then.
        bool openCasino(CasinoGame game, FalloutScript::FormId casino, int minBet, int maxBet, int needed);
        /// GetCasinoWinningsLevel and the condition GetCasinoWinningStage: where the winnings in the casino are among
        /// the four quarters of the most it lets the player win (0 for a player who never played there)
        int casinoWinningsLevel(FalloutScript::FormId casino);
        /// SetCasinoWinningsLevel
        bool setCasinoWinningsLevel(FalloutScript::FormId casino, int level);
        std::optional<CasinoScreen> casinoScreen();
        /// The roulette table of the open game, nothing for another game
        struct RouletteView
        {
            int mAmount = 0;
            int mTotal = 0;
            int mChips = 0;
            int mBets = 0;
            int mMinBet = 0;
            int mMaxTotal = 0;
            bool mCanSpin = false;
            int mEarnings = 0;
            /// The table closes after the spin: it took the winnings to a new quarter of the casino's most, or the
            /// chips are fewer than the least bet (mBroke, and the player is told so)
            bool mCloses = false;
            bool mBroke = false;
            /// The stake on each area that has one (the place of the area in the list of areas, the stake)
            std::vector<std::pair<int, int>> mStakes;
        };
        std::optional<RouletteView> rouletteView();
        bool rouletteAmount(bool increase);
        bool roulettePlace(int area);
        bool rouletteRemove(int area);
        std::optional<FalloutScript::Casino::Roulette::Result> rouletteSpin();
        /// The slot machine of the open game, nothing for another game
        struct SlotsView
        {
            int mBet = 0;
            int mChips = 0;
            int mMinBet = 0;
            int mMaxBet = 0;
            int mEarnings = 0;
            /// The stops the three reels show now (0 to 13: the even numbers are the seven symbols, the odd ones the
            /// blank after a symbol)
            std::array<int, 3> mStops{};
            bool mCanSpin = false;
            /// The player has no chips left: the machine closes after the pull and says so
            bool mBroke = false;
            /// The machine closes after the pull: the winnings reached a new quarter of the casino's most, or no chips
            /// are left
            bool mCloses = false;
            /// The files of the casino's record that the screen is made from: the model of the machine (its eighth
            /// model) and the pictures of the seven symbols, as paths of the virtual file system (empty for one the
            /// record does not have)
            std::string mScene;
            std::array<std::string, 7> mSymbols;
        };
        enum class SlotsMove
        {
            Increase,
            Decrease,
            Highest,
        };
        std::optional<SlotsView> slotsView();
        bool slotsMove(SlotsMove move);
        /// One pull of the lever: what the reels came to, nothing when the machine cannot be played now
        std::optional<FalloutScript::Casino::SlotMachine::Result> slotsSpin();
        /// The blackjack table of the open game, nothing for another game
        struct BlackjackView
        {
            enum class State
            {
                Betting,
                Playing,
                Finished,
            };
            struct Card
            {
                int mRank = 0;
                int mSuit = 0;
            };
            struct Hand
            {
                std::vector<Card> mCards;
                int mBet = 0;
                /// The numbers of the executable: 0 none, 1 lose, 2 push, 3 win, 4 bust, 5 blackjack, 6 surrender
                int mResult = 0;
                bool mDoubled = false;
                bool mDone = false;
            };
            State mState = State::Betting;
            int mBet = 0;
            int mChips = 0;
            int mMinBet = 0;
            int mMaxBet = 0;
            int mEarnings = 0;
            std::vector<Card> mDealer;
            bool mHoleHidden = false;
            std::vector<Hand> mHands;
            int mActive = 0;
            bool mCanDeal = false;
            bool mCanHit = false;
            bool mCanStay = false;
            bool mCanDouble = false;
            bool mCanSplit = false;
            bool mCanSurrender = false;
            bool mCanSwitch = false;
            /// What the last round came to, which of Luck's helps was used, and that the player has not the chips for
            /// the least bet any more
            int mNet = 0;
            bool mDealerHelped = false;
            bool mPlayerHelped = false;
            bool mBroke = false;
            /// The round took the winnings to a new quarter of the casino's most: the table closes after it
            bool mCloses = false;
        };
        enum class BlackjackMove
        {
            Increase,
            Decrease,
            Highest,
            Deal,
            Hit,
            Stay,
            Double,
            Split,
            Surrender,
            Switch,
        };
        std::optional<BlackjackView> blackjackView();
        /// One thing the player does at the table; false when it cannot be done now
        bool blackjackMove(BlackjackMove move);
        /// The player gets up from the table: the chips it has more or less than when it sat down are put in or taken
        /// out of its pack, and the player is told
        void closeCasino();

        // ---- The card game Caravan (M12) ----
        // The character a script runs ShowCaravanMenu on plays against the player in four steps, each of them a screen:
        // the stake, the deck the player builds from its cards, the match and its result (FalloutNV.exe, the menu
        // 0x43B; Mac round 12AS E3). The money moves when the match ends.

        enum class CaravanStage
        {
            Bet,
            Deck,
            Game,
            Results,
        };
        /// The cards the player owns, one entry for each card (a stack of three is three entries), and whether each is
        /// in the deck
        struct CaravanCardView
        {
            FalloutScript::FormId mForm = 0;
            int mValue = 0;
            int mSuit = 0;
            std::string mName;
            bool mInDeck = false;
            /// The face of the card: the texture of its record (TX00), as the record writes it, under Textures
            std::string mFace;
        };
        /// Where the match is: the stage, who plays, the money and the deck
        struct CaravanScreen
        {
            CaravanStage mStage = CaravanStage::Bet;
            int mSerial = 0;
            std::string mOpponent;
            /// The stake: what the player has, what the opponent asks, what the player puts up, the most the opponent
            /// follows a raise to, the most the player may raise by, and the pot of the match
            int mFunds = 0;
            int mAnte = 0;
            int mBet = 0;
            int mCeiling = 0;
            int mMostRaise = 0;
            int mPot = 0;
            bool mCanMatch = false;
            bool mCanAccept = false;
            /// The deck: the cards in it and the cards the player owns, and whether it is big enough to play with
            int mInDeck = 0;
            int mOwned = 0;
            bool mCanPlay = false;
            /// The result, when the match is over
            bool mWon = false;
            bool mForfeited = false;
            /// What the player has won and lost at Caravan in all (with this match), for the result screen
            FalloutScript::Casino::CaravanTotals mTotals;
        };
        /// The player sits down to play the opponent (the reference the script is called on) with the deck record and
        /// the kind of opponent (0 and 1 are what the game has) and the share of its money it opens with. Nothing
        /// opens, and the player is told why, when it has fewer than 30 cards. False then.
        bool openCaravan(FalloutScript::FormId opponent, FalloutScript::FormId deck, int aiType, float fraction);
        std::optional<CaravanScreen> caravanScreen();
        /// The stake: match what the opponent asks, raise (the most is CaravanScreen::mMostRaise), accept
        bool caravanMatch();
        bool caravanRaise(int amount);
        bool caravanAccept();
        /// The deck: the cards, one of them in or out, a random choice of them (as many as the game takes), and the
        /// match
        std::vector<CaravanCardView> caravanCards();
        bool caravanSetInDeck(int index, bool inDeck);
        bool caravanRandomize();
        bool caravanPlay();
        /// The match: the six caravans (the player's are 1 to 3, the opponent's 4 to 6; the player's n faces 7 - n),
        /// what the sides hold, whose turn it is and whether it is over
        struct CaravanBoardCard
        {
            int mValue = 0;
            int mSuit = 0;
            /// The face of the card (see CaravanCardView::mFace)
            std::string mFace;
        };
        struct CaravanBoardTrack
        {
            /// The rows, the number card first and the face cards on it after it
            std::vector<std::vector<CaravanBoardCard>> mRows;
            int mTotal = 0;
            int mDirection = 0;
            int mSuit = 0;
        };
        struct CaravanBoard
        {
            std::array<CaravanBoardTrack, 6> mTracks;
            std::vector<CaravanBoardCard> mHand;
            int mOpponentHand = 0;
            int mPile = 0;
            int mOpponentPile = 0;
            /// "start", "play" or "over"
            std::string mPhase;
            bool mPlayersTurn = false;
            bool mOver = false;
            bool mWon = false;
        };
        std::optional<CaravanBoard> caravanBoard();
        /// Whether the card of the hand (counted from 0) can go on the row of the caravan (both counted from 0)
        bool caravanCanPlace(int track, int row, int hand);
        bool caravanPlace(int track, int row, int hand);
        bool caravanDiscardCard(int hand);
        bool caravanDiscardTrack(int track);
        /// The player gives the match up
        bool caravanForfeit();
        /// What the opponent did: the kind of move (the numbers of FalloutScript::Casino::Caravan::MoveType), where,
        /// and the card of its hand it used
        struct CaravanOpponentMove
        {
            bool mMade = false;
            int mType = 0;
            int mTrack = 0;
            int mRow = 0;
            CaravanBoardCard mCard;
        };
        CaravanOpponentMove caravanOpponentMove();
        /// The player gets up: a match that is not over is lost
        void closeCaravan();

        // ---- Locks, terminals, waiting and sleeping, the map (M10 slice 4) ----
        // What the player has open is kept here and read by the screens of Lua (core.interact), which show it and give
        // the choices of the player back. A number that is another for each time something is opened tells the screen
        // that it is a new one.

        /// The player activates a door or a container. When it is locked and the player carries its key the lock is
        /// opened and the activation goes on; else the screen of the lock opens (or the player is told that only a key
        /// opens it) and the activation stops: true.
        bool lockStops(const OFWorld::Ptr& object, const OFWorld::Ptr& actor);
        /// The lock the player has in front of it: what it is, how hard, and what the player has to open it
        struct LockScreen
        {
            FalloutScript::FormId mReference = 0;
            std::string mName;
            /// The word for how hard the lock is (sLockLevelName*)
            std::string mWord;
            int mLevel = 0;
            /// Only a key opens it
            bool mKeyOnly = false;
            /// The bobby pins the player has, the Lockpick skill and the chance that a pin opens the lock
            int mPins = 0;
            double mSkill = 0;
            double mChance = 0;
            int mSerial = 0;
            /// The class of the lock (0 very easy to 4 very hard, 5 only a key), the Lockpick skill that is needed to
            /// try it and the chance, in per cent, to force it
            int mClass = 0;
            double mGate = 0;
            double mForce = 0;
        };
        std::optional<LockScreen> lockScreen();
        /// What happened on the screen of the lock: "opened" (a pick turned it, or forcing did: the lock is unlocked,
        /// the experience is given once for the lock and the activation goes on), "pickBroke" (a bobby pin is gone) or
        /// "forceFailed" (the lock is broken for good and the screen closes). The number of pins the player has left,
        /// after it, is the result for a broken pin; "" when no lock is open.
        std::string lockOutcome(const std::string& kind);
        /// The player tries a bobby pin on the lock: "opened", "broke", "nopins" or "keyonly"; empty when no lock is
        /// open. The player is told by the line of messages, and the activation goes on when the lock opens.
        std::string pickLock();
        void closeLockScreen();

        /// A page of the help manual or a hint of the game: the editor id of the message that holds it, its title and
        /// its words (the marked up text of a message, `&sName;` for the words of game settings)
        struct HelpPage
        {
            std::string mId;
            std::string mTitle;
            std::string mText;
        };
        /// The pages of the help manual: the messages of the form list `HelpManual` (`HelpManualXBox` when the player
        /// plays with a pad), in the order of the list; empty when the data has no such list
        std::vector<HelpPage> helpManual(bool pad);
        /// The message of a hint by its editor id (`HelpCrafting`), nothing when there is none
        std::optional<HelpPage> helpMessage(std::string_view editorId);

        /// The player activates a terminal: it opens, or the game of hacking starts when it is closed and the player
        /// has not got its password. False for an object that is no terminal.
        bool openTerminal(const OFWorld::Ptr& object, const OFWorld::Ptr& actor);
        struct TerminalLine
        {
            int mIndex = 0;
            std::string mText;
        };
        struct HackGuess
        {
            std::string mWord;
            int mLikeness = 0;
        };
        /// What the terminal shows: its text (or the result of the line that was chosen), the lines of its menu that
        /// the player may choose, and while it is closed the words of the game of hacking
        struct TerminalScreen
        {
            FalloutScript::FormId mTerminal = 0;
            std::string mName;
            std::string mBody;
            std::vector<TerminalLine> mLines;
            /// A menu above this one: the menus below the first have a Back line, and the first is left by the key
            bool mCanGoBack = false;
            /// What the line that was chosen last tells, shown under the menu (it stays until another comes or the
            /// menu changes; `mResultSerial` is another for each text)
            std::string mResult;
            int mResultSerial = 0;
            /// The text of a note that a line showed, in the zone of the screen; Back closes it
            std::string mNote;
            /// The introduction of the screen (the terminal asks for a password) is typed before the menu; the stars
            /// of its last line are as many as the letters of a word of the game of hacking of this terminal
            bool mIntro = false;
            int mIntroStars = 0;
            bool mHacking = false;
            int mHackLevel = 0;
            std::vector<std::string> mWords;
            std::vector<HackGuess> mGuesses;
            int mAttempts = 0;
            int mMaxAttempts = 0;
            /// The dump of memory of the game of hacking as it stands, the 16 bits of the address of its first row, and
            /// how many words are still in it
            std::string mDump;
            int mAddress = 0;
            int mWordsLeft = 0;
            /// The attempts were spent, or the terminal was locked out before: it stays shut until a script opens it
            bool mLockedOut = false;
            /// The password was found (the screen of the game of hacking shows it for a moment and then asks
            /// `hackFinish`)
            bool mSolved = false;
            /// Seconds before a board is laid out again, after the screen of a game of hacking was left (the executable
            /// waits iHackingRetryMilliseconds from the closing of the screen)
            double mGate = 0;
            int mSerial = 0;
        };
        /// The seconds that are left before a board of the game of hacking can be laid out (the executable waits
        /// iHackingRetryMilliseconds from the closing of the screen of the last one)
        double hackGate();
        /// The number of the screen of the terminal that is open, 0 when none is; another for each change
        int terminalSerial();
        std::optional<TerminalScreen> terminalScreen();
        /// The player chooses a line of the menu (the number the line has in the screen)
        void terminalChoose(int index);
        /// The player goes back, or leaves the terminal from its first menu
        void terminalBack();
        /// The player guesses a word of the game of hacking (its place in the words); the likeness of it, -1 when the
        /// guess was not made
        int hackGuess(int word);
        /// What is at a place of the dump (counted from 0): a word, a pair of brackets or a character, where it begins
        /// and how long it is
        struct HackUnit
        {
            /// "word", "brackets" or "character"
            std::string mKind;
            int mFirst = 0;
            int mLength = 0;
        };
        std::optional<HackUnit> hackUnit(int position);
        /// What choosing a place of the dump did (see FalloutScript::Hacking::Puzzle::Pick)
        struct HackPick
        {
            HackUnit mUnit;
            std::string mText;
            /// Nothing was done
            bool mIgnored = true;
            bool mCorrect = false;
            int mLikeness = 0;
            /// "none", "denied", "removed" or "replenished"
            std::string mEffect = "none";
            int mAttemptsLeft = 0;
            /// The attempts are spent: the terminal is locked for a while
            bool mLockedOut = false;
        };
        HackPick hackPick(int position);
        /// The screen of the game of hacking has shown that the password was found: the terminal goes on to its menu
        void hackFinish();
        void closeTerminal();

        /// What the screen of waiting and sleeping shows: it was asked for (the Wait key, or a bed)
        struct RestScreen
        {
            bool mSleeping = false;
            FalloutScript::FormId mBed = 0;
            int mSerial = 0;
        };
        /// The player wants to wait, or lies down on a bed (a bed that is no bed for the player is refused here)
        void requestRest(bool sleeping, FalloutScript::FormId bed);
        std::optional<RestScreen> restScreen();
        /// The player starts to wait or sleep for that many hours; the text of why not, empty when it began
        std::string startRest(double hours);
        struct RestProgress
        {
            bool mActive = false;
            bool mSleeping = false;
            double mDone = 0;
            double mTotal = 0;
            /// Why it ended before the hours were over (empty when it ran out)
            std::string mStopped;
            /// Another number for each rest that ended, so that the screen sees it end
            int mEnded = 0;
        };
        RestProgress restProgress();
        /// The player stops waiting or sleeping, or leaves the screen
        void cancelRest();

        /// The map markers the player has found, with where the player stands: for the map of the Pip-Boy
        struct MapEntry
        {
            FalloutScript::FormId mId = 0;
            std::string mName;
            osg::Vec3f mPosition;
            bool mTravel = false;
            double mDistance = 0;
            /// The worldspace the marker is in (0 for a place inside a building)
            FalloutScript::FormId mWorldspace = 0;
            /// What kind of place the record says it is (the first byte of its `TNAM`), which the icon follows
            unsigned mType = 0;
        };
        std::vector<MapEntry> mapEntries();

        /// What the map of the Pip-Boy needs to draw the world and the player on it: the worldspace whose picture it
        /// shows (the one the player is in, or the one of the first marker found when the player is inside a building),
        /// the picture and the rectangle of cells that the plugin gives it, where the player stands and faces, the name
        /// of the place, and the date and time of the world
        struct MapInfo
        {
            FalloutScript::FormId mWorldspace = 0;
            /// Whether the player is in the open world (the position and heading are on the map then)
            bool mOutside = false;
            std::string mLocation;
            std::string mWorldName;
            /// The picture of the worldspace record (`ICON`), empty when it has none
            std::string mMapFile;
            /// The record's `MNAM`: the usable size of the picture and the cells at its north-west and south-east
            /// corners (all 0 when the record has none)
            unsigned mWidth = 0;
            unsigned mHeight = 0;
            int mNorthWestX = 0;
            int mNorthWestY = 0;
            int mSouthEastX = 0;
            int mSouthEastY = 0;
            osg::Vec3f mPosition;
            /// Degrees clockwise from north
            double mHeading = 0;
            bool mHasDate = false;
            FalloutScript::GameDate mDate;
            double mHour = 0;
        };
        MapInfo mapInfo();
        /// The world space whose map was last said in the log (once for each, see `mapInfo`)
        FalloutScript::FormId mMapSpaceTold = 0;
        /// The world space the player last stood in outside (0 before): the map of a building is the one of the world
        /// outside it
        FalloutScript::FormId mLastOpenSpace = 0;
        /// The world space that holds the most map markers (the main world of the game: the Mojave, the Capital
        /// Wasteland), found once with the markers: the map of a building when the player has not been outside
        FalloutScript::FormId mMainSpace = 0;
        /// The player goes to a marker at once: the text of why not, empty when it did
        std::string travelTo(FalloutScript::FormId marker);

        /// The menu of recipes the player has open at a workbench, a reloading bench or a campfire (ShowRecipeMenu):
        /// the category, its name, and a number that is another for each time a menu is opened. Nothing when none is.
        struct RecipeMenu
        {
            FalloutScript::FormId mCategory = 0;
            std::string mName;
            int mSerial = 0;
        };
        std::optional<RecipeMenu> recipeMenu();
        /// What a recipe takes or gives, and how many of it the player has
        struct RecipeItem
        {
            FalloutScript::FormId mItem = 0;
            std::string mName;
            int mCount = 0;
            int mHave = 0;
        };
        /// A recipe of the open menu as the player sees it
        struct RecipeLine
        {
            FalloutScript::FormId mId = 0;
            std::string mName;
            FalloutScript::FormId mSubCategory = 0;
            std::string mSubCategoryName;
            /// "ready", "skill" (the skill is too low) or "items" (some ingredient is short)
            std::string mStatus;
            /// The actor value of the skill, -1 for none, the level of it the recipe needs and the skill of the player
            int mSkill = -1;
            int mLevel = 0;
            double mSkillValue = 0;
            std::vector<RecipeItem> mIngredients;
            std::vector<RecipeItem> mOutputs;
        };
        /// The recipes of the open menu that the player may use (none when no menu is open)
        std::vector<RecipeLine> recipeLines();
        /// The player makes the recipe of the open menu: "made", or why not ("skill", "items"); empty for a recipe that
        /// is not in the menu
        std::string craftRecipe(FalloutScript::FormId recipe);
        /// The player is done with the menu
        void closeRecipes();
        /// A script shows the player the recipes of a category (ShowRecipeMenu); false for a form that is no category
        bool openRecipes(FalloutScript::FormId category);
        /// The name of an item (a weapon, armor, ammunition, aid item, book, key, item mod, ingredient, other thing
        /// that is carried), its editor id when it has none; empty for a form that is none
        std::string itemName(FalloutScript::FormId item);

        // ---- The barter menu (M12) ----
        /// The barter menu a script showed (ShowBarterMenu, from the response of a conversation): the character that
        /// trades, its name, the percent that the script gave (the player pays that much less when it is above 0) and a
        /// number that is another for each time a menu is opened
        struct BarterMenu
        {
            FalloutScript::FormId mVendor = 0;
            std::string mName;
            int mPercent = 0;
            int mSerial = 0;
            /// A deal was made while the menu was open: the vendor says a line when it closes
            bool mDealt = false;
        };
        /// A script opens the barter menu for the character it is called on; false when there is none to trade with
        bool openBarter(FalloutScript::FormId vendor, int percent);
        std::optional<BarterMenu> barterMenu();
        /// The player is done with the menu
        void closeBarter();

        // ---- Repair (M12) ----
        /// What an item does at a condition, for the cards of the repair screens: the damage of a weapon and the rating and
        /// the threshold of armor, as whole numbers
        struct RepairStat
        {
            int mDamage = 0;
            int mRating = 0;
            int mThreshold = 0;
        };
        /// One thing that can repair an item and be used up by it
        struct RepairDonor
        {
            FalloutScript::FormId mItem = 0;
            int mCondition = FalloutScript::ActorState::fullCondition;
            int mMods = 0;
            std::string mName;
            bool mWorn = false;
            /// The condition the item is in when this repairs it, and what it does then
            int mResult = 0;
            RepairStat mStat;
        };
        /// The repair screen of the Pip-Boy for one weapon or piece of armor of the player
        struct RepairView
        {
            /// The Repair skill of the player now
            int mSkill = 0;
            FalloutScript::FormId mItem = 0;
            std::string mName;
            std::string mIcon;
            /// The condition of the copy that is repaired, in tenths of a percent of its health
            int mCondition = FalloutScript::ActorState::fullCondition;
            /// The mods installed in the copy (a mask of the slots of a weapon)
            int mMods = 0;
            bool mWorn = false;
            RepairStat mStat;
            /// One for each copy that can be used up, the weakest first (FalloutNV.exe 0x7B6AA0, 0x7B5950)
            std::vector<RepairDonor> mDonors;
        };
        /// The screen for the copy of the item that the number names (condition with mods over it, see copyOf; a negative
        /// one: the copy that is on, else the best);
        /// nothing for what is neither a weapon nor armor, or when the player has no such copy
        std::optional<RepairView> repairView(FalloutScript::FormId item, int condition);
        /// Whether the Pip-Boy offers to repair that copy: it is below full condition and something can repair it
        bool canRepairItem(FalloutScript::FormId item, int condition);
        /// The donor is used up and the copy is repaired with it; the number of the copy as it is now (it keeps its mods),
        /// -1 when nothing was done
        int repairItem(FalloutScript::FormId item, int condition, FalloutScript::FormId donor, int donorCondition);

        /// The repair services a script showed (ShowRepairMenu, on the character that repairs): the character, its name
        /// and a number that is another for each time a menu is opened
        struct RepairServices
        {
            FalloutScript::FormId mVendor = 0;
            std::string mName;
            int mSerial = 0;
            /// The Repair skill of the vendor, read once when the menu opens (-1 until then)
            int mSkill = -1;
        };
        bool openRepairServices(FalloutScript::FormId vendor);
        std::optional<RepairServices> repairServices();
        /// The player is done with the menu; the vendor says a line of the topic RepairExit when a conversation is open
        void closeRepairServices();
        /// A copy of a weapon or armor that the vendor can repair, and what it asks
        struct ServiceRow
        {
            FalloutScript::FormId mItem = 0;
            int mCondition = FalloutScript::ActorState::fullCondition;
            int mMods = 0;
            std::string mName;
            std::string mIcon;
            bool mWorn = false;
            int mCost = 0;
            bool mCanPay = false;
            /// The health of the copy in points (the sort of the executable goes by it, not by the percent)
            double mHealth = 0.0;
            /// What the copy does as it is, and what it does at the condition the vendor leaves it in
            RepairStat mStat;
            RepairStat mCapStat;
        };
        struct ServiceView
        {
            int mSkill = 0;
            int mCaps = 0;
            /// What all the rows ask together (the rows that cost nothing add none)
            int mTotal = 0;
            /// The condition the vendor leaves things in
            int mCap = 0;
            std::vector<ServiceRow> mRows;
        };
        std::optional<ServiceView> serviceView();
        /// The vendor repairs the copy (a copy of the row, the worn one when `worn`), or every row that can be paid for
        /// when `all`, and the player pays; how many copies were repaired
        int repairService(FalloutScript::FormId item, int condition, bool worn, bool all);

        // ---- Item mods (M12, New Vegas) ----
        /// What a weapon's record says about its three mod slots; false when the weapon has none (the record has no game
        /// data, or it is not a weapon)
        bool modSlotsOf(FalloutScript::FormId weapon, FalloutScript::ItemMods::Slots& slots);
        /// What the mods installed in a copy add to its worth
        int modsValue(FalloutScript::FormId weapon, int mods);
        /// One line of the item mod screen: a mod the weapon takes (installed in it, or in the player's pack)
        struct ModRow
        {
            FalloutScript::FormId mMod = 0;
            std::string mName;
            std::string mIcon;
            std::string mDescription;
            bool mInstalled = false;
        };
        /// The item mod screen of the Pip-Boy for one weapon of the player
        struct ModView
        {
            FalloutScript::FormId mItem = 0;
            std::string mName;
            std::string mIcon;
            /// The condition of the copy (tenths of a percent of the health it has at most) and the mods installed
            int mCondition = FalloutScript::ActorState::fullCondition;
            int mMods = 0;
            /// What a shot of the copy does as it is now
            int mDamage = 0;
            std::vector<ModRow> mRows;
        };
        /// The copy a number names; a negative number names the copy that is on, else the best (-1 when there is none)
        int copyToUse(FalloutScript::FormId item, int copy);
        /// The player opens the screen for the copy that the number names (condition with mods over it, see copyOf): the
        /// weapon is taken off when it was on (the screen puts it back when it closes); nothing when the copy is not a
        /// weapon of the player
        std::optional<ModView> openItemMods(FalloutScript::FormId item, int copy);
        /// The screen for the copy as it is now (after a change)
        std::optional<ModView> modView(FalloutScript::FormId item, int copy);
        /// The mod is installed in the copy and one copy of the mod item is used up; the number of the copy as it is now,
        /// -1 when nothing was done
        int installMod(FalloutScript::FormId item, int copy, FalloutScript::FormId mod);
        /// The player is done: the weapon that was taken off when the screen opened is on again
        void closeItemMods(FalloutScript::FormId item, int copy);

        // ---- The companion wheel (M12) ----
        /// The wheel the player has open on a companion (companion_wheel_menu.xml, FalloutNV.exe 0x754DE0): the
        /// character, its name, a number that is another for each time a wheel opens and what the script of the
        /// companion says when the wheel opens: its variables FollowerSwitchAggressive (1: aggressive), IsFollowingLong
        /// (1: keeps far), CombatStyleRanged (1: ranged) and Waiting (0: following; a variable the script has not
        /// counts as -1, so it is not any of them)
        struct CompanionWheel
        {
            FalloutScript::FormId mActor = 0;
            std::string mName;
            int mSerial = 0;
            bool mAggressive = false;
            bool mStaysFar = false;
            bool mRanged = false;
            bool mFollowing = false;
            bool mWaiting = false;
            /// A pad was the last thing the player used when the wheel opened
            bool mPad = false;
        };
        /// What the wheel tells under the slice the pointer is on: the stimpaks of the player, the health of the
        /// companion, what it carries of what it can carry, and the weapon in its hands
        struct CompanionStatus
        {
            int mStimpaks = 0;
            double mHealth = 0;
            double mMaxHealth = 0;
            double mCarried = 0;
            double mCapacity = 0;
            std::string mWeapon;
        };
        /// A character that follows the player and is alive: the player gets the wheel when it activates it, and not a
        /// conversation (Activate on a companion, FalloutNV.exe 0x5FA330 and 0x754D90)
        bool isCompanion(const OFWorld::Ptr& actor);
        /// The player activated the companion. The thread of the frame: the variables of its script are read here.
        bool openCompanionWheel(const OFWorld::Ptr& actor);
        std::optional<CompanionWheel> companionWheel();
        CompanionStatus companionStatus();
        /// The companion says a topic of the commands (FollowersWait, FollowersLetsGo, FollowersTacticsCombatAggressive
        /// ...) found by its editor id: the result of the response is what changes the companion. False when there
        /// is no such topic or nothing to say.
        bool companionSay(std::string_view topic);
        /// The wheel writes FollowerSwitchAggressive itself when the player toggles it (0x755260)
        void companionSetAggressive(bool aggressive);
        /// A stimpak of the player's goes to the companion and is used on it; false when the player has none
        bool companionStimpak();
        /// The wheel is closed and the conversation with the companion begins
        void companionTalk();
        /// The player is done with the wheel
        void closeCompanionWheel();

        // ---- The hot keys (M12) ----
        /// One slot of the hot key wheel: what the player put on that number key (nothing when mItem is 0)
        struct HotKeySlot
        {
            int mSlot = 0;
            /// The second slot is the ammunition swap, which takes no item
            bool mAssignable = true;
            FalloutScript::FormId mItem = 0;
            std::string mName;
            /// How many of it the player carries now
            int mCount = 0;
            /// The picture of the item (the record's ICON)
            std::string mIcon;
            bool mEquipped = false;
        };
        /// The eight slots, in order. A slot whose item the player no longer carries (dropped, sold, used up) is
        /// emptied here: the executable keeps the key on the entry of the inventory, so it goes with the entry.
        std::vector<HotKeySlot> hotKeySlots();
        /// Puts an item the player carries on the key of the slot: "assigned", "ammo" (ammunition goes on no key),
        /// "cannot" (a thing that is neither held, worn nor used up, or one the player does not carry) or "slot" (the
        /// ammunition swap, or none)
        std::string assignHotKey(int slot, FalloutScript::FormId item);
        /// The picture of an item (its record's ICON), empty for a form that has none
        std::string itemIcon(FalloutScript::FormId item);
        /// The container that holds a vendor's goods: the one a script gave it (SetMerchantContainer; none after
        /// RemoveMerchantContainer), else the XMRC of its placed reference. 0 for none.
        FalloutScript::FormId merchantContainer(FalloutScript::FormId vendor);
        void setMerchantContainer(FalloutScript::FormId vendor, FalloutScript::FormId container);
        /// Where the things and caps that the vendor gets go: its container, else the vendor itself
        FalloutScript::FormId barterHolder(FalloutScript::FormId vendor);
        /// A row of a list of the menu: the item as the Pip-Boy lists it and the price of one of it
        struct BarterRow
        {
            PipboyItem mItem;
            double mPrice = 0;
        };
        /// What the vendor sells (`vendorSide`) or what the player carries, as the menu lists it
        std::vector<BarterRow> barterRows(bool vendorSide);
        /// What is picked out to buy or to sell: the item and how many
        struct BarterPick
        {
            FalloutScript::FormId mItem = 0;
            int mCount = 0;
            /// The copies at that condition (tenths of a percent of the health); negative: the best ones
            int mCondition = -1;
        };
        using BarterPiles = std::vector<BarterPick>;
        /// What the menu shows for the two piles: the balance (what the vendor owes the player is plus), the caps that
        /// would move, whether the Accept button works, the caps each side has and the holder of the vendor's things
        struct BarterQuote
        {
            bool mValid = false;
            bool mPilesEmpty = true;
            bool mAccept = false;
            double mBalance = 0;
            int mAmount = 0;
            int mPlayerCaps = 0;
            int mVendorCaps = 0;
            FalloutScript::FormId mHolder = 0;
        };
        BarterQuote barterQuote(const BarterPiles& buy, const BarterPiles& sell);
        /// The player accepts the deal: "done" (with the caps that moved and whether the vendor paid them), or
        /// "empty", "items" (a pile asks for what is not there), "caps" (the player has too few) or "closed"
        struct BarterOutcome
        {
            std::string mStatus;
            int mAmount = 0;
            bool mReceived = false;
        };
        BarterOutcome barterDeal(const BarterPiles& buy, const BarterPiles& sell);

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
            bool mCrippled = false; // the blow crippled the part of the body it hit
        };
        /// The attacker strikes the victim with the weapon (0 for the weapon the attacker holds, or its hands): the
        /// blow is worked out from the weapon, the skill and strength of the attacker and the armor of the victim, the
        /// health of the victim goes down, and the victim dies, or when it is essential falls unconscious. The victim
        /// fights back if it was at peace. The OnHit, OnHitWith and OnDeath blocks of the victim run in the next
        /// frame. `limb` is the part of the body that was hit, -1 when not known; `roll` (0 to 100) decides whether the
        /// blow is critical, a negative one is drawn at random. The copy that strikes is the one in the hands of the
        /// attacker unless `condition` (tenths of a percent, 0 or more) and `mods` name it: a weapon that breaks with
        /// the blow is out of the hands by then. With `noDamage` (a shot of UseWeapon with DoNoDamage) the blow is a
        /// hit that does no damage: the blocks of the victim run and it fights back, and nothing is taken from it. Any
        /// thread.
        StrikeResult strike(FalloutScript::FormId attacker, FalloutScript::FormId victim,
            FalloutScript::FormId weapon = 0, int limb = -1, double roll = -1, int condition = -1, int mods = 0,
            bool noDamage = false);

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
        /// How far a blow of the bare hands reaches
        double unarmedReach(FalloutScript::FormId attacker);

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
        /// `ignoreCooldown` is for an attack of V.A.T.S., which comes when the player queued it and not when the
        /// weapon is ready.
        /// `noDamage` is for an attack of UseWeapon that hits and does no damage.
        FireResult fire(FalloutScript::FormId attacker, const OFWorld::Ptr& target, int limb = -1, double chance = 1.0,
            bool ignoreCooldown = false, bool noDamage = false);

        /// V.A.T.S.: what the player's weapon and action points allow
        struct VatsState
        {
            bool mAvailable
                = false; // the player has a weapon it can attack with in V.A.T.S. and is not dead or unconscious
            bool mOpen = false;
            double mActionPoints = 0;
            double mMaxActionPoints = 0;
            double mCost = 0; // the action points one attack costs
            double mRange = 0; // the farthest a target can be (infinite for a shot of a weapon without fixed range)
            std::string mWeaponName;
        };
        VatsState vatsState();
        /// A part of the body of a target that V.A.T.S. can aim at, with the chance in percent of a hit with the weapon
        /// the player holds. The parts a body does not have for V.A.T.S. (no target name) are left out.
        struct VatsPart
        {
            int mIndex = 0; // the number of the part in the body part data, which the screen and vatsAttack use
            int mType = -1; // the type of the part, which strike and GetHitLocation use
            /// The actor value that is the condition of the part (25 head, 26 torso, 27 and 28 the arms, 29 and 30 the
            /// legs, 31 the brain), which V.A.T.S. takes for the group the part is in: the parts that share it are
            /// picked out together. -1 for a part that has none
            int mGroup = -1;
            std::string mName;
            std::string mTarget;
            double mChance = 0;
            double mCondition = 100; // in percent
        };
        std::vector<VatsPart> vatsParts(const OFWorld::Ptr& target);
        /// What the screen shows of a target: its name, how far it is from the player and its health
        struct VatsTarget
        {
            std::string mName;
            double mDistance = 0;
            double mHealth = 0;
            double mMaxHealth = 0;
        };
        VatsTarget vatsTarget(const OFWorld::Ptr& target);
        /// Whether the target can be aimed at in V.A.T.S.: a character or creature that is alive, in reach of the
        /// weapon
        bool vatsCanTarget(const OFWorld::Ptr& target);
        struct VatsAttackResult
        {
            /// Why nothing was done: "points", "ammo", "range", "dead", "none" (no weapon or no such part); empty when
            /// the attack was made
            std::string mRefused;
            bool mHit = false;
            double mCost = 0;
            double mChance = 0;
            StrikeResult mStrike;
        };
        /// The player attacks the part of the target in V.A.T.S.: the action points are spent, the round used up and
        /// the blow struck when the roll is below the chance. Main thread.
        VatsAttackResult vatsAttack(const OFWorld::Ptr& target, int part);
        /// V.A.T.S. is open or not (GetVATSMode answers it; the screen of Lua says it)
        void setVatsOpen(bool open);

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
        /// character with no weapon and no melee weapon in its inventory fights with its hands), and draws it; with
        /// `say` the log tells what it took and what it has
        void prepareFight(const OFWorld::Ptr& actor, bool say = false);
        /// Whether the character, which is at peace, goes for the player when it sees it: from its aggression and how
        /// its factions stand to the player. `radius` is the radius its AI data names, within which it sees the player
        /// whatever the stealth equation says, 0 when it has none.
        bool attacksPlayerOnSight(const OFWorld::Ptr& actor, float& radius);
        /// How well the character sees or hears the player: the stealth equation of the games with the sneak and the
        /// movement of the player, the light on it and the distance, and a view cone and a line of sight
        FalloutScript::Detection::Sighting sightOfPlayer(const OFWorld::Ptr& watcher);
        /// The seconds between the looks of a character that looks for the player (fDetectionTimerSetting)
        double detectionInterval();
        /// A character looks for the player when its turn comes (every detectionInterval), and what it made of the
        /// player is kept for the sneak meter. Only characters near enough to tell are looked at. Main thread.
        void watchPlayer(const OFWorld::Ptr& watcher);

        /// The head-up display. The line of messages the games print in the corner (a reference to it: the display
        /// takes what was said, other code says things).
        FalloutScript::Hud::MessageLine& hudMessages() { return mHudMessages; }
        /// What the characters say to the player outside of a conversation, for the subtitles of the display
        FalloutScript::Hud::SubtitleLine& hudSubtitles() { return mHudSubtitles; }
        /// What the quests and the finding of places tell the display: the quest that was added, completed or failed, the
        /// place that was found, and the lines of the objectives
        FalloutScript::Hud::QuestTextLine& hudQuestText() { return mHudQuestText; }
        /// What the line over the compass is told since this was last called, for the display: the name of the region
        /// the player came into, and, when the cell the player came to is not the next one of the outside (a door, a
        /// teleport, a load) and is in the outside, the name of its world space (FalloutNV.exe 0x7728F0 and 0x772C30). It is
        /// looked for when the player's cell is another one. Lua thread.
        struct RegionEvent
        {
            std::string mName;
            /// The name of the world space after a placement in the outside (else the name of a region)
            bool mCell = false;
        };
        std::vector<RegionEvent> takeRegionEvents();
        /// What the meters of the display that are not the player's stats show
        struct HudStatus
        {
            double mRads = 0;
            double mRadsMax = 1000;
            bool mSneaking = false;
            FalloutScript::Hud::Sneak mSneak = FalloutScript::Hud::Sneak::Hidden;
            std::string mSneakText;
            /// The names of the textures of the limbs that are crippled (under Interface\Stats)
            std::vector<std::string> mCrippled;
            bool mWeaponDrawn = false;
            /// The breath of the player: whether its head is under the water, the share of the breath that is left and
            /// how many of the 16 ticks of the air meter show (FalloutScript::DerivedStats::airMeterVisibleTicks)
            bool mHeadUnder = false;
            double mBreath = 1;
            int mBreathTicks = 16;
            /// The controls that DisablePlayerControls took away, one bit for each in the order of
            /// FalloutScript::PlayerControl
            unsigned mControlsOff = 0;
        };
        HudStatus hudStatus();
        /// The character or creature that the crosshair is on: its name, health and whether it fights the player. False
        /// for anything that is not one.
        struct HudTarget
        {
            std::string mName;
            double mHealth = 0;
            double mMaxHealth = 0;
            bool mHostile = false;
            bool mDead = false;
        };
        bool hudTarget(const OFWorld::Ptr& target, HudTarget& result);
        /// What the text under the crosshair says of the object that the crosshair is on (FalloutNV.exe 0x775A00, Mac
        /// rounds 12AM B2 and 12AN B7): its name, what the player does with it, its weight and value, and what its lock
        /// is. False for an object the text says nothing of (one with no name, or a static one).
        struct HudInfo
        {
            std::string mName;
            /// How many there are, when it is a stack of things
            int mCount = 1;
            /// The setting that holds the word for what the player does with it (sTargetTypeTake, sSearch ...), empty
            /// when the player can do nothing with it
            std::string mVerb;
            /// The weight and value row is under the name (not for every thing that can be taken: see hudInfo)
            bool mStats = false;
            double mWeight = 0;
            double mValue = 0;
            /// Its lock is broken (forcing it failed)
            bool mBroken = false;
            /// It is a door that the reference marks as inaccessible (after the broken lock in the order of the text)
            bool mInaccessible = false;
            /// It is locked, whether the player carries its key, the setting that holds the word for how hard the lock
            /// is, and whether only the key opens it
            bool mLocked = false;
            bool mHasKey = false;
            std::string mLockWord;
            bool mKeyOnly = false;
            /// The name of the key (or of the button of the pad) that activates, as the player has it set
            std::string mKeyText;
            /// The player plays with the pad
            bool mPad = false;
        };
        bool hudInfo(const OFWorld::Ptr& target, HudInfo& result);
        /// A thing the compass shows
        struct HudMarker
        {
            enum Kind
            {
                Quest,
                Landmark,
                Enemy,
            };
            Kind mKind = Landmark;
            /// Where in the window of the compass (0 is its left edge, 1 its right edge)
            double mShare = 0.5;
            double mDistance = 0;
            std::string mName;
        };
        /// The things in the window of the compass, for the way the player faces now (`heading` is that, in degrees
        /// clockwise from north)
        std::vector<HudMarker> hudCompass(double& heading);
        /// The text of a note of text, the `&name;` tokens in it filled in the way the messages are (none when the form
        /// is no note or holds no text)
        std::optional<std::string> noteText(FalloutScript::FormId note);
        /// The weapon in the hands of the character, for the animations: its form (0 for bare hands), the model
        /// path of its record and the animation type of its record (-1 for bare hands)
        struct HeldWeapon
        {
            FalloutScript::FormId mWeapon = 0;
            std::string mModel;
            int mAnimationType = -1;
        };
        HeldWeapon heldWeapon(const OFWorld::Ptr& actor);
        /// The character goes for the player
        void fightPlayer(const OFWorld::Ptr& actor);
        /// The character attacks the target with the weapon it holds, as far as its skill allows it to hit. A
        /// weapon that has no ammunition left is put away and the character takes another.
        FireResult attack(const OFWorld::Ptr& attacker, const OFWorld::Ptr& target);
        /// UseWeapon: the character shoots at the target from the place, with the weapon the script names, which it
        /// need not carry and spends no round of. The order is a package that the games make at run time; it is kept
        /// for as long as the attacks are not made, in a cell that is loaded (it is not saved).
        void useWeapon(FalloutScript::FormId actor, const FalloutScript::UseWeaponOrder& order) override;
        /// What the order of a character says it does now. `mActive` is false for a character with no order, one whose
        /// attacks are all made, and one whose target is gone (not in a loaded cell, disabled or dead): the order is
        /// ended then. Main thread.
        struct ScriptedAttack
        {
            bool mActive = false;
            /// A number for each order that scripts gave (a second order replaces the first, with another number)
            unsigned mSerial = 0;
            FalloutScript::UseWeaponOrder mOrder;
            OFWorld::Ptr mTarget;
            /// The reference it shoots from: empty when that is the character itself or is not around
            OFWorld::Ptr mPlace;
            /// The reference it aims at instead of the target (the target is still who the shot hits): empty when the
            /// order names none or it is not around
            OFWorld::Ptr mAim;
        };
        ScriptedAttack scriptedAttack(const OFWorld::Ptr& actor);
        /// The order is dropped, with the reason in the log line (the target is where the shooter can not go to)
        void endScriptedAttack(const OFWorld::Ptr& actor, const char* why);
        bool hasScriptedAttack(const OFWorld::Ptr& actor) const;
        /// An attack of the order, when the weapon is ready again: a hit when the order says so or the character's aim
        /// is good enough. Main thread.
        FireResult scriptedShot(const OFWorld::Ptr& actor, const OFWorld::Ptr& target);
        /// Why `attack` has not fired for a while, in a line for the log: the weapon, its range, the rounds against
        /// what a shot uses, the time until it is ready again
        std::string attackDiagnosis(const OFWorld::Ptr& attacker);
        /// The reference of the actor in hex, as the lines of the log that name a placed character write it (the
        /// editor id of a character is that of its base, which all of its kind share)
        std::string referenceName(const OFWorld::Ptr& actor) const;
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
        /// Whether the actor uses this furniture now
        bool usesFurniture(const OFWorld::Ptr& actor, const OFWorld::Ptr& furniture) const;
        /// The kind of the entry of the furniture the actor uses (the position reference byte of the model: 1 to 9
        /// lying, 10 to 20 sitting), 0 when it uses none
        int furnitureKindOf(const OFWorld::Ptr& actor) const;
        /// How many more characters can use the furniture: the entries of its marker that the record enables and that
        /// no one uses (0 when the model has none)
        int freeFurniturePlaces(const OFWorld::Ptr& furniture);

        /// The AI packages that scripts gave the character, the last given first (those that no content file has a
        /// record of are left out)
        std::vector<const ESM4::AIPackage*> scriptPackages(const OFWorld::Ptr& actor) const;
        /// Enables or disables a reference of a cell that is loaded as the commands of scripts do: the objects that
        /// have it for their enable parent follow it. Must be called on the main thread. False for one that has no
        /// number a script can use (the player, an object that is in no cell), which the caller enables or disables
        /// itself.
        bool setEnabled(const OFWorld::Ptr& ptr, bool enabled);
        /// What the AI of the character keeps that a saved game holds (the packages it has done, the one it follows,
        /// the order to follow somebody), kept while its cell is unloaded and written to the saved game. A character
        /// that has no number a script can use is not kept.
        void rememberAi(const OFWorld::Ptr& actor, FalloutScript::AiState state);
        std::optional<FalloutScript::AiState> savedAi(const OFWorld::Ptr& actor) const;

        /// The games' commands that save the game ask for it, and the save is made in the first frame in which the
        /// interface allows one: one request of each kind is kept, and one is carried out in each frame
        enum class SaveRequest : std::uint8_t
        {
            Autosave,
            ForceSave,
            SystemSave,
        };
        void requestSave(SaveRequest request);
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
        /// The character that the player is in a conversation with, empty when there is no conversation open. For the
        /// thread of the frame (the camera turns to the speaker).
        OFWorld::Ptr conversationSpeaker();
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
            /// The name of the message and its words, apart (mText is both); the box of the game has the name as its
            /// title and the words under it
            std::string mTitle;
            std::string mBody;
            /// A message with no button of its own: the game's box shows no button and closes itself after `mTime`
            /// seconds (the record's display time); `mButtons` then has the one that closes it
            bool mTimed = false;
            double mTime = 0;
            /// A script of Lua draws this box (the button it picks is the answer); false when the window manager's own
            /// window is open for it, which a script must not draw a second time
            bool mLua = false;
        };
        /// The box that is open, if any (any thread)
        std::optional<OpenMessage> openMessage() const;
        /// A message box that a script of Lua draws is open: it is modal, and the actions of the keys and buttons of
        /// the game (Escape, which would close the screen under it) are not carried out while it is (any thread)
        bool luaMessageOpen() const;
        /// A screen of Lua has a question open in a box of its own (I.FalloutMessage.ask, which the engine knows
        /// nothing of otherwise): it is modal as a message box is. The script says it every frame while the question is
        /// open, and a question that has not been said for a second is gone (a script that stopped must not leave the
        /// keys dead). Any thread.
        void setLuaQuestion(bool open);
        /// A screen that Lua draws (the Vigor Tester, the screen of the face) says every frame that Escape opens the pause menu
        /// over it, as the executables' menus do, instead of leaving the screen; a script that stopped saying it for a second
        /// no longer claims it
        void setLuaEscapeClaim(bool claimed);
        bool luaEscapeClaimed() const;
        /// A script of Lua draws the message boxes (message_menu.xml) when `drawn` is true: the box of the window
        /// manager is not made then, and the button that Lua picks (pressMessageButton) is the answer. When it is
        /// false again (the file is not there, or the script cannot show a box) a box that is open is made as the
        /// window manager's in the next frame, and the next ones are too. The script says it every frame: a claim that
        /// is a second old is no longer one. Any thread.
        void setMessagesDrawnByLua(bool drawn);
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
        /// The name, the description and the picture that the data has for an actor value (an attribute or a skill) by
        /// its editor id; the name given when it has none. The picture is the `ICON` of the record: the character
        /// making screens of both executables show it beside the description.
        struct ActorValueText
        {
            std::string mName;
            std::string mDescription;
            std::string mIcon;
            /// The record's own flag (0x2000) that keeps the actor value out of the list of the level-up screen
            bool mLevelUpSkip = false;
        };
        ActorValueText actorValueText(std::string_view editorId, std::string_view fallback) const;
        /// The name of an attribute or a skill by its actor value number; empty for any other actor value
        std::string actorValueName(int number) const;
        /// The races the player can be (playable and not children), by their form ids
        struct ChargenRace
        {
            FalloutScript::FormId mId = 0;
            std::string mName;
        };
        std::vector<ChargenRace> playableRaces() const;

        // ---- The look of the player, which the screen of the sex, race and face (RaceSexMenu) edits ----
        /// A row of a list of that screen
        struct LookChoice
        {
            FalloutScript::FormId mId = 0;
            std::string mName;
            bool mSelected = false;
        };
        /// What the lists of the screen hold for the sex and race the player has now
        struct LookCatalog
        {
            std::vector<LookChoice> mRaces;
            std::vector<LookChoice> mHairs;
            std::vector<LookChoice> mEyes;
            /// The head parts a man can wear (the beards)
            std::vector<LookChoice> mFacialHair;
            /// The characters of the data that the sex and race have as faces to start from (the preset slider's range)
            std::vector<LookChoice> mPresets;
        };
        /// How the player looks now: what the screen picked, and else what the record of the player says
        FalloutScript::FaceLook playerLook();
        /// What the record of the player says it looks like, with the sex and race a script or a screen set
        FalloutScript::FaceLook recordLook();
        LookCatalog lookCatalog();
        /// The sex or race clicked: the head parts go, the first face of the new kind is put on and a hair or eyes that
        /// the kind does not have are changed (FalloutNV.exe 0x7B5180)
        void lookSetSex(bool female);
        void lookSetRace(FalloutScript::FormId race);
        void lookSetHair(FalloutScript::FormId hair);
        void lookSetEyes(FalloutScript::FormId eyes);
        void lookSetHairColour(int red, int green, int blue);
        /// A head part clicked: it goes on, or comes off when it was on
        void lookToggleFacialHair(FalloutScript::FormId part);
        /// The preset face of that number (1 on): its hair, eyes, colour, head parts and face numbers
        bool lookSetPreset(int number);
        /// New random numbers for the shape and tone of the face
        void lookRandomize();
        /// The whole number a slider shows for the control, and moving the slider to one
        int lookSlider(FalloutScript::FaceKind kind, int control);
        void lookSetSlider(FalloutScript::FaceKind kind, int control, int value);
        /// The age slider (1 to 10) moved to a number: the whole face along the age directions of the face file. False
        /// when the file has none.
        bool lookSetAge(int slider);
        bool lookHasAge();
        /// How many controls of facegen\si.ctl the screen has (none when the file was not read)
        int lookControlCount(FalloutScript::FaceKind kind);
        /// A character that has the look of the player, for the head the screen shows: the record of a character of the
        /// data that has the sex and race (the first preset, which has a skeleton and a body the way a character of the
        /// world does) with the look written over it. `mTemplate` is that record's id.
        struct LookPreview
        {
            ESM::RefId mTemplate;
            ESM4::Npc mRecord;
        };
        bool lookPreview(LookPreview& preview);
        /// What the body of the player is made from in third person: the look as `lookPreview` makes it, and the armor
        /// the player wears (the pieces of the record that the player's items stand for). False when the player has no
        /// look to draw (no plugin of Fallout is loaded).
        bool playerBody(LookPreview& preview, std::vector<const ESM4::Armor*>& armor);
        /// A number that changes when what `playerBody` gives changes (the sex, race or look, armor put on or taken
        /// off), cheap enough to ask every frame
        std::uint64_t playerBodyRevision();
        /// The traits of the data (New Vegas), by name; the picture is the `ICON` of the perk, which the trait menu of
        /// the executable shows beside the description
        struct ChargenTrait
        {
            FalloutScript::FormId mId = 0;
            std::string mName;
            std::string mDescription;
            std::string mIcon;
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
        int currentPackageType(FalloutScript::FormId actor) override;
        bool currentPackageOffersServices(FalloutScript::FormId actor) override;

        bool placement(FalloutScript::FormId reference, FalloutScript::Placement& result) override;
        void moveTo(FalloutScript::FormId reference, FalloutScript::FormId target,
            const std::array<double, 3>& offset) override;
        void setPosition(FalloutScript::FormId reference, int axis, double value) override;
        void setAngle(FalloutScript::FormId reference, int axis, double radians) override;
        void setScale(FalloutScript::FormId reference, double scale) override;
        /// How large the reference is: as the world has it when its cell is loaded, else as its record says
        double scaleOf(FalloutScript::FormId reference);

        bool markerFlags(FalloutScript::FormId marker, std::uint8_t& flags) override;
        /// Tells the player that a map marker was uncovered
        void markerShown(FalloutScript::FormId marker);
        /// The player came near the place and found it: the display says so (FalloutNV.exe 0x76B960)
        void markerDiscovered(FalloutScript::FormId marker);
        void playSound(FalloutScript::FormId sound, FalloutScript::FormId reference) override;
        bool isSoundPlaying(FalloutScript::FormId sound, FalloutScript::FormId reference) override;
        bool isMusicPlaying() override;
        bool playMusic(FalloutScript::FormId track) override;
        void forceWeather(FalloutScript::FormId weather, bool hold) override;
        void releaseWeather() override;
        double imageSpaceDuration(FalloutScript::FormId modifier) override;
        void achievement(int id) override;
        FalloutScript::FormId parentCell(FalloutScript::FormId reference) override;
        FalloutScript::FormId parentWorldspace(FalloutScript::FormId reference) override;
        bool gameRestarted(FalloutScript::FormId script) override;
        bool gameLoaded(FalloutScript::FormId script) override;
        bool isActor(FalloutScript::FormId reference) override;
        FalloutScript::FormId playerBase() override;
        FalloutScript::FormId ownerForm(FalloutScript::FormId form) override;
        FalloutScript::FormId recordOwner(FalloutScript::FormId reference) override;
        FalloutScript::FormId recordCellOwner(FalloutScript::FormId cell) override;
        bool recordCellPublic(FalloutScript::FormId cell) override;
        bool recordGhost(FalloutScript::FormId actor) override;
        bool reputation(FalloutScript::FormId reputation, double& cap, std::string& name) override;
        double setting(std::string_view name, double fallback) override;
        /// Tells the player that a reputation went up
        void reputationChanged(const FalloutScript::Reputation::Bump& bump);

        bool inInterior(FalloutScript::FormId reference) override;
        bool hasLoaded3D(FalloutScript::FormId reference) override;
        FalloutScript::SkyWeather sky() override;
        bool playerSleeping() override { return mPlayerSleeping; }
        /// The engine has no crime system: nobody sees a crime, so every count is the 0 that a clean player has in the
        /// executables (the commands that ask are the greeting lines of the characters)
        int majorCrimeCount(FalloutScript::FormId) override { return 0; }
        bool playerMovingIntoNewSpace() override { return mPlayerMoving; }
        bool inZone(FalloutScript::FormId reference, FalloutScript::FormId zone) override;
        bool talksThroughActivator(FalloutScript::FormId actor) override;

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
        bool playIdle(FalloutScript::FormId reference, std::string_view name, FalloutScript::FormId idle) override;
        /// The idle record named by its editor id, or the form when the name is empty; null when there is none
        const ESM4::IdleAnimation* findIdle(std::string_view name, FalloutScript::FormId form);
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
        /// The files that a sound record (SOUN) plays: its one file, or the files of the folder that it names (the game
        /// plays one of them); none when the data does not have it. Looked up once.
        std::vector<std::string> soundFilesOf(FalloutScript::FormId sound);
        /// The sound record that has this editor id (not case sensitive), 0 when there is none: how the menu files and
        /// the code of the games name their sounds
        FalloutScript::FormId soundByEditorId(std::string_view name);

    public:
        /// Play the sound the interface names by its editor id ("UIMenuOK"), in 2D, as the games' interface does; false
        /// when there is no such sound record or it has no file
        bool playInterfaceSound(std::string_view name);

        /// The note that the games print on the HUD line when the player quick saves or quick loads (the words of
        /// sQuickSaving and sQuickLoading, 2 seconds, the neutral face) and when that cannot be done (sCantQuickSave
        /// and sCantQuickLoad, the sad face), Mac round 12BQ A1. A save made by the autosave has none.
        void quickNote(bool save, bool denied);

        /// The player is about to be moved through a door or by a fast travel (the two moves that the executables
        /// flag; a move that a script makes, and a walk from one exterior cell to the next, do not count). The
        /// executables hold the flag for the whole move, a loading screen long; here the move is done within a frame,
        /// so the scripts of the next pass still see the flag on, and it goes off when they have run.
        void playerMoveBegins()
        {
            mPlayerMoving = true;
            mPlayerMoveDone = false;
        }
        /// The move is done; the flag goes off after the scripts of the next pass
        void playerMoveEnds() { mPlayerMoveDone = true; }

    private:
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
        /// Tells the scripts of the characters that were told to say what nobody could say that they are done
        void raiseNothingSaid();

        void forEachTopic(const std::function<void(const ESM4::Dialogue&)>& function) const override;
        void forEachInfo(const std::function<void(const ESM4::DialogInfo&)>& function) const override;

        bool initialItems(
            FalloutScript::FormId holder, std::vector<std::pair<FalloutScript::FormId, int>>& items) override;
        bool initialFactions(
            FalloutScript::FormId actor, std::vector<std::pair<FalloutScript::FormId, int>>& factions) override;
        bool initiallyDead(FalloutScript::FormId actor, bool& dead) override;
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
        /// What a weapon does with a blow, for the attacker that holds it; the hands for a weapon that is none. The
        /// condition of the weapon is the one of the copy in the hands (`condition`, in tenths of a percent, names another)
        FalloutScript::WeaponDamage describeWeapon(
            FalloutScript::FormId weapon, FalloutScript::FormId attacker, int condition = -1, int mods = 0);
        /// The damage of one shot (one pellet of a shotgun) of the weapon in the hands of the player as the card of
        /// the item tells it: the damage of the record with the skill, the condition (of the copy named, in tenths of a
        /// percent) and the perks of the player
        double weaponCardDamage(
            FalloutScript::FormId weapon, int condition = FalloutScript::ActorState::fullCondition, int mods = 0);
        /// What wears items out, from the game's settings
        const FalloutScript::Wear::Settings& wearSettings();
        /// What repairs go by, from the game's settings
        const FalloutScript::Repair::Settings& repairSettings();
        /// Whether the weapon in the hands of the screen was on when it opened, to be on again when it closes (under
        /// mRepairMutex)
        bool mModsWeaponWasOn = false;
        /// What the copy of an item does at a condition (the damage of a weapon, the rating and threshold of armor)
        RepairStat repairStatAt(FalloutScript::FormId item, int condition, int mods = 0);
        /// Whether an item can be used up to repair another (its form is on the repair list of the other, or the perk lets
        /// weapons of one kind repair each other), and whether the record of the item is for the player
        bool repairsWith(FalloutScript::FormId item, FalloutScript::FormId other);
        bool isQuestItem(FalloutScript::FormId item);
        /// The effects of the ammunition of one kind (the kinds of AMEF: 0 damage, 1 resistance, 2 threshold, 3 spread, 4
        /// condition, 5 fatigue) applied to a number, in the order the ammunition lists them
        double applyAmmoEffects(FalloutScript::FormId ammo, std::uint32_t kind, double value);
        /// A weapon that the player fires loses health (a shot, a swing), an armor piece that is hit loses health; the
        /// copy that is on takes it, and the player is told when it is nearly gone and when it is
        void wearWeaponOfShot(FalloutScript::FormId attacker, const WeaponStats& stats);
        void wearArmorOfHit(FalloutScript::FormId victim, int limb, double damage, double resistance, double threshold,
            double remaining, double scale);
        /// An item that is on lost health (the condition before and after, in tenths of a percent): the player is told when
        /// it is below a quarter and when it is gone, and a weapon that broke is put away
        void itemWorn(FalloutScript::FormId holder, FalloutScript::FormId item, int before, int after);
        /// Whether the weapon takes the ammunition (the one of its record, or one of its list of them)
        bool weaponTakesAmmo(const ESM4::Weapon& weapon, FalloutScript::FormId ammo);
        /// The ammunition a weapon in the hands of the holder shoots: the one chosen when the weapon takes it, else
        /// the one of the record, else the first of its list that the holder carries (the first of the list when it
        /// carries none), 0 when the weapon names none
        FalloutScript::FormId ammoOfWeapon(const ESM4::Weapon& weapon, FalloutScript::FormId holder);
        /// What one of an item weighs for the player: New Vegas lightens small things (Pack Rat) as the card of the
        /// item and the total of the Pip-Boy tell it
        double carriedWeight(double unit, bool packRat);
        /// Whether the player has nothing a stimpak (or in New Vegas a doctor's bag) would mend
        bool atFullHealthFor(FalloutScript::FormId item);
        const FalloutScript::DamageSettings& damageSettings();
        /// Runs the OnHit, OnHitWith, OnDeath and OnCombatEnd blocks for what happened in the fights since the last
        /// frame, wakes the essential characters that were down long enough, and lets the time of the fights pass
        void raiseCombatEvents(float frametime);
        /// Whether the victim lets this blow pass: the player struck someone on the side of its factions, and the blows
        /// the victim has taken from the player are no more than it allows. The blow is counted for GetFriendHit.
        bool toleratesBlow(FalloutScript::FormId attacker, FalloutScript::FormId victim);
        /// A character whose health is gone (or that a script kills): an essential one is put out of its fights and
        /// lies unconscious until it gets up, or has its health back to a share of the most if it is down already
        /// (true), any other one dies (false, nothing is done). Any thread.
        bool fallsInsteadOfDying(FalloutScript::FormId victim);
        bool initialValues(FalloutScript::FormId actor, std::vector<std::pair<int, double>>& values) override;
        bool levelOf(FalloutScript::FormId actor, int playerLevel, int& level) override;
        std::optional<double> derivedValue(
            FalloutScript::FormId actor, int value, const std::function<double(int)>& permanent) override;
        /// The settings of the files for the actor values that are worked out, read when the first is asked for
        const FalloutScript::DerivedStatSettings& derivedSettings();
        /// How the games move a character from the entry of a marker to where it stays on the furniture, from the
        /// settings of the files (read when first asked for)
        const FalloutScript::FurnitureDeltas& furnitureDeltas();
        /// Where a character that began to use a piece of furniture at the entry stays on it
        FalloutScript::FurnitureSeat settledOn(
            const FalloutScript::FurnitureSeat& entry, const OFWorld::Ptr& furniture);
        /// A character that gets up from a piece of furniture stands where it began, at the entry of the marker
        void standUpFrom(FalloutScript::FormId user, const OFWorld::Ptr& actor, int entry,
            FalloutScript::FormId furniture, const osg::Vec3f& sat);
        bool initialLock(FalloutScript::FormId reference, bool& locked, int& level) override;
        bool formListEntries(FalloutScript::FormId list, std::vector<FalloutScript::FormId>& entries) override;
        bool perk(FalloutScript::FormId perk, FalloutScript::PerkInfo& info) override;
        std::vector<FalloutScript::FormId> perks() override;
        bool abilityEffects(FalloutScript::FormId ability, std::vector<FalloutScript::AbilityEffect>& effects) override;
        bool consumable(FalloutScript::FormId item, FalloutScript::Consumable& result) override;
        bool spell(FalloutScript::FormId spell, FalloutScript::SpellInfo& info) override;
        /// What one effect of an item or a spell is, false for one that is of no use
        bool effectOf(const ESM4::EffectEntry& entry, FalloutScript::ConsumableEffect& made);
        /// The character takes the effects of a spell it cast on itself
        void castSpell(FalloutScript::FormId actor, FalloutScript::FormId spell,
            const std::vector<FalloutScript::SpellEffect>& kept);
        bool book(FalloutScript::FormId item, FalloutScript::BookInfo& result) override;
        std::vector<FalloutScript::FormId> recipesOf(FalloutScript::FormId category) override;
        bool recipe(FalloutScript::FormId id, FalloutScript::Recipe& result) override;
        bool allowed(FalloutScript::FormId recipe, FalloutScript::FormId crafter) override;
        bool bodyParts(FalloutScript::FormId actor, std::vector<FalloutScript::BodyPart>& parts) override;
        /// What the weapon says that V.A.T.S. needs, for the weapon a character holds (the hands when none)
        FalloutScript::Vats::Weapon vatsWeapon(FalloutScript::FormId attacker);
        const FalloutScript::Vats::Rules& vatsRules();
        /// Brings the rules of levelling up from the settings of the files, once they can be read
        void ensureLevellingRules();
        /// Hands out the points of the levels the player reached, brings the perks up to date and lets the effects of
        /// the items that last go on
        void updateProgress(float frametime);
        /// The experience of a kill: the player did enough of the damage that killed the character
        void awardKillExperience(FalloutScript::FormId victim);
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
        /// The record id of a form number, empty for 0
        static ESM::RefId refIdOf(FalloutScript::FormId id);
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
        /// Takes in an object that was placed while the game runs: scripts name it by `id`, which the world knows by
        /// the number it gave the object. It runs its script unless its cell is not loaded, where the cell starts it.
        void adoptObject(FalloutScript::FormId id, const OFWorld::Ptr& placed, bool run);
        /// Does what a loaded game said about references and made objects, at the start of a frame: the objects are
        /// made again, the references that were deleted or scaled are so again once their cell is loaded
        void applySaved();
        /// A reference of a cell that was just loaded: its Lua scripts are those of the loaded game at once, what else
        /// the game said about it waits for the next frame. True for one that the game had deleted.
        bool savedReferenceLoaded(FalloutScript::FormId reference, const OFWorld::Ptr& ptr);
        /// What scripts asked of a reference while it was out of the loaded cells (or what a loaded game says they
        /// had asked) is done to it at the start of the next frame
        void takeSavedChanges(FalloutScript::FormId reference);
        /// The volume of a trigger was just put in: the actors a loaded game said were inside are so, and the events
        /// it had not raised yet wait again
        void restoreSavedTrigger(FalloutScript::FormId trigger);
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
        /// Whether the player is in the region, as the condition PlayerInRegion and the command IsPlayerInRegion ask:
        /// the region is one that the exterior cell of the player lists (its XCLR) and the player stands inside one of
        /// the areas of the region. Nothing in a cell inside. (The games work this out once, when the player enters a
        /// cell; this works it out where the player is when asked.)
        bool playerInRegion(FalloutScript::FormId region);
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
        /// Whether the object is disabled now, not counting what another thread has asked for and is not done yet
        bool isDisabledNow(FalloutScript::FormId reference);
        /// Enables or disables the object alone, where it is or when its cell is loaded
        void applyDisabledAlone(FalloutScript::FormId reference, bool disabled);
        /// Who has whom for an enable parent, read from the records the first time it is asked for
        const FalloutScript::EnableParents& enableParents();
        /// (`kind` is the face a message without a picture gets: 0 neutral, 1 very happy, 2 sad; the executable's
        /// callers choose it, Mac round 12BA E)
        // `seconds`: the display time of the message the text is from (its TNAM), 0 for the time that the length of the
        // text gives
        void tellPlayer(const std::string& text, int kind = 0, float seconds = 0.f);
        /// A box with the text and no button of its own: it shows for `seconds` (0 until the player closes it)
        void showNotice(const std::string& text, double seconds);

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
            std::string mTitle;
            std::string mBody;
            bool mTimed = false;
            double mTime = 0;
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
        FalloutScript::Reputation mReputation;
        FalloutScript::ObjectState mObjectState;
        FalloutScript::MapMarkers mMapMarkers;
        FalloutScript::HotKeys mHotKeys;
        FalloutScript::Presentation mPresentation;
        FalloutScript::Switches mSwitches;
        /// Which script forms have been told that the program was started or that a game was started or loaded
        /// (GetGameRestarted, GetGameLoaded): the first set lasts as long as the program, the second until the next
        /// game is started or loaded
        std::mutex mSessionMutex;
        std::set<FalloutScript::FormId> mRestartAsked;
        std::set<FalloutScript::FormId> mLoadAsked;
        FalloutScript::FormLists mFormLists;
        FalloutScript::PlayerProgress mProgress;
        FalloutScript::PerkEngine mPerks;
        FalloutScript::ItemUse mItemUse;
        FalloutScript::Magic mMagic;
        FalloutScript::Crafting mCrafting;
        FalloutScript::Limbs mLimbs;
        std::atomic<bool> mVatsOpen{ false };
        /// The seconds of breath that the player has left (-1 until the first frame has filled it) and whether its head
        /// is under the water
        std::atomic<double> mBreathLeft{ -1.0 };
        std::atomic<bool> mHeadUnder{ false };
        std::optional<FalloutScript::Vats::Rules> mVatsRules;
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
        /// The speakers and topics that sayTopic has logged, with whether the last call found nothing to say. The first
        /// call is logged, and each later one that finds a line where the last found none (or the other way round),
        /// under mDialogueMutex
        std::map<std::pair<FalloutScript::FormId, FalloutScript::FormId>, bool> mSayLogged;
        /// The speakers and topics that a script told to say something nobody could say, whose scripts are told at the
        /// start of the next frame that they are done (Begin SayToDone Topic), under mDialogueMutex
        std::vector<std::pair<FalloutScript::FormId, FalloutScript::FormId>> mNothingSaid;
        FalloutScript::FormId mPlayer;
        FalloutScript::Game mGame;
        /// The characters whose perks have been brought up to date (to take back the effects of those they lost)
        std::set<FalloutScript::FormId> mPerkOwners;
        /// The rules of levelling up are read from the settings of the files once, and whoever asks for them while
        /// they are being read waits for that
        std::once_flag mLevellingRulesOnce;
        /// The levels that were reached and not shown yet, and whether the screen of a level is open (under the mutex)
        std::mutex mLevelUpMutex;
        std::deque<FalloutScript::PlayerProgress::LevelUp> mLevelUps;
        bool mLevelUpOpen = false;
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
        /// The box that is open is drawn by Lua, which has not made one of the window manager's (the thread of the
        /// frame only); `mLuaPressed` is the button it picked (counted from 0), -1 for none yet
        bool mOpenByLua = false;
        MessageBox mOpenBox;
        std::atomic<bool> mMessagesDrawnByLua{ false };
        /// When Lua last said it draws them (milliseconds of the steady clock): a script that has stopped (the game
        /// was left, another was loaded) no longer draws, and the next box is the window manager's
        std::atomic<std::int64_t> mMessagesClaimedAt{ 0 };
        /// How many times it said so since it last gave them back (1 is the claim of the script when it was loaded)
        std::atomic<int> mMessageClaims{ 0 };
        /// The script says it draws them and said it within `leaseMilliseconds` (any thread)
        bool luaClaimIsFresh(std::int64_t leaseMilliseconds) const;
        std::atomic<int> mLuaPressed{ -1 };
        /// When a script of Lua last said it has a question open (milliseconds of the steady clock), 0 for none
        std::atomic<std::int64_t> mLuaQuestionAt{ 0 };
        /// When Lua last said that Escape opens the pause menu over its screen (milliseconds of the steady clock), 0 for none
        std::atomic<std::int64_t> mLuaEscapeAt{ 0 };
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
        /// Whether the player sleeps (IsPCSleeping)
        std::atomic<bool> mPlayerSleeping{ false };
        /// Whether the player is being moved into another space (IsPlayerMovingIntoNewSpace), and whether the move is
        /// over
        std::atomic<bool> mPlayerMoving{ false };
        std::atomic<bool> mPlayerMoveDone{ false };
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
        /// The objects that are loaded and disabled: their OnLoad blocks wait until they are enabled (their GameMode
        /// blocks do not wait), and those enabled since, whose OnLoad blocks run at the start of the next frame
        std::set<FalloutScript::FormId> mLoadWhenEnabled;
        std::vector<FalloutScript::FormId> mPendingEnabledLoad;
        /// Where scripts put objects: those that are waiting for the start of the next frame (the world is not changed
        /// in the middle of a script), and those whose cell was not loaded at the time, which hold until it is
        std::map<FalloutScript::FormId, Move> mPendingMoves;
        std::map<FalloutScript::FormId, Move> mRelocations;
        /// The scale scripts gave to objects that have not been given it yet (their cell was not loaded, or the world
        /// is changed at the start of the next frame): they get it when they can, and scripts see it from the call on,
        /// whichever thread they run on
        std::map<FalloutScript::FormId, double> mScales;
        std::mutex mScalesMutex;
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
        /// The attacks that scripts told characters to make (UseWeapon), and how many each has made. Main thread.
        struct ScriptedOrder
        {
            FalloutScript::UseWeaponOrder mOrder;
            int mMade = 0;
            unsigned mSerial = 0;
        };
        std::map<FalloutScript::FormId, ScriptedOrder> mScriptedAttacks;
        unsigned mScriptedSerial = 0;
        FalloutScript::AiMemory mAi;
        /// The kinds of save that scripts asked for and that were not made yet, one bit for each
        std::atomic<unsigned> mSaveRequests{ 0 };
        /// The state of a loaded game that is put back when the cells and the frame it needs come
        std::unique_ptr<PendingSave> mPendingSave;
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
        std::optional<FalloutScript::Wear::Settings> mWearSettings;
        std::optional<FalloutScript::Repair::Settings> mRepairSettings;
        /// The controls and the age of facegen\si.ctl, read when the face screen first needs them (without the controls
        /// when the file is not there or is not the layout)
        const FalloutScript::FaceControlFile& faceControls();
        std::mutex mFaceControlsMutex;
        std::optional<FalloutScript::FaceControlFile> mFaceControls;
        /// The numbers of the generator of the random faces
        std::mt19937 mLookRandom{ std::random_device{}() };
        std::optional<FalloutScript::FurnitureDeltas> mFurnitureDeltas;
        /// The settings of the files by their names in lower case, read when the first of them is asked for (the
        /// caller holds mDerivedSettingsMutex)
        const std::map<std::string, double>& gameSettingValues();
        /// The number a game setting has in the files (the names are not case sensitive), else the fallback
        double gameSetting(std::string_view name, double fallback);
        std::optional<std::map<std::string, double>> mGameSettingValues;
        /// The settings that are texts, by name (under mDerivedSettingsMutex as well)
        std::optional<std::map<std::string, std::string>> mGameSettingTexts;
        /// The default texts of the settings that the executables of the games build, by name in lower case (under
        /// mDerivedSettingsMutex as well)
        std::map<std::string, std::string> mExecutableTexts;
        /// What the player has open to take things from, under mLootMutex
        std::mutex mLootMutex;
        std::optional<Loot> mLoot;
        int mLootSerial = 0;
        /// What the player has won in each casino
        FalloutScript::Casino::Ledger mCasinoLedger;
        /// The game of a casino that the player has open, under mCasinoMutex
        struct CasinoSession
        {
            CasinoScreen mScreen;
            FalloutScript::FormId mCurrency = 0;
            int mMaxWinnings = 0;
            /// The quest the casino starts when the table closes after the winnings reached a new quarter (CSNO)
            FalloutScript::FormId mWinningsQuest = 0;
            bool mNewLevel = false;
            std::optional<FalloutScript::Casino::Roulette> mRoulette;
            std::optional<FalloutScript::Casino::Blackjack> mBlackjack;
            std::optional<FalloutScript::Casino::SlotMachine> mSlots;
            /// The result of the round is in the winnings of the casino
            bool mRoundSettled = true;
        };
        std::mutex mCasinoMutex;
        std::optional<CasinoSession> mCasino;
        int mCasinoSerial = 0;
        std::mt19937 mCasinoRandom{ std::random_device()() };
        /// The match of Caravan that the player has open, under mCaravanMutex
        struct CaravanSession
        {
            CaravanStage mStage = CaravanStage::Bet;
            int mSerial = 0;
            FalloutScript::FormId mOpponent = 0;
            std::string mOpponentName;
            FalloutScript::FormId mDeck = 0;
            int mAiType = 0;
            std::optional<FalloutScript::Casino::CaravanStake> mStake;
            struct Owned
            {
                FalloutScript::Casino::Caravan::Card mCard;
                std::string mName;
                bool mInDeck = false;
            };
            std::vector<Owned> mOwned;
            std::vector<FalloutScript::Casino::Caravan::Card> mOpponentCards;
            std::optional<FalloutScript::Casino::Caravan> mGame;
            bool mWon = false;
            bool mForfeited = false;
        };
        std::mutex mCaravanMutex;
        std::optional<CaravanSession> mCaravan;
        FalloutScript::Casino::CaravanRecord mCaravanRecord;
        /// The face texture that the record of a card names (empty for a form that is not a card)
        std::string caravanFace(FalloutScript::FormId card) const;
        int mCaravanSerial = 0;
        std::mt19937 mCaravanRandom{ std::random_device()() };
        /// The match is over: the pot is paid, in caravan money first and then in caps, by the side that lost
        /// (0x74CC70)
        void payCaravan(CaravanSession& session);
        /// The lock, the terminal and the rest that the player has open, under mInteractionMutex
        std::mutex mInteractionMutex;
        std::optional<LockScreen> mLockScreen;
        int mLockSerial = 0;
        /// The locks that gave their experience, and those that forcing broke (kept for the run, not in saves)
        std::set<FalloutScript::FormId> mLockExperienceGiven;
        std::set<FalloutScript::FormId> mLockBroken;
        struct TerminalState
        {
            FalloutScript::FormId mReference = 0;
            /// The terminals (their records) the player went through; the menu that is shown is the last
            std::vector<FalloutScript::FormId> mPath;
            /// The result of the line that was chosen last, and the text of a note a line showed
            std::string mResult;
            int mResultSerial = 0;
            std::string mNote;
            /// The game of hacking, while the terminal is closed
            std::unique_ptr<FalloutScript::Hacking::Puzzle> mPuzzle;
            std::vector<HackGuess> mGuesses;
            /// The attempts were spent, or the terminal was locked out by an earlier visit
            bool mLockedOut = false;
            int mHackLevel = 0;
            /// The bit of the terminal that makes the words of its game of hacking one letter longer
            int mHackParity = 0;
            int mSerial = 0;
        };
        /// The game of hacking for a terminal of that hacking level: the words of the game's dictionary (the file is
        /// read once), as many as the Science of the player allows
        std::unique_ptr<FalloutScript::Hacking::Puzzle> newPuzzle(int level, int parity);
        const FalloutScript::Hacking::Dictionary& hackingDictionary();
        /// A guess of the game of hacking has come out: the terminal opens for a right one and locks for the last wrong
        /// one; mInteractionMutex held
        void hackGuessed(TerminalState& state, bool correct);
        std::mutex mHackingDictionaryMutex;
        std::optional<FalloutScript::Hacking::Dictionary> mHackingDictionary;
        std::optional<TerminalState> mTerminal;
        int mTerminalSerial = 0;
        /// How many times each terminal locked the player out (the low 7 bits of the byte the executable keeps for it:
        /// 1 is a second try for a perk, 2 or more is shut until a script opens it). Not saved yet. Under
        /// mInteractionMutex.
        std::unordered_map<FalloutScript::FormId, int> mTerminalLockouts;
        /// The seconds that are left before a board can be laid out again, counted down in every frame
        double mHackGate = 0;
        /// Closes the screen of the terminal and keeps what is left of its lockout; mInteractionMutex held
        void leaveTerminal();
        struct RestState
        {
            bool mOpen = false;
            bool mSleeping = false;
            FalloutScript::FormId mBed = 0;
            int mSerial = 0;
            bool mActive = false;
            double mTotal = 0;
            double mDone = 0;
            std::string mStopped;
            int mEnded = 0;
            /// The player stopped it: it ends where it is, in the next frame
            bool mFinish = false;
            /// The screen of a sleep was made dark (it is made light again when the sleep ends)
            bool mFaded = false;
            /// The seconds that the sleeper still lies in the dark before the time starts to pass
            double mFadeHold = 0;
        };
        RestState mRest;
        /// A door or container whose lock was just opened, and the seconds before it is activated again
        struct Reactivation
        {
            FalloutScript::FormId mReference = 0;
            double mDelay = 0;
        };
        std::optional<Reactivation> mReactivate;
        float mDiscoveryTimer = 0;
        /// The hours of waiting that pass for each second of play while the player waits or sleeps
        static constexpr double sRestHoursPerSecond = 4.0;
        /// The seconds that the screen takes to go dark when the player lies down to sleep, and to get light again when
        /// it gets up (a stand-in: the games' own times are not read)
        static constexpr float sRestFadeSeconds = 1.0f;
        /// Moves the game of hacking, waiting and sleeping on; every frame, even while a screen stops the world
        void updateInteraction(float frametime);
        /// Makes the save that a script asked for, when the interface allows one
        void runSaveRequests();
        void updateRest(float frametime);
        /// The breath of the player: it runs down while the head is under the water and a player that is out of it
        /// loses health (FalloutNV.exe 0x889930, Mac rounds 12BI and 12BM). It is full again as soon as the head is
        /// out.
        void updateBreath(float frametime);
        /// The seconds that the player holds its breath
        double mostBreath();
        /// Whether the furniture is a bed that the actor would lie on
        bool isBedFor(const OFWorld::Ptr& actor, const OFWorld::Ptr& furniture);
        /// The map markers the player walks up to are found
        void discoverMarkers(float frametime);
        /// Whether the reference is in a cell of the open world (the cell of an exterior has an id as well, so the id
        /// does not say)
        bool inOpenWorld(FalloutScript::FormId reference);
        /// The text of a game setting, the fallback when the files have none
        std::string wordOf(std::string_view setting, std::string_view fallback);
        /// The text of the setting that names why the player can not wait, sleep or travel (with the spelling of the
        /// files for hostile characters tried as well), empty when the files have none
        std::string refusalWord(const std::string& setting);
        /// Why the player can not wait or sleep now, empty when it can
        std::string restRefusalText(bool sleeping, FalloutScript::FormId bed);
        /// The text of a terminal with the tokens of it filled in
        std::string terminalFormat(const std::string& text);
        /// The things that make fast travel and waiting impossible, as far as the host can tell
        bool hostilesNearPlayer();
        /// Whether the player is in the air: not on the ground, not in water, not held up by flight
        bool playerInAir();
        /// Whether two cells are the same place for eyes, for the compass and for a fight: one cell, or two exterior
        /// cells of one worldspace (an interior is a cell of its own)
        static bool sharesSpace(const OFWorld::CellStore* a, const OFWorld::CellStore* b);
        /// The bobby pin, as a form of the file the player is in (see masterForm)
        static constexpr FalloutScript::FormId sBobbyPin = 0x0000000A;
        /// The menu of recipes the player has open, under mRecipeMutex
        std::mutex mRecipeMutex;
        std::optional<RecipeMenu> mRecipeMenu;
        int mRecipeSerial = 0;
        /// The barter menu the player has open, under mBarterMutex
        std::mutex mBarterMutex;
        std::optional<BarterMenu> mBarter;
        int mBarterSerial = 0;
        /// The repair services the player has open, under mRepairMutex
        std::mutex mRepairMutex;
        std::optional<RepairServices> mRepairServices;
        int mRepairSerial = 0;
        /// The companion wheel the player has open, under mCompanionMutex
        std::mutex mCompanionMutex;
        std::optional<CompanionWheel> mCompanion;
        int mCompanionSerial = 0;
        /// The merchant containers that scripts gave to vendors (0: taken away), under mBarterMutex
        std::map<FalloutScript::FormId, FalloutScript::FormId> mMerchantContainers;
        /// Where a vendor's goods are, in the order they are taken from: its container, then the vendor itself
        std::vector<FalloutScript::FormId> barterSources(FalloutScript::FormId vendor);
        /// How many of an item the vendor sells (its container's and its own, not what it wears), and the move of
        /// that many to a holder, taken from the container first; the number that moved
        int barterStock(FalloutScript::FormId vendor, FalloutScript::FormId item, int condition = -1);
        int barterMove(FalloutScript::FormId vendor, FalloutScript::FormId to, FalloutScript::FormId item, int count,
            int condition = -1);
        /// A pile in which every pick names its condition: a pick that named none takes the best copies there are
        /// (what the vendor sells, or what the player carries), the next best for what is left
        BarterPiles barterNamed(const BarterPiles& pile, bool buying);
        /// The value of an item record (0 for a form that is no item), the settings the prices go by and the price of
        /// one of an item for the player buying (or selling)
        int itemValue(FalloutScript::FormId item);
        FalloutScript::Barter::Settings barterSettings();
        double barterPrice(const BarterMenu& menu, FalloutScript::FormId item, bool buying,
            int condition = FalloutScript::ActorState::fullCondition);
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
        /// The sound of a shot of the weapon that the attacker holds, or the click of it when it has no ammunition: the
        /// player hears the sound made for it (a 2D one) and the rest are heard from where they stand. Main thread.
        void playWeaponSound(FalloutScript::FormId attacker, FalloutScript::FormId weapon, bool dryFire);
        /// The loop of an automatic weapon (NAM7) goes on while the character keeps firing and stops a moment after it
        /// stops; every shot renews it. False when the weapon has no loop that can be played (the shot sound is then
        /// played for each shot). Main thread.
        bool keepWeaponLoop(FalloutScript::FormId attacker, FalloutScript::FormId weapon, double interval);
        void stopWeaponLoop(FalloutScript::FormId attacker);
        struct WeaponLoop
        {
            FalloutScript::FormId mWeapon = 0;
            std::string mFile;
            float mRemaining = 0.f;
        };
        std::map<FalloutScript::FormId, WeaponLoop> mWeaponLoops;
        /// What is printed in the corner of the screen, what the characters around make of the player, and the clock
        /// (seconds of play) that the second is told by
        FalloutScript::Hud::MessageLine mHudMessages;
        FalloutScript::Hud::SubtitleLine mHudSubtitles;
        FalloutScript::Hud::QuestTextLine mHudQuestText;
        /// The region line of the display: the cell the player was last looked for in, and the region it showed
        std::mutex mRegionMutex;
        FalloutScript::RegionLine::Announcer mRegionLine;
        ESM::RefId mRegionCell;
        bool mRegionCellKnown = false;
        /// Where the cell of the last look was, to tell a step to the next cell of the outside from a transition
        bool mRegionExterior = false;
        ESM::RefId mRegionWorld;
        int mRegionGridX = 0;
        int mRegionGridY = 0;
        FalloutScript::Hud::Watchers mWatchers;
        double mHudTime = 0;
        /// Whether the log last said that the player sneaks (it says when that changes)
        bool mSneakingLogged = false;
        /// Every map marker the content files place: the reference, where it is and its name. Read once.
        struct PlacedMarker
        {
            FalloutScript::FormId mId = 0;
            FalloutScript::FormId mWorldspace = 0;
            osg::Vec3f mPosition;
            std::string mName;
            unsigned mType = 0;
        };
        const std::vector<PlacedMarker>& placedMarkers();
        std::optional<std::vector<PlacedMarker>> mPlacedMarkers;
        std::mutex mSoundFilesMutex;
        std::map<FalloutScript::FormId, std::vector<std::string>> mSoundFiles;
        std::mutex mSoundNamesMutex;
        std::unordered_map<std::string, FalloutScript::FormId> mSoundNames;
        bool mSoundNamesBuilt = false;
        std::set<std::string> mInterfaceSoundsLogged;
        /// The idle records by editor id (lower case), made when the first PlayIdle asks, and the idles that a PlayIdle
        /// could not play (told once in the log)
        std::mutex mIdleMutex;
        bool mIdlesIndexed = false;
        std::map<std::string, ESM::FormId> mIdlesByName;
        std::set<std::string> mIdleProblems;
        /// The sounds of weapons that were looked for: the weapon, whether it is the click, and the sound (the player
        /// and the rest hear different shots of a weapon). Each is logged once, for what the data lacks
        std::set<std::tuple<FalloutScript::FormId, bool, std::uint32_t>> mWeaponSoundsLogged;
        std::mt19937 mWeaponSoundRandom;
        /// How a character behaves in a fight, from its AIDT (the record, or the template it takes it from)
        struct FightTraits
        {
            int mAggression = 0;
            int mConfidence = 2;
            float mRadius = 0; // how near the player has to be for it to go for it; 0 when the AIDT has no radius
        };
        const FightTraits& fightTraitsOf(const OFWorld::Ptr& actor);
        std::map<FalloutScript::FormId, FightTraits> mFightTraits;
        /// How far a character sees another one when the stealth equation can not be asked (fSneakMaxDistance of the
        /// files, which is where the games stop looking for a sneaking one)
        double sightDistance();
        /// How well the watcher sees or hears the target, by the stealth equation. In the world only.
        FalloutScript::Detection::Sighting sightOf(const OFWorld::Ptr& watcher, const OFWorld::Ptr& target);
        /// How brightly the character is lit, from 0 to 100: the sky by the hour outside, the ambient light of the cell
        /// inside (the lamps are not counted)
        double lightOn(const OFWorld::Ptr& target);
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

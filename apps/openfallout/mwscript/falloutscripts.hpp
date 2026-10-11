#ifndef OPENFALLOUT_SCRIPT_FALLOUTSCRIPTS_H
#define OPENFALLOUT_SCRIPT_FALLOUTSCRIPTS_H

#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <random>
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
#include <components/falloutscript/commandtable.hpp>
#include <components/falloutscript/conditions.hpp>
#include <components/falloutscript/dialogue.hpp>
#include <components/falloutscript/formlists.hpp>
#include <components/falloutscript/interpreter.hpp>
#include <components/falloutscript/messagecommands.hpp>
#include <components/falloutscript/objectcommands.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/packagecommands.hpp>
#include <components/falloutscript/placementcommands.hpp>
#include <components/falloutscript/playerprogress.hpp>
#include <components/falloutscript/quests.hpp>
#include <components/falloutscript/referencestate.hpp>
#include <components/falloutscript/vanillacommands.hpp>
#include <components/falloutscript/voicefiles.hpp>

#include <osg/Vec3f>

#include "../mwworld/ptr.hpp"

namespace ESM4
{
    struct AIPackage;
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
                                 private FalloutScript::ActorTraitRecords,
                                 private FalloutScript::DialogueRecords,
                                 private FalloutScript::ActorValueRecords,
                                 private FalloutScript::ReferenceRecords,
                                 private FalloutScript::FormListRecords,
                                 private FalloutScript::PackageWorld,
                                 private FalloutScript::PlacementWorld,
                                 private FalloutScript::MessageWorld
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

        /// Whether a script killed the character or creature (and none brought it back). Nothing else in the game kills
        /// yet.
        bool isDead(const OFWorld::Ptr& actor) const;

        /// The AI packages that scripts gave the character, the last given first (those that no content file has a
        /// record of are left out)
        std::vector<const ESM4::AIPackage*> scriptPackages(const OFWorld::Ptr& actor) const;
        /// Whether the conditions of the package hold for the character. A package that has none holds.
        bool packageConditionsHold(const ESM4::AIPackage& package, const OFWorld::Ptr& actor);

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
        /// The character talks to the player. The greeting is said (a topic given takes its place) and the conversation
        /// goes on from there, which the interface shows. False, with no conversation open, when the character has
        /// nothing to say and the player nothing to pick. A conversation that is open stays.
        bool startConversation(const OFWorld::Ptr& speaker, FalloutScript::FormId topic = 0);
        /// startConversation and the interface that shows it. For the thread of the frame.
        bool talkTo(const OFWorld::Ptr& speaker, FalloutScript::FormId topic = 0);
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
        void activate(FalloutScript::FormId target, FalloutScript::FormId activator, bool runOnActivate) override;

        void addScriptPackage(FalloutScript::FormId actor, FalloutScript::FormId package) override;
        void removeScriptPackage(FalloutScript::FormId actor) override;
        void evaluatePackage(FalloutScript::FormId actor) override;
        FalloutScript::FormId currentPackage(FalloutScript::FormId actor) override;

        bool placement(FalloutScript::FormId reference, FalloutScript::Placement& result) override;
        void moveTo(FalloutScript::FormId reference, FalloutScript::FormId target,
            const std::array<double, 3>& offset) override;
        void setPosition(FalloutScript::FormId reference, int axis, double value) override;
        void setAngle(FalloutScript::FormId reference, int axis, double radians) override;

        void showMessage(FalloutScript::FormId message) override;

        bool traitsOf(FalloutScript::FormId reference, FalloutScript::ActorTraits& traits) override;

        /// The character says a response of the topic outside of a conversation: what the response does is done, and
        /// the lines are told to the player: the voice of the data and, when subtitles are on, the text
        bool sayTopic(FalloutScript::FormId reference, FalloutScript::FormId topic);
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
        bool initialValues(FalloutScript::FormId actor, std::vector<std::pair<int, double>>& values) override;
        bool levelOf(FalloutScript::FormId actor, int playerLevel, int& level) override;
        bool initialLock(FalloutScript::FormId reference, bool& locked, int& level) override;
        bool formListEntries(FalloutScript::FormId list, std::vector<FalloutScript::FormId>& entries) override;
        bool baseFormOf(FalloutScript::FormId reference, FalloutScript::FormId& base) override;
        FalloutScript::FormId worldspaceOf(FalloutScript::FormId reference) override;
        FalloutScript::FormId linkedRef(FalloutScript::FormId reference, FalloutScript::FormId keyword) override;
        FalloutScript::FormId placeAtMe(FalloutScript::FormId reference, FalloutScript::FormId base, int count,
            double distance, int direction) override;
        double gameHour() override;
        double gameDaysPassed() override;

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
        /// The object of the reference when it is in a cell that is loaded, else an empty one
        OFWorld::Ptr findReference(FalloutScript::FormId reference);
        void runPending();
        /// Objects that were moved to cells that are not loaded stop running their scripts
        void dropObjectsThatLeft();

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
        void tellPlayer(const std::string& text);
        void buildNames() const;

        const OFWorld::ESMStore& mStore;
        FalloutScript::CommandTable mCommands;
        FalloutScript::Interpreter mInterpreter;
        FalloutScript::ConditionEvaluator mConditions;
        FalloutScript::QuestManager mQuests;
        FalloutScript::ObjectScripts mObjects;
        FalloutScript::ActorState mActors;
        FalloutScript::ActorValues mActorValues;
        FalloutScript::ReferenceState mReferenceState;
        FalloutScript::FormLists mFormLists;
        FalloutScript::PlayerProgress mProgress;
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
        FalloutScript::FormId mPlayer;
        std::thread::id mMainThread;
        /// What other threads asked the world for, and the state of the objects among it that scripts must see at once
        std::mutex mDeferredMutex;
        std::vector<std::function<void()>> mDeferred;
        std::map<FalloutScript::FormId, bool> mDeferredDisabled;
        /// Where other threads asked to put objects, for the same reason: the next command of the script starts from
        /// there
        std::map<FalloutScript::FormId, Move> mDeferredPlaces;
        /// Whether scripts disabled (true) or enabled (false) objects in cells that were not loaded at the time, which
        /// holds until the cell is loaded
        std::map<FalloutScript::FormId, bool> mDisabled;
        /// The objects of the loaded cells by their references: the world only knows those that were put in the scene,
        /// which a disabled one never was
        std::unordered_map<FalloutScript::FormId, OFWorld::Ptr> mReferences;
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
        /// The packages that scripts gave characters, the last given at the end. They stay while the character's cell
        /// is not loaded.
        std::map<FalloutScript::FormId, std::vector<FalloutScript::FormId>> mScriptPackages;
        std::mt19937 mRandom;
        std::vector<std::string> mContentFiles;
        /// Seconds of play since the commands that scripts reached without a handler were last written to the log
        float mMissingTimer = 0;
        /// Writes the commands that scripts reached without a handler, with how often, to the log (nothing when there
        /// were none)
        void reportMissingCommands();
        /// The values the global variables have now, for those that were set (the others have the value of the record)
        std::map<FalloutScript::FormId, double> mGlobals;
        /// Editor ids in lower case, made when first asked for
        mutable bool mNamesBuilt = false;
        mutable std::map<std::string, FalloutScript::FormId> mQuestNames;
        mutable std::map<std::string, FalloutScript::FormId> mGlobalNames;
    };
}

#endif

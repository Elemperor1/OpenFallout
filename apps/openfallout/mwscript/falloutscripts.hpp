#ifndef OPENFALLOUT_SCRIPT_FALLOUTSCRIPTS_H
#define OPENFALLOUT_SCRIPT_FALLOUTSCRIPTS_H

#include <functional>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <components/falloutscript/commandtable.hpp>
#include <components/falloutscript/conditions.hpp>
#include <components/falloutscript/interpreter.hpp>
#include <components/falloutscript/messagecommands.hpp>
#include <components/falloutscript/objectcommands.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/packagecommands.hpp>
#include <components/falloutscript/quests.hpp>
#include <components/falloutscript/vanillacommands.hpp>

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
                                 private FalloutScript::PackageWorld,
                                 private FalloutScript::MessageWorld
    {
    public:
        FalloutScripts(const OFWorld::ESMStore& store, FalloutScript::Game game,
            FalloutScript::FormId player = FalloutScript::playerReference);

        FalloutScripts(const FalloutScripts&) = delete;
        FalloutScripts& operator=(const FalloutScripts&) = delete;

        /// Starts over, as for a new game: global variables have the values of their records, quests are at their
        /// beginning. What was loaded of the world is forgotten.
        void reset();

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
        /// action reference, and tells whether it has one.
        bool activated(const OFWorld::Ptr& object, const OFWorld::Ptr& actor);

        /// The AI packages that scripts gave the character, the last given first (those that no content file has a
        /// record of are left out)
        std::vector<const ESM4::AIPackage*> scriptPackages(const OFWorld::Ptr& actor) const;
        /// Whether the conditions of the package hold for the character. A package that has none holds.
        bool packageConditionsHold(const ESM4::AIPackage& package, const OFWorld::Ptr& actor);

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

        void addScriptPackage(FalloutScript::FormId actor, FalloutScript::FormId package) override;
        void removeScriptPackage(FalloutScript::FormId actor) override;
        void evaluatePackage(FalloutScript::FormId actor) override;

        void showMessage(FalloutScript::FormId message) override;

        /// The object of the reference when it is in a cell that is loaded, else an empty one
        OFWorld::Ptr findReference(FalloutScript::FormId reference);
        void runPending();
        /// Objects that were moved to cells that are not loaded stop running their scripts
        void dropObjectsThatLeft();

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
        FalloutScript::FormId mPlayer;
        std::thread::id mMainThread;
        /// What other threads asked the world for, and the state of the objects among it that scripts must see at once
        std::mutex mDeferredMutex;
        std::vector<std::function<void()>> mDeferred;
        std::map<FalloutScript::FormId, bool> mDeferredDisabled;
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
        /// The packages that scripts gave characters, the last given at the end. They stay while the character's cell
        /// is not loaded.
        std::map<FalloutScript::FormId, std::vector<FalloutScript::FormId>> mScriptPackages;
        std::mt19937 mRandom;
        /// The values the global variables have now, for those that were set (the others have the value of the record)
        std::map<FalloutScript::FormId, double> mGlobals;
        /// Editor ids in lower case, made when first asked for
        mutable bool mNamesBuilt = false;
        mutable std::map<std::string, FalloutScript::FormId> mQuestNames;
        mutable std::map<std::string, FalloutScript::FormId> mGlobalNames;
    };
}

#endif

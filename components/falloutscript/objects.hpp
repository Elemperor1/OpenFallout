#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_OBJECTS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_OBJECTS_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <components/esm4/loadscpt.hpp>

#include "interpreter.hpp"
#include "saveaccess.hpp"

namespace FalloutScript
{
    /// The scripts of objects (the SCRI of a base record, run for each reference placed in a cell): one instance per
    /// reference, with the variables of its own, and the events they run on. A reference keeps its instance, and so
    /// its variables, when its cell is unloaded and loaded again.
    ///
    /// A reference that is loaded (its cell is) runs the blocks of GameMode every frame, and OnLoad each time it
    /// becomes loaded. The other events are triggered from outside (OnActivate, OnAdd ...).
    class ObjectScripts
    {
        template <class T>
        friend struct SaveAccess;

    public:
        /// Finds the record of a script by its form id, null for one that is not known
        using ScriptFinder = std::function<const ESM4::Script*(FormId)>;

        ObjectScripts(Interpreter& interpreter, ScriptFinder finder);

        /// Gives the reference the script. Returns its instance: the one it has already when it has been given a
        /// script before, null when the script is not known or can not be run (that is reported, once for each).
        Instance* add(FormId reference, FormId script);

        Instance* instance(FormId reference);
        bool has(FormId reference) const { return mObjects.count(reference) != 0; }
        std::size_t size() const { return mObjects.size(); }

        /// Whether the script of the reference has a block for the event (one the filter accepts, when it is given)
        bool hasBlock(FormId reference, std::uint16_t blockType, const BlockFilter& filter = {}) const;

        /// A reference is loaded or not loaded. Loading it runs its OnLoad blocks, unless it was loaded already.
        /// Nothing happens for a reference that has no script.
        void setLoaded(FormId reference, bool loaded);
        bool loaded(FormId reference) const;
        /// All the references that are loaded, whatever blocks their scripts have
        std::vector<FormId> loadedReferences() const { return std::vector<FormId>(mLoaded.begin(), mLoaded.end()); }

        /// Runs the blocks of the event on the script of the reference. The action reference is the object that caused
        /// the event (the one that activates), which GetActionRef tells while it runs. Returns whether the script has
        /// such a block. The filter picks among the blocks of the event by what they name (a SayToDone block names the
        /// topic that was said): the blocks it refuses are not run, and do not count as blocks of the script.
        bool trigger(
            FormId reference, std::uint16_t blockType, FormId actionReference = 0, const BlockFilter& filter = {});

        /// Runs a script that is a list of statements, such as the one a package runs when it begins, ends or changes,
        /// with the reference as its object: the commands in it that name none act on the reference. Returns whether
        /// the script has code to run (one that can not be run is reported, once for each). The definition must remain
        /// alive and unchanged until clear(), because prepared scripts are cached by definition address.
        bool runResult(const ESM4::ScriptDefinition& definition, FormId reference);

        /// Lets a frame pass: the GameMode blocks of the loaded references run, in the order of their form ids
        void update();
        /// Lets a frame pass while a menu is open: the MenuMode blocks of the loaded references run instead (those
        /// the filter accepts: a block that names a menu is for that menu)
        void updateMenu(const BlockFilter& filter = {});
        /// The references that run GameMode now
        std::vector<FormId> updating() const { return std::vector<FormId>(mUpdating.begin(), mUpdating.end()); }

        /// The seconds the last frame took, which GetSecondsPassed tells (set before any script runs in the frame)
        void setSecondsPassed(float seconds) { mSeconds = seconds; }
        float secondsPassed() const { return mSeconds; }
        /// The object that caused the event that runs now, 0 outside of one
        FormId actionReference() const { return mActionReference; }

        /// Forgets all the instances, as for a new game
        void clear();

    private:
        struct Object
        {
            std::shared_ptr<Instance> mInstance;
            /// The script the instance was made from
            FormId mScript = 0;
            bool mLoaded = false;
            bool mHasGameMode = false;
            bool mHasMenuMode = false;
        };

        /// Makes the object that caused the event the one GetActionRef tells, and gives back the one of the event
        /// outside when it ends, however it ends
        struct ActionScope
        {
            ActionScope(FormId& target, FormId value)
                : mTarget(target)
                , mPrevious(target)
            {
                mTarget = value;
            }
            ~ActionScope() { mTarget = mPrevious; }
            ActionScope(const ActionScope&) = delete;
            ActionScope& operator=(const ActionScope&) = delete;

        private:
            FormId& mTarget;
            FormId mPrevious;
        };

        std::shared_ptr<const Script> prepared(const ESM4::ScriptDefinition& definition);
        void run(const std::set<FormId>& references, std::uint16_t blockType, const BlockFilter& filter = {});

        /// The variables that a saved game held for the script of a reference whose script has not been given to it yet
        /// (its cell is not loaded): the script takes them when it is given
        struct SavedInstance
        {
            FormId mScript = 0;
            /// The kind and the value of each variable, from the first
            std::vector<VariableKind> mKinds;
            std::vector<double> mVariables;
        };

        Interpreter& mInterpreter;
        ScriptFinder mFinder;
        std::map<FormId, Object> mObjects;
        std::map<FormId, SavedInstance> mSaved;
        /// The references that run GameMode: the loaded ones whose script has such a block
        std::set<FormId> mUpdating;
        /// The references that run MenuMode while a menu is open: the loaded ones whose script has such a block
        std::set<FormId> mMenuUpdating;
        /// The references that are loaded
        std::set<FormId> mLoaded;
        std::map<const ESM4::ScriptDefinition*, std::shared_ptr<const Script>> mPrepared;
        /// The scripts that were reported as not runnable, so that they are reported once
        std::set<FormId> mReported;
        std::set<const ESM4::ScriptDefinition*> mReportedDefinitions;
        float mSeconds = 0;
        FormId mActionReference = 0;
    };
}

#endif

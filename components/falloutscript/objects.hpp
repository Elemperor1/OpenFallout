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

        /// Whether the script of the reference has a block for the event
        bool hasBlock(FormId reference, std::uint16_t blockType) const;

        /// A reference is loaded or not loaded. Loading it runs its OnLoad blocks, unless it was loaded already.
        /// Nothing happens for a reference that has no script.
        void setLoaded(FormId reference, bool loaded);
        bool loaded(FormId reference) const;

        /// Runs the blocks of the event on the script of the reference. The action reference is the object that caused
        /// the event (the one that activates), which GetActionRef tells while it runs. Returns whether the script has
        /// such a block.
        bool trigger(FormId reference, std::uint16_t blockType, FormId actionReference = 0);

        /// Lets a frame pass: the GameMode blocks of the loaded references run, in the order of their form ids
        void update();
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
            bool mLoaded = false;
            bool mHasGameMode = false;
        };

        std::shared_ptr<const Script> prepared(const ESM4::ScriptDefinition& definition);

        Interpreter& mInterpreter;
        ScriptFinder mFinder;
        std::map<FormId, Object> mObjects;
        /// The references that run GameMode: the loaded ones whose script has such a block
        std::set<FormId> mUpdating;
        std::map<const ESM4::ScriptDefinition*, std::shared_ptr<const Script>> mPrepared;
        /// The scripts that were reported as not runnable, so that they are reported once
        std::set<FormId> mReported;
        float mSeconds = 0;
        FormId mActionReference = 0;
    };
}

#endif

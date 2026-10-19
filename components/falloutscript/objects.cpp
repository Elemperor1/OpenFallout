#include "objects.hpp"

#include <algorithm>
#include <vector>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        bool hasBlockOfType(const Script& script, std::uint16_t blockType, const BlockFilter& filter = {})
        {
            return std::any_of(script.blocks().begin(), script.blocks().end(),
                [&](const Block& block) { return block.mType == blockType && (!filter || filter(block)); });
        }
    }

    ObjectScripts::ObjectScripts(Interpreter& interpreter, ScriptFinder finder)
        : mInterpreter(interpreter)
        , mFinder(std::move(finder))
    {
    }

    std::shared_ptr<const Script> ObjectScripts::prepared(const ESM4::ScriptDefinition& definition)
    {
        auto& script = mPrepared[&definition];
        if (!script)
            script = std::make_shared<const Script>(Script::prepare(definition, mInterpreter.commands()));
        return script;
    }

    Instance* ObjectScripts::add(FormId reference, FormId script)
    {
        if (reference == 0)
            return nullptr;
        if (const auto found = mObjects.find(reference); found != mObjects.end())
            return found->second.mInstance.get();

        const ESM4::Script* record = script != 0 && mFinder ? mFinder(script) : nullptr;
        if (record == nullptr)
        {
            if (mReported.insert(script).second)
                mInterpreter.host().log("The script " + hex(script) + " of an object is not known");
            return nullptr;
        }
        std::shared_ptr<const Script> prepared = this->prepared(record->mScript);
        if (!prepared->usable())
        {
            if (mReported.insert(script).second)
                mInterpreter.host().log(
                    "The script " + hex(script) + " of an object cannot be run: " + prepared->problemText());
            return nullptr;
        }

        Object& object = mObjects[reference];
        object.mHasGameMode = hasBlockOfType(*prepared, BlockType::GameMode);
        object.mHasMenuMode = hasBlockOfType(*prepared, BlockType::MenuMode);
        object.mInstance = std::make_shared<Instance>(std::move(prepared), reference);
        object.mScript = script;
        if (const auto saved = mSaved.find(reference); saved != mSaved.end())
        {
            // A script that was changed since the save (other variables) starts over, as the games do
            const std::vector<Variable>& variables = object.mInstance->script().variables();
            const SavedInstance& state = saved->second;
            if (state.mScript == script && state.mVariables.size() + 1 == variables.size())
            {
                bool same = true;
                for (std::size_t i = 0; i < state.mKinds.size(); ++i)
                    same = same && variables[i + 1].mKind == state.mKinds[i];
                if (same)
                    for (std::size_t i = 0; i < state.mVariables.size(); ++i)
                        object.mInstance->setVariable(static_cast<std::uint32_t>(i + 1), state.mVariables[i]);
            }
            mSaved.erase(saved);
        }
        return object.mInstance.get();
    }

    Instance* ObjectScripts::instance(FormId reference)
    {
        const auto found = mObjects.find(reference);
        return found != mObjects.end() ? found->second.mInstance.get() : nullptr;
    }

    bool ObjectScripts::hasBlock(FormId reference, std::uint16_t blockType, const BlockFilter& filter) const
    {
        const auto found = mObjects.find(reference);
        return found != mObjects.end() && hasBlockOfType(found->second.mInstance->script(), blockType, filter);
    }

    void ObjectScripts::setLoaded(FormId reference, bool loaded)
    {
        const auto found = mObjects.find(reference);
        if (found == mObjects.end() || found->second.mLoaded == loaded)
            return;
        Object& object = found->second;
        object.mLoaded = loaded;
        if (!loaded)
        {
            mUpdating.erase(reference);
            mMenuUpdating.erase(reference);
            mLoaded.erase(reference);
            return;
        }
        mLoaded.insert(reference);
        if (object.mHasGameMode)
            mUpdating.insert(reference);
        if (object.mHasMenuMode)
            mMenuUpdating.insert(reference);
        trigger(reference, BlockType::OnLoad);
    }

    bool ObjectScripts::loaded(FormId reference) const
    {
        const auto found = mObjects.find(reference);
        return found != mObjects.end() && found->second.mLoaded;
    }

    bool ObjectScripts::trigger(
        FormId reference, std::uint16_t blockType, FormId actionReference, const BlockFilter& filter)
    {
        const auto found = mObjects.find(reference);
        if (found == mObjects.end() || !hasBlockOfType(found->second.mInstance->script(), blockType, filter))
            return false;
        // The script keeps living while it runs, even if a command it calls makes the object forget it
        const std::shared_ptr<Instance> instance = found->second.mInstance;
        const ActionScope scope(mActionReference, actionReference);
        mInterpreter.run(*instance, blockType, filter);
        return true;
    }

    bool ObjectScripts::runResult(const ESM4::ScriptDefinition& definition, FormId reference)
    {
        if (reference == 0 || definition.compiledScript.empty())
            return false;
        std::shared_ptr<const Script> script = prepared(definition);
        if (!script->usable())
        {
            if (mReportedDefinitions.insert(&definition).second)
                mInterpreter.host().log(
                    "A script that runs on an event of a package cannot be run: " + script->problemText());
            return false;
        }
        // A script with blocks is not a list of statements: there is nothing to run
        if (!script->isResult())
            return false;
        Instance instance(std::move(script), reference);
        const ActionScope scope(mActionReference, 0);
        mInterpreter.runResult(instance);
        return true;
    }

    void ObjectScripts::update()
    {
        run(mUpdating, BlockType::GameMode);
    }

    void ObjectScripts::updateMenu(const BlockFilter& filter)
    {
        run(mMenuUpdating, BlockType::MenuMode, filter);
    }

    void ObjectScripts::run(const std::set<FormId>& references, std::uint16_t blockType, const BlockFilter& filter)
    {
        // What runs can load and unload references, so what is run is a copy, and each is checked before its turn
        const std::vector<FormId> running(references.begin(), references.end());
        for (const FormId reference : running)
        {
            if (references.count(reference) == 0)
                continue;
            const auto found = mObjects.find(reference);
            if (found == mObjects.end())
                continue;
            const std::shared_ptr<Instance> instance = found->second.mInstance;
            mInterpreter.run(*instance, blockType, filter);
        }
    }

    void ObjectScripts::clear()
    {
        mObjects.clear();
        mSaved.clear();
        mUpdating.clear();
        mMenuUpdating.clear();
        mLoaded.clear();
        mPrepared.clear();
        mReported.clear();
        mReportedDefinitions.clear();
        mSeconds = 0;
        mActionReference = 0;
    }
}

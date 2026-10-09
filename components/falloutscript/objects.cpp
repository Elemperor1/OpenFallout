#include "objects.hpp"

#include <algorithm>
#include <vector>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        bool hasBlockOfType(const Script& script, std::uint16_t blockType)
        {
            return std::any_of(script.blocks().begin(), script.blocks().end(),
                [blockType](const Block& block) { return block.mType == blockType; });
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
                mInterpreter.host().log("The script " + hex(script) + " of an object cannot be run");
            return nullptr;
        }

        Object& object = mObjects[reference];
        object.mHasGameMode = hasBlockOfType(*prepared, BlockType::GameMode);
        object.mInstance = std::make_shared<Instance>(std::move(prepared), reference);
        return object.mInstance.get();
    }

    Instance* ObjectScripts::instance(FormId reference)
    {
        const auto found = mObjects.find(reference);
        return found != mObjects.end() ? found->second.mInstance.get() : nullptr;
    }

    bool ObjectScripts::hasBlock(FormId reference, std::uint16_t blockType) const
    {
        const auto found = mObjects.find(reference);
        return found != mObjects.end() && hasBlockOfType(found->second.mInstance->script(), blockType);
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
            return;
        }
        if (object.mHasGameMode)
            mUpdating.insert(reference);
        trigger(reference, BlockType::OnLoad);
    }

    bool ObjectScripts::loaded(FormId reference) const
    {
        const auto found = mObjects.find(reference);
        return found != mObjects.end() && found->second.mLoaded;
    }

    bool ObjectScripts::trigger(FormId reference, std::uint16_t blockType, FormId actionReference)
    {
        const auto found = mObjects.find(reference);
        if (found == mObjects.end() || !hasBlockOfType(found->second.mInstance->script(), blockType))
            return false;
        // The script keeps living while it runs, even if a command it calls makes the object forget it
        const std::shared_ptr<Instance> instance = found->second.mInstance;
        // The action reference of the event outside is back when this one ends, however it ends
        struct Restore
        {
            FormId& mTarget;
            FormId mValue;
            ~Restore() { mTarget = mValue; }
        } restore{ mActionReference, mActionReference };
        mActionReference = actionReference;
        mInterpreter.run(*instance, blockType);
        return true;
    }

    void ObjectScripts::update()
    {
        // What runs can load and unload references, so what is run is a copy, and each is checked before its turn
        const std::vector<FormId> running(mUpdating.begin(), mUpdating.end());
        for (const FormId reference : running)
        {
            if (mUpdating.count(reference) == 0)
                continue;
            const auto found = mObjects.find(reference);
            if (found == mObjects.end())
                continue;
            const std::shared_ptr<Instance> instance = found->second.mInstance;
            mInterpreter.run(*instance, BlockType::GameMode);
        }
    }

    void ObjectScripts::clear()
    {
        mObjects.clear();
        mUpdating.clear();
        mPrepared.clear();
        mReported.clear();
        mSeconds = 0;
        mActionReference = 0;
    }
}

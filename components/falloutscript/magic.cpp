#include "magic.hpp"

#include <algorithm>

#include "actorvalues.hpp"
#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        constexpr int typeDisease = 1;
        constexpr int typeAbility = 4;
        constexpr int typeAddiction = 10;

        /// The types of spell that are put on the character when it is given the spell
        bool appliedWhenAdded(int type)
        {
            return type == typeDisease || type == typeAbility || type == typeAddiction;
        }
    }

    Magic::Magic(MagicRecords& records)
        : mRecords(records)
    {
    }

    void Magic::setModifier(Modifier modifier)
    {
        const std::lock_guard lock(mMutex);
        mModifier = std::move(modifier);
    }

    void Magic::setCaster(Caster caster)
    {
        const std::lock_guard lock(mMutex);
        mCaster = std::move(caster);
    }

    void Magic::setConditions(ConditionCheck check)
    {
        const std::lock_guard lock(mMutex);
        mConditions = std::move(check);
    }

    void Magic::setScriptRunner(ScriptRunner runner)
    {
        const std::lock_guard lock(mMutex);
        mScripts = std::move(runner);
    }

    void Magic::dropFailing(FormId actor, SpellInfo& info)
    {
        ConditionCheck check;
        {
            const std::lock_guard lock(mMutex);
            check = mConditions;
        }
        // The conditions are asked outside the lock: they call back into the host
        info.mEffects.erase(std::remove_if(info.mEffects.begin(), info.mEffects.end(),
                                [&](const SpellEffect& effect) {
                                    if (effect.mConditions.empty())
                                        return false;
                                    ConditionContext context;
                                    context.mSubject = actor;
                                    return !check || !check(effect.mConditions, context);
                                }),
            info.mEffects.end());
    }

    void Magic::clear()
    {
        const std::lock_guard lock(mMutex);
        mSpells.clear();
        mRunning.clear();
    }

    bool Magic::addSpell(FormId actor, FormId spell)
    {
        SpellInfo info;
        if (actor == 0 || !mRecords.spell(spell, info))
            return false;
        if (appliedWhenAdded(info.mType))
            dropFailing(actor, info);
        Modifier modifier;
        ScriptRunner scripts;
        std::vector<std::pair<int, double>> changes;
        std::vector<std::pair<FormId, EffectId>> started;
        {
            const std::lock_guard lock(mMutex);
            if (!mSpells[actor].insert(spell).second)
                return false;
            if (!appliedWhenAdded(info.mType))
                return true;
            modifier = mModifier;
            scripts = mScripts;
            for (const SpellEffect& effect : info.mEffects)
            {
                Running running;
                running.mActor = actor;
                running.mEffect = effect.mEffect;
                running.mSource = spell;
                running.mScript = mRecords.effectScript(effect.mEffect);
                running.mId = ++mLastId;
                if (running.mScript != 0)
                    started.emplace_back(running.mScript, running.mId);
                if (effect.mArchetype == 0 && ActorValues::isValid(effect.mActorValue))
                {
                    running.mActorValue = effect.mActorValue;
                    running.mDelta = effect.mDetrimental ? -effect.mMagnitude : effect.mMagnitude;
                    changes.emplace_back(running.mActorValue, running.mDelta);
                }
                mRunning.push_back(running);
            }
        }
        if (modifier)
            for (const auto& [actorValue, amount] : changes)
                modifier(actor, actorValue, amount);
        if (scripts)
            for (const auto& [script, id] : started)
                scripts(actor, script, ScriptStage::Start, id);
        return true;
    }

    bool Magic::removeSpell(FormId actor, FormId spell)
    {
        Modifier modifier;
        ScriptRunner scripts;
        std::vector<std::pair<int, double>> changes;
        std::vector<std::pair<FormId, EffectId>> finished;
        {
            const std::lock_guard lock(mMutex);
            const auto found = mSpells.find(actor);
            if (found == mSpells.end() || found->second.erase(spell) == 0)
                return false;
            modifier = mModifier;
            scripts = mScripts;
            for (auto it = mRunning.begin(); it != mRunning.end();)
            {
                // What a cast of the spell started runs out on its own; only what the spell itself keeps goes now
                if (it->mActor == actor && it->mSource == spell && it->mRemaining <= 0)
                {
                    if (it->mActorValue >= 0)
                        changes.emplace_back(it->mActorValue, -it->mDelta);
                    if (it->mScript != 0)
                        finished.emplace_back(it->mScript, it->mId);
                    it = mRunning.erase(it);
                }
                else
                    ++it;
            }
        }
        if (modifier)
            for (const auto& [actorValue, amount] : changes)
                modifier(actor, actorValue, amount);
        if (scripts)
            for (const auto& [script, id] : finished)
                scripts(actor, script, ScriptStage::Finish, id);
        return true;
    }

    bool Magic::hasSpell(FormId actor, FormId spell)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mSpells.find(actor);
        return found != mSpells.end() && found->second.count(spell) != 0;
    }

    std::vector<FormId> Magic::spells(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mSpells.find(actor);
        return found == mSpells.end() ? std::vector<FormId>()
                                      : std::vector<FormId>(found->second.begin(), found->second.end());
    }

    void Magic::castOnSelf(FormId actor, FormId spell)
    {
        SpellInfo info;
        if (actor == 0 || !mRecords.spell(spell, info) || info.mType == typeAbility)
            return;
        // An effect whose conditions do not hold is not on the character, and the caster leaves it out as well
        dropFailing(actor, info);
        Caster caster;
        ScriptRunner scripts;
        struct Started
        {
            FormId mScript;
            EffectId mId;
            bool mAtOnce;
        };
        std::vector<Started> started;
        std::vector<std::pair<FormId, EffectId>> replaced;
        {
            const std::lock_guard lock(mMutex);
            caster = mCaster;
            scripts = mScripts;
            for (const SpellEffect& effect : info.mEffects)
            {
                const FormId script = mRecords.effectScript(effect.mEffect);
                // An effect that happens at once is over as it is cast (a script of one is run from its start to its
                // end: not traced in the executables)
                if (effect.mDuration <= 0)
                {
                    if (script != 0)
                        started.push_back({ script, ++mLastId, true });
                    continue;
                }
                if (script != 0)
                {
                    // A spell cast again while it runs renews its script effect: the earlier one ends before the new
                    // one begins, so that its end does not undo what the new start did (the bed takes the perk away at
                    // the end of the effect that gave it)
                    for (auto it = mRunning.begin(); it != mRunning.end();)
                    {
                        if (it->mActor == actor && it->mSource == spell && it->mEffect == effect.mEffect
                            && it->mScript != 0 && it->mRemaining > 0)
                        {
                            replaced.emplace_back(it->mScript, it->mId);
                            it = mRunning.erase(it);
                        }
                        else
                            ++it;
                    }
                }
                Running running;
                running.mActor = actor;
                running.mEffect = effect.mEffect;
                running.mSource = spell;
                running.mRemaining = effect.mDuration;
                running.mScript = script;
                running.mId = ++mLastId;
                mRunning.push_back(running);
                if (script != 0)
                    started.push_back({ script, running.mId, false });
            }
        }
        if (caster)
            caster(actor, spell, info.mEffects);
        if (scripts)
        {
            for (const auto& [script, id] : replaced)
                scripts(actor, script, ScriptStage::Finish, id);
            for (const Started& one : started)
            {
                scripts(actor, one.mScript, ScriptStage::Start, one.mId);
                if (one.mAtOnce)
                    scripts(actor, one.mScript, ScriptStage::Finish, one.mId);
            }
        }
    }

    void Magic::noteEffect(FormId actor, FormId effect, FormId source, double seconds)
    {
        if (actor == 0 || effect == 0 || seconds <= 0)
            return;
        Running running;
        running.mActor = actor;
        running.mEffect = effect;
        running.mSource = source;
        running.mRemaining = seconds;
        running.mScript = mRecords.effectScript(effect);
        ScriptRunner scripts;
        {
            const std::lock_guard lock(mMutex);
            running.mId = ++mLastId;
            mRunning.push_back(running);
            scripts = mScripts;
        }
        if (scripts && running.mScript != 0)
            scripts(actor, running.mScript, ScriptStage::Start, running.mId);
    }

    bool Magic::hasEffect(FormId actor, FormId effect)
    {
        const std::lock_guard lock(mMutex);
        return std::any_of(mRunning.begin(), mRunning.end(),
            [&](const Running& running) { return running.mActor == actor && running.mEffect == effect; });
    }

    bool Magic::isSpellTarget(FormId actor, FormId spell)
    {
        const std::lock_guard lock(mMutex);
        return std::any_of(mRunning.begin(), mRunning.end(),
            [&](const Running& running) { return running.mActor == actor && running.mSource == spell; });
    }

    void Magic::update(double seconds)
    {
        ScriptRunner scripts;
        struct Scripted
        {
            FormId mActor;
            FormId mScript;
            EffectId mId;
        };
        std::vector<Scripted> running;
        std::vector<Scripted> finished;
        {
            const std::lock_guard lock(mMutex);
            scripts = mScripts;
            for (auto it = mRunning.begin(); it != mRunning.end();)
            {
                // An effect that lasts until its spell is removed has no seconds left to count
                if (it->mRemaining > 0 && (it->mRemaining -= seconds) <= 0)
                {
                    if (it->mScript != 0)
                        finished.push_back({ it->mActor, it->mScript, it->mId });
                    it = mRunning.erase(it);
                }
                else
                {
                    if (it->mScript != 0)
                        running.push_back({ it->mActor, it->mScript, it->mId });
                    ++it;
                }
            }
        }
        // The scripts run outside the lock: they cast, give and take away like any other
        if (scripts)
        {
            for (const Scripted& one : running)
            {
                // The script of an effect that an earlier one took away in this frame has ended: it is not run again
                if (still(one.mId))
                    scripts(one.mActor, one.mScript, ScriptStage::Update, one.mId);
            }
            for (const Scripted& one : finished)
                scripts(one.mActor, one.mScript, ScriptStage::Finish, one.mId);
        }
    }

    bool Magic::still(EffectId id)
    {
        const std::lock_guard lock(mMutex);
        return std::any_of(
            mRunning.begin(), mRunning.end(), [id](const Running& running) { return running.mId == id; });
    }

    template <>
    struct SaveAccess<Magic>
    {
        static void write(Magic& magic, SaveWriter& out)
        {
            const std::lock_guard lock(magic.mMutex);
            out.count(magic.mSpells.size());
            for (const auto& [actor, spells] : magic.mSpells)
            {
                out.form(actor);
                out.count(spells.size());
                for (const FormId spell : spells)
                    out.form(spell);
            }
            out.count(magic.mRunning.size());
            for (const Magic::Running& running : magic.mRunning)
            {
                out.form(running.mActor);
                out.form(running.mEffect);
                out.form(running.mSource);
                out.f64(running.mRemaining);
                out.i32(running.mActorValue);
                out.f64(running.mDelta);
            }
        }

        static void read(Magic& magic, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, std::set<FormId>> spells;
            const std::size_t actors = in.count(8);
            for (std::size_t i = 0; i < actors; ++i)
            {
                const std::optional<FormId> actor = in.form();
                const std::size_t count = in.count(4);
                std::set<FormId> held;
                for (std::size_t j = 0; j < count; ++j)
                    if (const std::optional<FormId> spell = in.form())
                        held.insert(*spell);
                if (actor)
                    spells[*actor] = std::move(held);
            }
            std::vector<Magic::Running> running;
            const std::size_t count = in.count(32);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> actor = in.form();
                const std::optional<FormId> effect = in.form();
                const std::optional<FormId> source = in.form();
                Magic::Running entry;
                entry.mRemaining = in.f64();
                entry.mActorValue = in.i32();
                entry.mDelta = in.f64();
                // What comes from a plugin that is gone is left out
                if (actor && effect && source)
                {
                    entry.mActor = *actor;
                    entry.mEffect = *effect;
                    entry.mSource = *source;
                    entry.mScript = magic.mRecords.effectScript(*effect);
                    running.push_back(entry);
                }
            }
            const std::lock_guard lock(magic.mMutex);
            // A loaded game numbers its running effects again
            for (Magic::Running& entry : running)
                entry.mId = ++magic.mLastId;
            magic.mSpells = std::move(spells);
            magic.mRunning = std::move(running);
        }
    };

    void writeMagic(Magic& magic, SaveWriter& out)
    {
        SaveAccess<Magic>::write(magic, out);
    }

    void readMagic(Magic& magic, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<Magic>::read(magic, in, version);
    }

    void addMagicCommands(Interpreter& interpreter, Magic& magic, MagicRecords& records)
    {
        interpreter.setHandler("AddSpell", [&magic, &records](CallContext& call) {
            if (call.mReference != 0 && records.isActor(call.mReference))
                magic.addSpell(call.mReference, formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("RemoveSpell", [&magic, &records](CallContext& call) {
            if (call.mReference != 0 && records.isActor(call.mReference))
                magic.removeSpell(call.mReference, formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("CastImmediateOnSelf", [&magic, &records](CallContext& call) {
            if (call.mReference != 0 && records.isActor(call.mReference))
                magic.castOnSelf(call.mReference, formArgument(call, 0));
            return 0.0;
        });
        // The command does nothing in the executables
        interpreter.setHandler("Cast", [](CallContext&) { return 1.0; });
        interpreter.setHandler("HasMagicEffect", [&magic, &records](CallContext& call) {
            const FormId effect = formArgument(call, 0);
            return call.mReference != 0 && effect != 0 && records.isActor(call.mReference)
                    && magic.hasEffect(call.mReference, effect)
                ? 1.0
                : 0.0;
        });
        interpreter.setHandler("IsSpellTarget", [&magic](CallContext& call) {
            const FormId spell = formArgument(call, 0);
            return call.mReference != 0 && spell != 0 && magic.isSpellTarget(call.mReference, spell) ? 1.0 : 0.0;
        });
    }
}

#include "magic.hpp"

#include <algorithm>
#include <cmath>

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

    void Magic::setEnder(Ender ender)
    {
        const std::lock_guard lock(mMutex);
        mEnder = std::move(ender);
    }

    void Magic::setConditionInterval(double seconds)
    {
        const std::lock_guard lock(mMutex);
        mConditionInterval = seconds > 0 ? seconds : 1.0;
    }

    bool Magic::holds(FormId actor, const SpellEffect& effect)
    {
        if (effect.mConditions.empty())
            return true;
        ConditionCheck check;
        {
            const std::lock_guard lock(mMutex);
            check = mConditions;
        }
        // The conditions are asked outside the lock: they call back into the host
        ConditionContext context;
        context.mSubject = actor;
        return check && check(effect.mConditions, context);
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
        mSelected = 0;
    }

    bool Magic::addSpell(FormId actor, FormId spell)
    {
        SpellInfo info;
        if (actor == 0 || !mRecords.spell(spell, info))
            return false;
        // The effects that an ability or a disease keeps are all on the list; those whose conditions do not hold do
        // nothing until the conditions are asked again and hold (0x8043F0), and are asked again while the spell lasts
        std::vector<bool> enabled;
        if (appliedWhenAdded(info.mType))
            for (const SpellEffect& effect : info.mEffects)
                enabled.push_back(holds(actor, effect));
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
            for (std::size_t i = 0; i < info.mEffects.size(); ++i)
            {
                const SpellEffect& effect = info.mEffects[i];
                Running running;
                running.mActor = actor;
                running.mEffect = effect.mEffect;
                running.mSource = spell;
                running.mMagnitude = effect.mMagnitude;
                running.mConditions = effect.mConditions;
                running.mEnabled = enabled[i];
                running.mScript = mRecords.effectScript(effect.mEffect);
                running.mId = ++mLastId;
                if (effect.mArchetype == 0 && ActorValues::isValid(effect.mActorValue))
                {
                    running.mActorValue = effect.mActorValue;
                    running.mDelta = effect.mDetrimental ? -effect.mMagnitude : effect.mMagnitude;
                }
                if (running.mEnabled)
                {
                    if (running.mScript != 0)
                        started.emplace_back(running.mScript, running.mId);
                    if (running.mActorValue >= 0)
                        changes.emplace_back(running.mActorValue, running.mDelta);
                }
                mRunning.push_back(std::move(running));
            }
        }
        if (modifier)
            for (const auto& [actorValue, amount] : changes)
                modifier(actor, actorValue, amount);
        if (scripts)
            for (const auto& [script, id] : started)
            {
                // A script that an earlier start took away with its spell has ended already: it does not begin
                if (still(id))
                    scripts(actor, script, ScriptStage::Start, id, 0.0);
            }
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
                    // An effect whose conditions do not hold has done nothing that is to be undone
                    if (it->mEnabled)
                    {
                        if (it->mActorValue >= 0)
                            changes.emplace_back(it->mActorValue, -it->mDelta);
                        if (it->mScript != 0)
                            finished.emplace_back(it->mScript, it->mId);
                    }
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
                scripts(actor, script, ScriptStage::Finish, id, 0.0);
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

    void Magic::castOnSelf(FormId actor, FormId spell, double magnitudeScale)
    {
        SpellInfo info;
        if (actor == 0 || !mRecords.spell(spell, info) || info.mType == typeAbility)
            return;
        // An effect whose conditions do not hold is not on the character, and the caster leaves it out as well
        dropFailing(actor, info);
        if (magnitudeScale != 1.0)
            for (SpellEffect& effect : info.mEffects)
                effect.mMagnitude *= magnitudeScale;
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
                running.mMagnitude = effect.mMagnitude;
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
                scripts(actor, script, ScriptStage::Finish, id, 0.0);
            for (const Started& one : started)
            {
                // The effect of a script that an earlier start took away or renewed (a cast of the same spell from it)
                // has ended already: it does not begin
                if (!one.mAtOnce && !still(one.mId))
                    continue;
                scripts(actor, one.mScript, ScriptStage::Start, one.mId, 0.0);
                if (one.mAtOnce)
                    scripts(actor, one.mScript, ScriptStage::Finish, one.mId, 0.0);
            }
        }
    }

    void Magic::noteEffect(FormId actor, FormId effect, FormId source, double seconds, double magnitude)
    {
        if (actor == 0 || effect == 0 || seconds <= 0)
            return;
        Running running;
        running.mActor = actor;
        running.mEffect = effect;
        running.mSource = source;
        running.mRemaining = seconds;
        running.mMagnitude = magnitude;
        running.mScript = mRecords.effectScript(effect);
        ScriptRunner scripts;
        {
            const std::lock_guard lock(mMutex);
            running.mId = ++mLastId;
            mRunning.push_back(running);
            scripts = mScripts;
        }
        if (scripts && running.mScript != 0)
            scripts(actor, running.mScript, ScriptStage::Start, running.mId, 0.0);
    }

    bool Magic::hasEffect(FormId actor, FormId effect)
    {
        const std::lock_guard lock(mMutex);
        return std::any_of(mRunning.begin(), mRunning.end(), [&](const Running& running) {
            return running.mActor == actor && running.mEffect == effect && running.mEnabled;
        });
    }

    bool Magic::isSpellTarget(FormId actor, FormId spell)
    {
        const std::lock_guard lock(mMutex);
        return std::any_of(mRunning.begin(), mRunning.end(), [&](const Running& running) {
            return running.mActor == actor && running.mSource == spell && running.mEnabled;
        });
    }

    void Magic::update(double seconds)
    {
        ScriptRunner scripts;
        struct Scripted
        {
            FormId mActor;
            FormId mScript;
            EffectId mId;
            /// The seconds of the step that the effect was still running for
            double mSeconds;
        };
        std::vector<Scripted> running;
        std::vector<Scripted> finished;
        {
            const std::lock_guard lock(mMutex);
            scripts = mScripts;
            for (auto it = mRunning.begin(); it != mRunning.end();)
            {
                // An effect that lasts until its spell is removed has no seconds left to count
                const double left = it->mRemaining;
                if (it->mRemaining > 0 && (it->mRemaining -= seconds) <= 0)
                {
                    if (it->mScript != 0)
                        finished.push_back({ it->mActor, it->mScript, it->mId, std::min(left, seconds) });
                    it = mRunning.erase(it);
                }
                else
                {
                    if (it->mScript != 0)
                        running.push_back({ it->mActor, it->mScript, it->mId, seconds });
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
                    scripts(one.mActor, one.mScript, ScriptStage::Update, one.mId, one.mSeconds);
            }
            for (const Scripted& one : finished)
            {
                // The step in which an effect runs out runs its update for the seconds it was still running (a rest
                // passes an hour in one step), and then finishes it
                scripts(one.mActor, one.mScript, ScriptStage::Update, one.mId, one.mSeconds);
                scripts(one.mActor, one.mScript, ScriptStage::Finish, one.mId, 0.0);
            }
        }
        // The conditions of the effects that an ability or a disease keeps are asked again once each interval
        std::vector<EffectId> due;
        {
            const std::lock_guard lock(mMutex);
            for (Running& running : mRunning)
            {
                if (running.mConditions.empty())
                    continue;
                running.mSinceAsked += seconds;
                if (running.mSinceAsked >= mConditionInterval)
                {
                    running.mSinceAsked = std::fmod(running.mSinceAsked, mConditionInterval);
                    due.push_back(running.mId);
                }
            }
        }
        askAgain(due);
    }

    void Magic::askAgain(const std::vector<EffectId>& ids)
    {
        struct Check
        {
            EffectId mId;
            FormId mActor;
            std::vector<ESM4::TargetCondition> mConditions;
            bool mWas;
        };
        std::vector<Check> checks;
        ConditionCheck check;
        {
            const std::lock_guard lock(mMutex);
            check = mConditions;
            for (const EffectId id : ids)
                for (const Running& running : mRunning)
                    if (running.mId == id)
                        checks.push_back({ id, running.mActor, running.mConditions, running.mEnabled });
        }
        struct Change
        {
            FormId mActor;
            FormId mScript;
            EffectId mId;
            int mActorValue;
            double mDelta;
            bool mNowEnabled;
        };
        std::vector<Change> changed;
        for (const Check& one : checks)
        {
            ConditionContext context;
            context.mSubject = one.mActor;
            const bool now = check && check(one.mConditions, context);
            if (now == one.mWas)
                continue;
            const std::lock_guard lock(mMutex);
            for (Running& running : mRunning)
                if (running.mId == one.mId && running.mEnabled != now)
                {
                    running.mEnabled = now;
                    changed.push_back(
                        { running.mActor, running.mScript, running.mId, running.mActorValue, running.mDelta, now });
                }
        }
        Modifier modifier;
        ScriptRunner scripts;
        {
            const std::lock_guard lock(mMutex);
            modifier = mModifier;
            scripts = mScripts;
        }
        // An effect whose conditions hold again begins as it did when it was put on, and one whose conditions stopped
        // holding ends: what it changed is changed back and its script finishes
        for (const Change& one : changed)
        {
            if (one.mNowEnabled)
            {
                if (modifier && one.mActorValue >= 0)
                    modifier(one.mActor, one.mActorValue, one.mDelta);
                if (scripts && one.mScript != 0 && still(one.mId))
                    scripts(one.mActor, one.mScript, ScriptStage::Start, one.mId, 0.0);
            }
            else
            {
                if (modifier && one.mActorValue >= 0)
                    modifier(one.mActor, one.mActorValue, -one.mDelta);
                if (scripts && one.mScript != 0)
                    scripts(one.mActor, one.mScript, ScriptStage::Finish, one.mId, 0.0);
            }
        }
    }

    void Magic::evaluateConditions(FormId actor)
    {
        std::vector<EffectId> ids;
        {
            const std::lock_guard lock(mMutex);
            for (Running& running : mRunning)
                if (running.mActor == actor && !running.mConditions.empty())
                {
                    running.mSinceAsked = 0;
                    ids.push_back(running.mId);
                }
        }
        askAgain(ids);
    }

    int Magic::endEntries(FormId actor, const std::function<bool(const Running&)>& chosen)
    {
        struct Ended
        {
            FormId mSource;
            FormId mEffect;
            FormId mScript;
            EffectId mId;
            int mActorValue;
            double mDelta;
            bool mEnabled;
        };
        std::vector<Ended> ended;
        Modifier modifier;
        ScriptRunner scripts;
        Ender ender;
        {
            const std::lock_guard lock(mMutex);
            modifier = mModifier;
            scripts = mScripts;
            ender = mEnder;
            for (auto it = mRunning.begin(); it != mRunning.end();)
            {
                if (it->mActor == actor && chosen(*it))
                {
                    ended.push_back(
                        { it->mSource, it->mEffect, it->mScript, it->mId, it->mActorValue, it->mDelta, it->mEnabled });
                    it = mRunning.erase(it);
                }
                else
                    ++it;
            }
        }
        // What the effects did is undone, their scripts finish and whoever kept what they changed is told, all outside
        // the lock: they call back into the host
        for (const Ended& one : ended)
        {
            if (one.mEnabled)
            {
                if (modifier && one.mActorValue >= 0 && one.mDelta != 0)
                    modifier(actor, one.mActorValue, -one.mDelta);
                if (scripts && one.mScript != 0)
                    scripts(actor, one.mScript, ScriptStage::Finish, one.mId, 0.0);
            }
            if (ender)
                ender(actor, one.mSource, one.mEffect);
        }
        return static_cast<int>(ended.size());
    }

    int Magic::dispel(FormId actor, FormId source, FormId effect)
    {
        if (actor == 0 || source == 0)
            return 0;
        return endEntries(actor, [&](const Running& running) {
            return running.mSource == source && (effect == 0 || running.mEffect == effect);
        });
    }

    int Magic::dispelAll(FormId actor)
    {
        if (actor == 0)
            return 0;
        return endEntries(actor, [this](const Running& running) {
            // Spells (0), powers (2), lesser powers (3), aid items (7) and ingredients (8) are ended; diseases (1),
            // abilities (4), poisons (5) and addictions (10) are not, and neither are enchantments (6)
            SpellInfo info;
            const int kind = mRecords.spell(running.mSource, info) ? info.mType : mRecords.itemKind(running.mSource);
            return kind == 0 || kind == 2 || kind == 3 || kind == 7 || kind == 8;
        });
    }

    double Magic::usage(FormId actor, FormId source)
    {
        const std::lock_guard lock(mMutex);
        double sum = 0;
        for (const Running& running : mRunning)
            if (running.mActor == actor && running.mSource == source && running.mEnabled)
                sum += running.mMagnitude;
        return sum;
    }

    void Magic::selectSpell(FormId spell)
    {
        const std::lock_guard lock(mMutex);
        mSelected = spell;
    }

    FormId Magic::selectedSpell()
    {
        const std::lock_guard lock(mMutex);
        return mSelected;
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
                // The number the effect has, which the scripts that run for it are kept by (the objects section)
                out.u64(running.mId);
                // Version 3: the magnitude and whether the conditions hold
                out.f64(running.mMagnitude);
                out.boolean(running.mEnabled);
            }
            out.form(magic.mSelected);
        }

        static void read(Magic& magic, SaveReader& in, std::uint32_t version)
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
                if (version >= 2)
                    entry.mId = in.u64();
                if (version >= 3)
                {
                    entry.mMagnitude = in.f64();
                    entry.mEnabled = in.boolean();
                }
                // What comes from a plugin that is gone is left out
                if (actor && effect && source)
                {
                    entry.mActor = *actor;
                    entry.mEffect = *effect;
                    entry.mSource = *source;
                    entry.mScript = magic.mRecords.effectScript(*effect);
                    // What an ability or a disease keeps its effect by is asked of the records again
                    SpellInfo info;
                    if (entry.mRemaining <= 0 && magic.mRecords.spell(*source, info) && appliedWhenAdded(info.mType))
                        for (const SpellEffect& candidate : info.mEffects)
                            if (candidate.mEffect == *effect)
                            {
                                entry.mConditions = candidate.mConditions;
                                break;
                            }
                    running.push_back(entry);
                }
            }
            const std::lock_guard lock(magic.mMutex);
            // A game saved before the effects were numbered in it (version 1) is given numbers now. The others keep
            // theirs, which the saved variables of their scripts are held by, and what runs next gets a number above.
            for (Magic::Running& entry : running)
            {
                if (version < 2 || entry.mId == 0)
                    entry.mId = ++magic.mLastId;
                else
                    magic.mLastId = std::max(magic.mLastId, entry.mId);
            }
            magic.mSpells = std::move(spells);
            magic.mRunning = std::move(running);
            magic.mSelected = 0;
            if (version >= 3)
                magic.mSelected = in.form().value_or(0);
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
        // The effects the magic item (a spell, an aid item) has running on the character end (0x5C20E0)
        interpreter.setHandler("Dispel", [&magic, &records](CallContext& call) {
            if (call.mReference != 0 && records.isActor(call.mReference))
                magic.dispel(call.mReference, formArgument(call, 0));
            return 1.0;
        });
        interpreter.setHandler("DispelAllSpells", [&magic, &records](CallContext& call) {
            if (call.mReference != 0 && records.isActor(call.mReference))
                magic.dispelAll(call.mReference);
            return 1.0;
        });
        interpreter.setHandler("EvaluateSpellConditions", [&magic, &records](CallContext& call) {
            if (call.mReference != 0 && records.isActor(call.mReference))
                magic.evaluateConditions(call.mReference);
            return 1.0;
        });
        interpreter.setHandler("GetSpellUsageNum", [&magic, &records](CallContext& call) {
            return call.mReference != 0 && records.isActor(call.mReference)
                ? magic.usage(call.mReference, formArgument(call, 0))
                : 0.0;
        });
        interpreter.setHandler("SelectPlayerSpell", [&magic](CallContext& call) {
            magic.selectSpell(formArgument(call, 0));
            return 1.0;
        });
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

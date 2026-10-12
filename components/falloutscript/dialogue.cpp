#include "dialogue.hpp"

#include <algorithm>
#include <cctype>
#include <random>

#include <components/esm4/common.hpp>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        bool sameText(const std::string& left, const std::string& right)
        {
            return left.size() == right.size()
                && std::equal(left.begin(), left.end(), right.begin(),
                    [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
        }

        bool hasFlag(const ESM4::DialogInfo& info, std::uint16_t flag)
        {
            return (info.mInfoFlags & flag) != 0;
        }

        bool hasCode(const ESM4::ScriptDefinition& definition)
        {
            return !definition.compiledScript.empty();
        }

        FormId toId(ESM::FormId id)
        {
            return id.isZeroOrUnset() ? 0 : id.toUint32();
        }
    }

    DialogueManager::DialogueManager(
        Interpreter& interpreter, ConditionEvaluator& conditions, QuestManager& quests, const DialogueRecords& records)
        : mInterpreter(interpreter)
        , mConditions(conditions)
        , mQuests(quests)
        , mRecords(records)
    {
        mRandom = [](std::size_t count) {
            static std::mt19937 generator{ std::random_device{}() };
            return static_cast<std::size_t>(generator() % count);
        };
    }

    std::vector<const ESM4::DialogInfo*> DialogueManager::chainOrder(
        const std::vector<const ESM4::DialogInfo*>& inFileOrder)
    {
        // The responses of a topic are a chain: each names the one before it. A response whose predecessor is not in
        // the topic starts a chain, and the responses that name one follow it, which is how a later file puts a
        // response in the middle of a topic.
        std::unordered_map<FormId, std::size_t> indices;
        for (std::size_t i = 0; i < inFileOrder.size(); ++i)
            indices.emplace(toId(inFileOrder[i]->mId), i);

        std::vector<std::vector<std::size_t>> followers(inFileOrder.size());
        std::vector<std::size_t> starts;
        for (std::size_t i = 0; i < inFileOrder.size(); ++i)
        {
            const auto previous = indices.find(toId(inFileOrder[i]->mPreviousInfo));
            if (previous == indices.end() || previous->second == i)
                starts.push_back(i);
            else
                followers[previous->second].push_back(i);
        }

        std::vector<const ESM4::DialogInfo*> result;
        result.reserve(inFileOrder.size());
        std::vector<bool> placed(inFileOrder.size(), false);
        std::vector<std::size_t> stack;
        const auto place = [&](std::size_t start) {
            stack.push_back(start);
            while (!stack.empty())
            {
                const std::size_t index = stack.back();
                stack.pop_back();
                if (placed[index])
                    continue;
                placed[index] = true;
                result.push_back(inFileOrder[index]);
                // The followers go on the stack the other way round, so that the first of them is placed next
                for (auto it = followers[index].rbegin(); it != followers[index].rend(); ++it)
                    stack.push_back(*it);
            }
        };
        for (const std::size_t start : starts)
            place(start);
        // Responses that name each other in a circle have no start
        for (std::size_t i = 0; i < inFileOrder.size(); ++i)
            place(i);
        return result;
    }

    void DialogueManager::reset()
    {
        mEntries.clear();
        mInfoIndex.clear();
        mTopLevel.clear();
        mGiven.clear();
        mTopics.clear();
        mSaid.clear();
        mSaidAt.clear();
        mTalkedTo.clear();
        mChain.clear();
        mPrepared.clear();
        mGreeting = 0;
        mActive = false;
        mView = DialogueView();
        mSpeaking = nullptr;

        // A record that a later file deletes is still in the store, with nothing in it
        mRecords.forEachTopic([this](const ESM4::Dialogue& topic) {
            if ((topic.mFlags & ESM4::Rec_Deleted) == 0)
                mEntries[toId(topic.mId)].mRecord = &topic;
        });

        std::unordered_map<FormId, std::vector<const ESM4::DialogInfo*>> infos;
        mRecords.forEachInfo([&](const ESM4::DialogInfo& info) {
            if ((info.mFlags & ESM4::Rec_Deleted) != 0)
                return;
            mInfoIndex[toId(info.mId)] = &info;
            infos[toId(info.mTopic)].push_back(&info);
        });
        for (auto& [id, entry] : mEntries)
        {
            const auto found = infos.find(id);
            if (found != infos.end())
                entry.mInfos = chainOrder(found->second);
            if (entry.mRecord == nullptr)
                continue;
            // The responses of other topics that the topic shows (New Vegas) are tried after its own, in the order they
            // are listed in. A response that is not there, or that a later file deletes, is left out.
            for (const ESM4::Dialogue::SharedInfo& shared : entry.mRecord->mSharedInfos)
            {
                const auto info = mInfoIndex.find(toId(shared.mInfo));
                if (info != mInfoIndex.end()
                    && std::find(entry.mInfos.begin(), entry.mInfos.end(), info->second) == entry.mInfos.end())
                    entry.mInfos.push_back(info->second);
            }
            if ((entry.mRecord->mDialFlags & ESM4::Dialogue::Flag_TopLevel) != 0)
                mTopLevel.push_back(id);
            if (sameText(entry.mRecord->mEditorId, greetingTopic))
                mGreeting = id;
        }
        // The order of the topics that have the same priority is that of their ids, which is stable and does not
        // depend on the hash
        std::sort(mTopLevel.begin(), mTopLevel.end());
        std::stable_sort(mTopLevel.begin(), mTopLevel.end(), [this](FormId left, FormId right) {
            return mEntries[left].mRecord->mPriority > mEntries[right].mRecord->mPriority;
        });
    }

    void DialogueManager::addTopic(FormId topic)
    {
        if (mTopics.insert(topic).second)
            mGiven.push_back(topic);
    }

    std::vector<const ESM4::DialogInfo*> DialogueManager::responses(FormId topic) const
    {
        const auto found = mEntries.find(topic);
        return found == mEntries.end() ? std::vector<const ESM4::DialogInfo*>() : found->second.mInfos;
    }

    const ESM4::Dialogue* DialogueManager::topic(FormId topic) const
    {
        const auto found = mEntries.find(topic);
        return found == mEntries.end() ? nullptr : found->second.mRecord;
    }

    FormId DialogueManager::findTopic(const std::string& editorId) const
    {
        for (const auto& [id, entry] : mEntries)
            if (entry.mRecord != nullptr && sameText(entry.mRecord->mEditorId, editorId))
                return id;
        return 0;
    }

    bool DialogueManager::canSay(const ESM4::DialogInfo& info, const DialogueSpeaker& speaker)
    {
        const FormId id = toId(info.mId);
        if (const FormId quest = toId(info.mQuest); quest != 0 && !mQuests.running(quest))
            return false;
        // What a character said is kept for that character: another can still say it
        const std::pair<FormId, FormId> key{ speaker.mReference, id };
        if (hasFlag(info, ESM4::INFO_SayOnce) && mSaid.count(key) != 0)
            return false;
        if (hasFlag(info, ESM4::INFO_SayOnceADay))
        {
            const auto said = mSaidAt.find(key);
            if (said != mSaidAt.end() && hours() - said->second < mDailyInterval)
                return false;
        }
        if (const FormId only = toId(info.mSpeaker); only != 0 && only != speaker.mBase)
            return false;
        if (hasFlag(info, ESM4::INFO_SpeechChallenge) && mChallenge && !mChallenge(info))
            return false;
        ConditionContext context;
        context.mSubject = speaker.mReference;
        context.mTarget = mListener;
        return mConditions.holds(info.mConditions, context);
    }

    const ESM4::DialogInfo* DialogueManager::responseFor(FormId topic, const DialogueSpeaker& speaker)
    {
        const auto found = mEntries.find(topic);
        if (found == mEntries.end())
            return nullptr;
        const std::vector<const ESM4::DialogInfo*>& infos = found->second.mInfos;
        for (std::size_t i = 0; i < infos.size(); ++i)
        {
            const ESM4::DialogInfo& info = *infos[i];
            if (!canSay(info, speaker))
                continue;
            if (!hasFlag(info, ESM4::INFO_Random))
                return &info;

            // The responses that can be said and are flagged Random that follow this one, up to the one flagged Random
            // End or the first that can be said and is not Random, are a set to pick from. A response that cannot be
            // said is passed over, whatever its flags.
            std::vector<const ESM4::DialogInfo*> candidates{ &info };
            if (!hasFlag(info, ESM4::INFO_RandomEnd))
            {
                for (std::size_t j = i + 1; j < infos.size(); ++j)
                {
                    const ESM4::DialogInfo& next = *infos[j];
                    if (!canSay(next, speaker))
                        continue;
                    if (!hasFlag(next, ESM4::INFO_Random))
                        break;
                    candidates.push_back(&next);
                    if (hasFlag(next, ESM4::INFO_RandomEnd))
                        break;
                }
            }
            return candidates[candidates.size() == 1 ? 0 : mRandom(candidates.size())];
        }
        return nullptr;
    }

    bool DialogueManager::addOption(std::vector<DialogueOption>& options, FormId topic, const DialogueSpeaker& speaker)
    {
        const auto found = mEntries.find(topic);
        if (found == mEntries.end() || found->second.mRecord == nullptr)
            return false;
        for (const DialogueOption& option : options)
            if (option.mTopic == topic)
                return false;
        const ESM4::DialogInfo* info = responseFor(topic, speaker);
        if (info == nullptr)
            return false;
        DialogueOption& option = options.emplace_back();
        option.mTopic = topic;
        option.mInfo = toId(info->mId);
        option.mText = info->mPrompt.empty() ? found->second.mRecord->mTopicName : info->mPrompt;
        return true;
    }

    std::vector<DialogueOption> DialogueManager::options(
        const DialogueSpeaker& speaker, const std::vector<ESM::FormId>* choices)
    {
        std::vector<DialogueOption> result;
        if (choices != nullptr)
        {
            for (const ESM::FormId choice : *choices)
                addOption(result, toId(choice), speaker);
            return result;
        }
        for (const FormId topic : mTopLevel)
            addOption(result, topic, speaker);
        for (const FormId topic : mGiven)
            addOption(result, topic, speaker);
        return result;
    }

    void DialogueManager::runScript(const ESM4::ScriptDefinition& definition, const DialogueSpeaker& speaker)
    {
        if (!hasCode(definition))
            return;
        auto& script = mPrepared[&definition];
        if (!script)
            script = std::make_shared<const Script>(Script::prepare(definition, mInterpreter.commands()));
        if (!script->usable())
        {
            mInterpreter.host().log("A script of a response cannot be run: " + script->problemText());
            return;
        }
        Instance instance(script, speaker.mReference, false);
        mInterpreter.runResult(instance);
    }

    DialogueView DialogueManager::speak(const ESM4::DialogInfo& info, const DialogueSpeaker& speaker, FormId topic)
    {
        const FormId id = toId(info.mId);
        DialogueView view;
        view.mSpeaker = speaker.mReference;
        // A response that a topic shows for another one is said under the topic that was picked
        view.mTopic = topic != 0 ? topic : toId(info.mTopic);
        view.mInfo = id;
        view.mSpeaking = true;
        for (const ESM4::DialogResponse& response : info.mResponses)
        {
            DialogueLine& line = view.mLines.emplace_back();
            line.mText = response.mText;
            line.mInfo = id;
            line.mNumber = response.mNumber;
            line.mEmotion = response.mEmotionType;
            line.mEmotionValue = response.mEmotionValue;
            line.mSound = toId(response.mSound);
            line.mSpeakerAnimation = toId(response.mSpeakerAnimation);
            line.mListenerAnimation = toId(response.mListenerAnimation);
        }
        // The script that runs when the response starts. The view is kept for the commands that ask what is said.
        const DialogueView outer = mView;
        mView = view;
        runScript(info.mScript, speaker);
        mView = outer;
        return view;
    }

    void DialogueManager::markSaid(const ESM4::DialogInfo& info, const DialogueSpeaker& speaker)
    {
        const std::pair<FormId, FormId> key{ speaker.mReference, toId(info.mId) };
        mSaid.insert(key);
        if (hasFlag(info, ESM4::INFO_SayOnceADay))
            mSaidAt[key] = hours();
    }

    const DialogueView& DialogueManager::start(const DialogueSpeaker& speaker, FormId topic)
    {
        if (mActive)
            return mView;
        mActive = true;
        mSpeaker = speaker;
        mSpeaking = nullptr;
        mChain.clear();
        mView = DialogueView();
        mView.mSpeaker = speaker.mReference;

        // The greeting of a first meeting asks whether the player has talked to the speaker (GetTalkedToPC is 0 for
        // it), so the speaker has been talked to once the greeting is chosen
        const FormId first = topic != 0 ? topic : mGreeting;
        const ESM4::DialogInfo* greeting = first != 0 ? responseFor(first, speaker) : nullptr;
        const bool firstMeeting = mTalkedTo.insert(speaker.mReference).second;
        if (greeting != nullptr)
        {
            mView = speak(*greeting, speaker, first);
            mSpeaking = greeting;
            mChain.insert(toId(greeting->mId));
        }
        else
            mView.mOptions = options(speaker, nullptr);
        // A speaker with nothing to say and nothing to pick from does not talk (the host ends the conversation), so
        // the player has not talked to it
        if (firstMeeting && mView.mLines.empty() && mView.mOptions.empty())
            mTalkedTo.erase(speaker.mReference);
        return mView;
    }

    const DialogueView& DialogueManager::choose(FormId topic)
    {
        if (!mActive || mSpeaking != nullptr)
            return mView;
        const auto option = std::find_if(mView.mOptions.begin(), mView.mOptions.end(),
            [topic](const DialogueOption& candidate) { return candidate.mTopic == topic; });
        if (option == mView.mOptions.end())
            return mView;

        // The response is the one the player saw the words of, unless what was true then is not now
        const ESM4::DialogInfo* info = nullptr;
        if (const auto found = mInfoIndex.find(option->mInfo);
            found != mInfoIndex.end() && canSay(*found->second, mSpeaker))
            info = found->second;
        if (info == nullptr)
            info = responseFor(topic, mSpeaker);
        if (info == nullptr)
            return mView;

        mChain = { toId(info->mId) };
        mView = speak(*info, mSpeaker, topic);
        mSpeaking = info;
        return mView;
    }

    const DialogueView& DialogueManager::finish()
    {
        if (!mActive || mSpeaking == nullptr)
            return mView;
        const ESM4::DialogInfo& info = *mSpeaking;
        mSpeaking = nullptr;
        mView.mSpeaking = false;

        markSaid(info, mSpeaker);
        runScript(info.mEndScript, mSpeaker);
        for (const ESM::FormId added : info.mAddTopics)
            addTopic(toId(added));

        if (hasFlag(info, ESM4::INFO_Goodbye))
        {
            mActive = false;
            mView.mEnded = true;
            mView.mOptions.clear();
            return mView;
        }

        // A response that goes on is followed by the first of its follow-ups that can be said, before the player has a
        // say. None is said twice in the exchange, so follow-ups that name each other in a circle end. The character
        // says them all: the next speaker of a response is Target in every record of the files that has follow-ups
        // (its default), and the followers are voiced by characters, never by the player.
        for (const ESM::FormId followUp : info.mFollowUps)
        {
            const auto next = mInfoIndex.find(toId(followUp));
            if (next == mInfoIndex.end() || mChain.count(next->first) != 0 || !canSay(*next->second, mSpeaker))
                continue;
            mChain.insert(next->first);
            mView = speak(*next->second, mSpeaker);
            mSpeaking = next->second;
            return mView;
        }

        const std::vector<ESM::FormId>* choices = info.mChoices.empty() ? nullptr : &info.mChoices;
        mView.mOptions = options(mSpeaker, choices);
        // Choices that none of the responses can follow leave the player with the list as it is
        if (choices != nullptr && mView.mOptions.empty())
            mView.mOptions = options(mSpeaker, nullptr);
        return mView;
    }

    void DialogueManager::end()
    {
        if (!mActive)
            return;
        mActive = false;
        mSpeaking = nullptr;
        mView.mSpeaking = false;
        mView.mEnded = true;
        mView.mOptions.clear();
    }

    DialogueView DialogueManager::say(const DialogueSpeaker& speaker, FormId topic, FormId listener, bool followUps)
    {
        // The listener matters for the choice of the responses (their conditions) and for nothing after it
        const FormId outerListener = mListener;
        mListener = listener;
        struct Restore
        {
            FormId& mTarget;
            FormId mValue;
            ~Restore() { mTarget = mValue; }
        } restore{ mListener, outerListener };

        const ESM4::DialogInfo* info = responseFor(topic, speaker);
        if (info == nullptr)
            return DialogueView();
        DialogueView view = speak(*info, speaker, topic);
        view.mSpeaking = false;
        DialogueView current = view; // the response that was said last
        std::set<FormId> chain;
        while (info != nullptr)
        {
            chain.insert(toId(info->mId));
            markSaid(*info, speaker);
            // As in finish(), the end script sees the response that was said
            const DialogueView outer = mView;
            mView = current;
            runScript(info->mEndScript, speaker);
            mView = outer;
            for (const ESM::FormId added : info->mAddTopics)
                addTopic(toId(added));

            // The first follow-up that can be said goes on, none is said twice, and a goodbye is the end
            const ESM4::DialogInfo* next = nullptr;
            for (const ESM::FormId followUp : info->mFollowUps)
            {
                const auto found = mInfoIndex.find(toId(followUp));
                if (!followUps || hasFlag(*info, ESM4::INFO_Goodbye) || found == mInfoIndex.end()
                    || chain.count(found->first) != 0 || !canSay(*found->second, speaker))
                    continue;
                next = found->second;
                break;
            }
            info = next;
            if (info != nullptr)
            {
                current = speak(*info, speaker, topic);
                current.mSpeaking = false;
                view.mLines.insert(view.mLines.end(), current.mLines.begin(), current.mLines.end());
            }
        }
        return view;
    }

    void addDialogueCommands(Interpreter& interpreter, DialogueManager& dialogue)
    {
        interpreter.setHandler("AddTopic", [&dialogue](CallContext& call) {
            dialogue.addTopic(formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("GetTalkedToPC",
            [&dialogue](CallContext& call) { return dialogue.talkedToPlayer(call.mReference) ? 1.0 : 0.0; });
        interpreter.setHandler("GetTalkedToPCParam",
            [&dialogue](CallContext& call) { return dialogue.talkedToPlayer(formArgument(call, 0)) ? 1.0 : 0.0; });
        interpreter.setHandler("IsTalking", [&dialogue](CallContext& call) {
            return dialogue.active() && dialogue.speaker().mReference == call.mReference ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetDialogueEmotion", [&dialogue](CallContext&) {
            const DialogueView& view = dialogue.view();
            return view.mLines.empty() ? 0.0 : static_cast<double>(view.mLines.back().mEmotion);
        });
        interpreter.setHandler("GetDialogueEmotionValue", [&dialogue](CallContext&) {
            const DialogueView& view = dialogue.view();
            return view.mLines.empty() ? 0.0 : static_cast<double>(view.mLines.back().mEmotionValue);
        });
    }
}

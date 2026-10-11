#include "conversationbindings.hpp"

#include <stdexcept>
#include <string>

#include <components/esm/refid.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwscript/falloutscripts.hpp"

#include "context.hpp"

namespace OFLua
{
    namespace
    {
        using FalloutScript::FormId;

        OFScript::FalloutScripts& scripts()
        {
            return *OFBase::Environment::get().getFalloutScripts();
        }

        std::string idText(FormId id)
        {
            return ESM::RefId::formIdRefId(ESM::FormId::fromUint32(id)).serializeText();
        }

        FormId parseId(std::string_view text)
        {
            const ESM::RefId id = ESM::RefId::deserializeText(text);
            const ESM::FormId* form = id.getIf<ESM::FormId>();
            if (form == nullptr)
                throw std::runtime_error("\"" + std::string(text) + "\" is not a form id");
            return form->toUint32();
        }

        sol::table view(const sol::state_view& lua)
        {
            const OFScript::FalloutScripts::Conversation conversation = scripts().conversation();
            const FalloutScript::DialogueView& shown = conversation.mView;

            sol::table result(lua, sol::create);
            result["active"] = conversation.mActive;
            result["speaking"] = shown.mSpeaking;
            result["ended"] = shown.mEnded;
            result["duration"] = conversation.mDuration;
            result["speakerId"] = shown.mSpeaker == 0 ? std::string() : idText(shown.mSpeaker);
            result["speakerName"] = conversation.mSpeakerName;
            result["topic"] = shown.mTopic == 0 ? std::string() : idText(shown.mTopic);
            result["response"] = shown.mInfo == 0 ? std::string() : idText(shown.mInfo);

            sol::table lines(lua, sol::create);
            int index = 1;
            for (const FalloutScript::DialogueLine& line : shown.mLines)
            {
                sol::table row(lua, sol::create);
                row["text"] = line.mText;
                row["emotion"] = line.mEmotion;
                row["emotionValue"] = line.mEmotionValue;
                lines[index++] = row;
            }
            result["lines"] = lines;

            sol::table options(lua, sol::create);
            index = 1;
            for (const FalloutScript::DialogueOption& option : shown.mOptions)
            {
                sol::table row(lua, sol::create);
                row["topic"] = idText(option.mTopic);
                row["text"] = option.mText;
                options[index++] = row;
            }
            result["options"] = options;
            return result;
        }
    }

    sol::table initCoreConversationBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        // Whether the loaded content has dialogue of Fallout, which the window of the conversation shows
        api["available"] = []() { return scripts().hasDialogue(); };
        api["get"] = [lua]() { return view(lua); };
        api["choose"] = [lua](std::string_view topic) {
            scripts().chooseOption(parseId(topic));
            return view(lua);
        };
        api["finish"] = [lua]() {
            scripts().finishResponse();
            return view(lua);
        };
        api["close"] = [lua]() {
            scripts().endConversation();
            return view(lua);
        };
        return LuaUtil::makeReadOnly(api);
    }
}

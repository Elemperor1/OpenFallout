#include "questbindings.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

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

        struct QuestRef
        {
            FormId mId = 0;
        };

        /// world.globals
        struct GlobalsRef
        {
        };

        OFScript::FalloutScripts& scripts()
        {
            return *OFBase::Environment::get().getFalloutScripts();
        }

        std::string idText(FormId id)
        {
            return ESM::RefId::formIdRefId(ESM::FormId::fromUint32(id)).serializeText();
        }

        const FalloutScript::QuestState& state(const QuestRef& quest)
        {
            const FalloutScript::QuestState* result = scripts().quests().state(quest.mId);
            if (result == nullptr)
                throw std::runtime_error("The quest is not in the loaded content");
            return *result;
        }

        int stageNumber(double value)
        {
            if (std::isnan(value))
                return 0;
            return static_cast<int>(std::clamp(std::trunc(value), -2147483648.0, 2147483647.0));
        }

        FormId globalId(std::string_view name)
        {
            const FormId id = scripts().findGlobal(name);
            if (id == 0)
                throw std::runtime_error("No global variable \"" + std::string(name) + "\"");
            return id;
        }
    }
}

namespace sol
{
    template <>
    struct is_automagical<OFLua::QuestRef> : std::false_type
    {
    };
    template <>
    struct is_automagical<OFLua::GlobalsRef> : std::false_type
    {
    };
}

namespace OFLua
{
    void addQuestBindings(sol::table& api, const Context& context)
    {
        sol::state_view lua = context.sol();

        sol::usertype<QuestRef> quest = lua.new_usertype<QuestRef>("FalloutQuest");
        quest[sol::meta_function::to_string]
            = [](const QuestRef& ref) { return "FalloutQuest[" + scripts().editorId(ref.mId) + "]"; };
        quest[sol::meta_function::equal_to] = [](const QuestRef& left, const QuestRef& right) {
            return left.mId == right.mId;
        };
        quest["id"] = sol::readonly_property([](const QuestRef& ref) { return idText(ref.mId); });
        quest["editorId"] = sol::readonly_property([](const QuestRef& ref) { return state(ref).mRecord->mEditorId; });
        quest["name"] = sol::readonly_property([](const QuestRef& ref) { return state(ref).mRecord->mQuestName; });
        // The stage that was set last, 0 when none was
        quest["stage"] = sol::readonly_property([](const QuestRef& ref) { return state(ref).mStage; });
        quest["running"] = sol::readonly_property([](const QuestRef& ref) { return state(ref).mRunning; });
        quest["completed"] = sol::readonly_property([](const QuestRef& ref) { return state(ref).mCompleted; });
        quest["failed"] = sol::readonly_property([](const QuestRef& ref) { return state(ref).mFailed; });
        quest["stageDone"] = [](const QuestRef& ref, double stage) {
            return scripts().quests().stageDone(ref.mId, stageNumber(stage));
        };
        quest["setStage"] = [](const QuestRef& ref, double stage) {
            return scripts().quests().setStage(ref.mId, stageNumber(stage));
        };
        quest["start"] = [](const QuestRef& ref) { scripts().quests().start(ref.mId); };
        quest["stop"] = [](const QuestRef& ref) { scripts().quests().stop(ref.mId); };
        quest["complete"] = [](const QuestRef& ref) { scripts().quests().complete(ref.mId); };
        quest["objectiveDisplayed"] = [](const QuestRef& ref, double objective) {
            return scripts().quests().objectiveDisplayed(ref.mId, stageNumber(objective));
        };
        quest["objectiveCompleted"] = [](const QuestRef& ref, double objective) {
            return scripts().quests().objectiveCompleted(ref.mId, stageNumber(objective));
        };
        // The text of the journal so far, one entry per stage that was reached and had a text
        quest["journal"] = sol::readonly_property([lua](const QuestRef& ref) {
            sol::table entries(lua, sol::create);
            int index = 1;
            for (const FalloutScript::JournalEntry& entry : state(ref).mJournal)
            {
                sol::table row(lua, sol::create);
                row["stage"] = entry.mStage;
                row["text"] = entry.mText;
                entries[index++] = row;
            }
            return entries;
        });

        sol::table quests(lua, sol::create);
        // The quest of that editor id or form id (0x followed by hex digits), or nil
        quests["find"] = [](std::string_view name) -> sol::optional<QuestRef> {
            const FormId id = scripts().findQuest(name);
            if (id == 0)
                return sol::nullopt;
            return QuestRef{ id };
        };
        api["quests"] = LuaUtil::makeReadOnly(quests);

        sol::usertype<GlobalsRef> globals = lua.new_usertype<GlobalsRef>("FalloutGlobals");
        globals[sol::meta_function::to_string] = [](const GlobalsRef&) { return "FalloutGlobals"; };
        globals[sol::meta_function::index] = [](const GlobalsRef&, std::string_view name) -> sol::optional<double> {
            const FormId id = scripts().findGlobal(name);
            double value = 0;
            if (id == 0 || !scripts().getGlobal(id, value))
                return sol::nullopt;
            return value;
        };
        globals[sol::meta_function::new_index] = [](const GlobalsRef&, std::string_view name, double value) {
            scripts().setGlobal(globalId(name), value);
        };
        api["globals"] = GlobalsRef{};
    }
}

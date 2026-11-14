#include "spawnbindings.hpp"

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include <components/esm/refid.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwscript/falloutscripts.hpp"

#include "context.hpp"
#include "luamanagerimp.hpp"

namespace OFLua
{
    namespace
    {
        OFScript::FalloutScripts& scripts()
        {
            OFScript::FalloutScripts* host = OFBase::Environment::get().getFalloutScripts();
            if (host == nullptr)
                throw std::runtime_error("core.spawn is only available with the content of Fallout 3 and New Vegas");
            return *host;
        }

        /// A form id from Lua: the text that a reference has as its `id` ("0x" and the number in hexadecimal), or the
        /// text that a record or a reference has as its serialized id ("FormId:0x" and the number)
        FalloutScript::FormId parseId(std::string_view text)
        {
            if (text.empty())
                return 0;
            if (text.starts_with("0x"))
            {
                std::uint32_t value = 0;
                const char* end = text.data() + text.size();
                const auto [stop, error] = std::from_chars(text.data() + 2, end, value, 16);
                if (error != std::errc() || stop != end)
                    throw std::runtime_error("\"" + std::string(text) + "\" is not a form id");
                return value;
            }
            const ESM::RefId id = ESM::RefId::deserializeText(text);
            const ESM::FormId* form = id.getIf<ESM::FormId>();
            if (form == nullptr)
                throw std::runtime_error("\"" + std::string(text) + "\" is not a form id");
            return form->toUint32();
        }

        std::string hexText(FalloutScript::FormId id)
        {
            char buffer[16];
            const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), id, 16);
            return "0x" + std::string(buffer, end);
        }
    }

    sol::table initCoreSpawnBindings(const Context& context)
    {
        sol::state_view lua = context.mLua->unsafeState();
        sol::table api(lua, sol::create);

        api["available"] = []() { return OFBase::Environment::get().getFalloutScripts() != nullptr; };

        // The reference of the player as scripts know it (the form id of the player's placed reference)
        api["player"] = []() { return hexText(scripts().playerReference()); };

        // Calls the script command of that name on the reference (a form id text, or nil for no reference) with the
        // arguments (numbers, or form id texts), the way a script or a condition does, and gives its value
        api["run"] = [](std::string_view name, sol::optional<std::string> reference,
                         sol::optional<sol::table> arguments) -> double {
            OFScript::FalloutScripts& host = scripts();
            const FalloutScript::CommandInfo* info = host.interpreter().commands().find(name);
            if (info == nullptr)
                throw std::runtime_error("There is no script command " + std::string(name));
            const FalloutScript::FormId target = reference ? parseId(*reference) : 0;
            std::vector<FalloutScript::Value> values;
            if (arguments)
                for (std::size_t index = 1;; ++index)
                {
                    const sol::object item = (*arguments)[index];
                    if (!item.valid() || item.get_type() == sol::type::lua_nil)
                        break;
                    FalloutScript::Value value;
                    if (item.get_type() == sol::type::number)
                        value.mNumber = item.as<double>();
                    else if (item.get_type() == sol::type::string)
                        value.mNumber = static_cast<double>(parseId(item.as<std::string_view>()));
                    else
                        throw std::runtime_error("An argument is neither a number nor a form id");
                    values.push_back(std::move(value));
                }
            return host.interpreter().call(info->mOpcode, target, target != 0, std::move(values));
        };

        // What a holder has: a list of { id, count } (the form number as text), nil when the host can not tell yet
        api["items"] = [lua](std::string holder) -> sol::object {
            std::vector<std::pair<FalloutScript::FormId, int>> items;
            if (!scripts().actors().items(parseId(holder), items))
                return sol::nil;
            sol::table list(lua, sol::create);
            std::size_t index = 1;
            for (const auto& [form, count] : items)
            {
                sol::table row(lua, sol::create);
                row["id"] = hexText(form);
                row["count"] = count;
                list[index++] = row;
            }
            return list;
        };
        api["count"] = [](std::string holder, std::string item) {
            return scripts().actors().itemCount(parseId(holder), parseId(item));
        };

        // The level and the level modifier of the placement of a reference that scripts made (nil for any other)
        api["placement"] = [lua](std::string reference) -> sol::object {
            int level = 0;
            int levelMod = 0;
            if (!scripts().spawnLevels().find(parseId(reference), level, levelMod))
                return sol::nil;
            sol::table row(lua, sol::create);
            row["level"] = level;
            row["levelMod"] = levelMod;
            return row;
        };

        // Whether the actor is dead, and the game hour it died at (nil when it did not die since this game began)
        api["dead"] = [](std::string reference) { return scripts().actors().isDead(parseId(reference)); };
        api["deathHour"] = [](std::string reference) -> sol::optional<double> {
            std::uint32_t when = 0;
            if (scripts().respawn().deathTime(parseId(reference), when))
                return static_cast<double>(when);
            return sol::nullopt;
        };
        // How often the inventory of the holder was rebuilt (ResetInventory, a respawn)
        api["generation"]
            = [](std::string holder) { return scripts().respawn().inventoryGeneration(parseId(holder)); };
        // The form a reference was made from, as text; nil when the host can not tell
        api["base"] = [](std::string reference) -> sol::optional<std::string> {
            FalloutScript::FormId base = 0;
            if (!scripts().actors().baseOf(parseId(reference), base))
                return sol::nullopt;
            return hexText(base);
        };
        // Whether the record of the character or creature makes it come back when its cell resets
        api["respawns"] = [](std::string reference) { return scripts().respawnsByRecord(parseId(reference)); };
        // Whether the record of the container makes it fill again when its cell resets
        api["containerRespawns"]
            = [](std::string reference) { return scripts().respawnsContainerByRecord(parseId(reference)); };
        // The records the template chain of a character or creature goes through (its own base first) for the roll it
        // has: what a levelled template gave, as form id texts
        api["template"] = [lua](std::string reference) {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const FalloutScript::FormId id : scripts().templateChainOf(parseId(reference)))
                rows[index++] = hexText(id);
            return rows;
        };
        // Asks for a save as the quick save key makes it, in the folder of saves the run has (for the rounds that show a
        // save and a load: the run's saves are its own copy); it is made at the start of the next frame
        api["quickSave"] = []() { scripts().requestQuickSave(); };
        // Whether the zone never resets its cells (the flag of its record, or what SetZoneRespawns made of it)
        api["zoneNeverResets"] = [](std::string zone) { return scripts().zoneNeverResets(parseId(zone)); };
        // The value that the trades use for an item (what SetItemValue gave it, else the value of its record)
        api["itemValue"] = [](std::string item) { return scripts().itemValueOf(parseId(item)); };
        // How many entries scripts added to the list
        api["additions"] = [](std::string list) {
            return static_cast<int>(scripts().levelledLists().additions(parseId(list)).size());
        };
        return api;
    }
}

#include "chargenbindings.hpp"

#include <stdexcept>
#include <string>

#include <components/esm/refid.hpp>
#include <components/falloutscript/chargen.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwscript/falloutscripts.hpp"

#include "context.hpp"

namespace OFLua
{
    namespace
    {
        using FalloutScript::Chargen;
        using FalloutScript::ChargenPage;
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

        const char* pageName(ChargenPage page)
        {
            switch (page)
            {
                case ChargenPage::Name:
                    return "name";
                case ChargenPage::Race:
                    return "race";
                case ChargenPage::Special:
                    return "special";
                case ChargenPage::Skills:
                    return "skills";
                case ChargenPage::Traits:
                    return "traits";
            }
            return "";
        }

        struct Attribute
        {
            const char* mEditorId;
            const char* mName;
        };

        // In the order of the numbers 5 to 11
        constexpr Attribute attributes[Chargen::attributeCount] = {
            { "Strength", "Strength" },
            { "Perception", "Perception" },
            { "Endurance", "Endurance" },
            { "Charisma", "Charisma" },
            { "Intelligence", "Intelligence" },
            { "Agility", "Agility" },
            { "Luck", "Luck" },
        };

        sol::table state(const sol::state_view& lua)
        {
            OFScript::FalloutScripts& host = scripts();
            Chargen& chargen = host.chargen();

            sol::table result(lua, sol::create);
            result["name"] = chargen.name();
            // What the player is now: what it picked, and else what its record says
            FalloutScript::ActorTraits traits;
            const bool known = host.playerTraits(traits);
            result["female"] = chargen.female().value_or(known && traits.mFemale);
            const FormId race = chargen.race() != 0 ? chargen.race() : (known ? traits.mRace : 0);
            result["race"] = race == 0 ? std::string() : idText(race);
            result["pointsLeft"] = chargen.pointsLeft();
            result["minimum"] = Chargen::minimumAttribute;
            result["maximum"] = Chargen::maximumAttribute;
            result["tagLimit"] = chargen.tagsAllowed();
            result["traitLimit"] = Chargen::traitLimit;
            result["nameLimit"] = static_cast<int>(Chargen::nameLimit);

            sol::table attributeRows(lua, sol::create);
            for (int i = 0; i < Chargen::attributeCount; ++i)
            {
                const OFScript::FalloutScripts::ActorValueText text
                    = host.actorValueText(attributes[i].mEditorId, attributes[i].mName);
                sol::table row(lua, sol::create);
                row["number"] = Chargen::firstAttribute + i;
                row["name"] = text.mName;
                row["description"] = text.mDescription;
                row["value"] = chargen.attribute(Chargen::firstAttribute + i);
                attributeRows[i + 1] = row;
            }
            result["attributes"] = attributeRows;

            sol::table skillRows(lua, sol::create);
            int index = 1;
            for (const FalloutScript::ChargenSkill& skill : chargen.skills())
            {
                const OFScript::FalloutScripts::ActorValueText text = host.actorValueText(skill.mEditorId, skill.mName);
                sol::table row(lua, sol::create);
                row["number"] = skill.mNumber;
                row["name"] = text.mName;
                row["description"] = text.mDescription;
                row["attribute"] = skill.mAttribute;
                row["value"] = chargen.skillValue(skill.mNumber);
                row["tagged"] = chargen.tagged(skill.mNumber);
                skillRows[index++] = row;
            }
            result["skills"] = skillRows;
            result["tagsLeft"] = chargen.tagsAllowed() - static_cast<int>(chargen.tags().size());

            sol::table traitRows(lua, sol::create);
            index = 1;
            for (const OFScript::FalloutScripts::ChargenTrait& trait : host.chargenTraits())
            {
                sol::table row(lua, sol::create);
                row["id"] = idText(trait.mId);
                row["name"] = trait.mName;
                row["description"] = trait.mDescription;
                row["picked"] = chargen.hasTrait(trait.mId);
                traitRows[index++] = row;
            }
            result["traits"] = traitRows;
            result["traitsLeft"] = Chargen::traitLimit - static_cast<int>(chargen.traits().size());

            sol::table raceRows(lua, sol::create);
            index = 1;
            for (const OFScript::FalloutScripts::ChargenRace& entry : host.playableRaces())
            {
                sol::table row(lua, sol::create);
                row["id"] = idText(entry.mId);
                row["name"] = entry.mName;
                row["selected"] = entry.mId == race;
                raceRows[index++] = row;
            }
            result["races"] = raceRows;
            return result;
        }
    }

    sol::table initCoreChargenBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        // The screen that is open: a table with its page ("name", "race", "special", "skills" or "traits"), a number
        // that is another for each screen that opens, and the values the menu was asked for with. Nothing when no menu
        // is open.
        api["page"] = [lua]() -> sol::object {
            Chargen& chargen = scripts().chargen();
            const std::optional<ChargenPage> page = chargen.page();
            if (!page)
                return sol::nil;
            sol::table result(lua, sol::create);
            result["page"] = pageName(*page);
            result["serial"] = chargen.serial();
            sol::table values(lua, sol::create);
            int index = 1;
            for (const double value : chargen.values())
                values[index++] = value;
            result["values"] = values;
            return result;
        };
        // How the character is now, with what the screens list
        api["state"] = [lua]() { return state(lua); };
        // The name is cleaned and cut to fit; the one that is kept is told
        api["setName"] = [](std::string_view name) { return scripts().chargen().setName(name); };
        api["setFemale"] = [](bool female) { scripts().chargen().setFemale(female); };
        api["setRace"] = [](std::string_view race) { scripts().chargen().setRace(parseId(race)); };
        // Gives (or with a negative amount takes) points of an attribute; false when they can not be given
        api["adjust"] = [](int attribute, int delta) { return scripts().chargen().adjustAttribute(attribute, delta); };
        api["toggleTag"] = [](int skill) { return scripts().chargen().toggleTag(skill); };
        api["toggleTrait"] = [](std::string_view trait) { return scripts().chargen().toggleTrait(parseId(trait)); };
        // The player is done with the screen: what it picked becomes the character and the next screen opens
        api["finish"] = []() { scripts().finishChargenPage(); };
        return LuaUtil::makeReadOnly(api);
    }
}

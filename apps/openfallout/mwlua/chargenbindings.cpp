#include "chargenbindings.hpp"

#include <stdexcept>
#include <string>

#include <components/esm/refid.hpp>
#include <components/falloutscript/chargen.hpp>
#include <components/falloutscript/facelook.hpp>
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
            { "AVStrength", "Strength" },
            { "AVPerception", "Perception" },
            { "AVEndurance", "Endurance" },
            { "AVCharisma", "Charisma" },
            { "AVIntelligence", "Intelligence" },
            { "AVAgility", "Agility" },
            { "AVLuck", "Luck" },
        };

        using FalloutScript::FaceKind;

        FaceKind parseKind(std::string_view text)
        {
            if (text == "shape")
                return FaceKind::Shape;
            if (text == "tone")
                return FaceKind::Tone;
            throw std::runtime_error("\"" + std::string(text) + "\" is not a kind of slider (\"shape\" or \"tone\")");
        }

        sol::table choiceRows(const sol::state_view& lua, const std::vector<OFScript::FalloutScripts::LookChoice>& rows)
        {
            sol::table result(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::LookChoice& entry : rows)
            {
                sol::table row(lua, sol::create);
                row["id"] = idText(entry.mId);
                row["name"] = entry.mName;
                row["selected"] = entry.mSelected;
                result[index++] = row;
            }
            return result;
        }

        sol::table look(const sol::state_view& lua)
        {
            OFScript::FalloutScripts& host = scripts();
            const FalloutScript::FaceLook face = host.playerLook();
            const OFScript::FalloutScripts::LookCatalog catalog = host.lookCatalog();

            sol::table result(lua, sol::create);
            result["female"] = face.mFemale;
            result["race"] = face.mRace == 0 ? std::string() : idText(face.mRace);
            result["hair"] = face.mHair == 0 ? std::string() : idText(face.mHair);
            result["hairLength"] = face.mHairLength;
            result["eyes"] = face.mEyes == 0 ? std::string() : idText(face.mEyes);
            sol::table colour(lua, sol::create);
            colour["red"] = static_cast<int>(face.mHairColour[0]);
            colour["green"] = static_cast<int>(face.mHairColour[1]);
            colour["blue"] = static_cast<int>(face.mHairColour[2]);
            result["hairColour"] = colour;
            sol::table parts(lua, sol::create);
            int index = 1;
            for (const FormId part : face.mHeadParts)
                parts[index++] = idText(part);
            result["headParts"] = parts;
            sol::table lists(lua, sol::create);
            lists["races"] = choiceRows(lua, catalog.mRaces);
            lists["hairs"] = choiceRows(lua, catalog.mHairs);
            lists["eyes"] = choiceRows(lua, catalog.mEyes);
            lists["facialHair"] = choiceRows(lua, catalog.mFacialHair);
            lists["presets"] = choiceRows(lua, catalog.mPresets);
            result["lists"] = lists;
            return result;
        }

        /// The sliders of the face lists with the whole number each shows now
        sol::table faceSliderRows(const sol::state_view& lua)
        {
            OFScript::FalloutScripts& host = scripts();
            sol::table result(lua, sol::create);
            int index = 1;
            for (const FalloutScript::FaceSlider& slider : FalloutScript::faceSliders())
            {
                sol::table row(lua, sol::create);
                row["kind"] = slider.mKind == FaceKind::Shape ? "shape" : "tone";
                row["control"] = slider.mControl;
                row["list"] = slider.mList;
                row["setting"] = slider.mSetting;
                row["minimum"] = slider.mMinimum;
                row["maximum"] = slider.mMaximum;
                row["value"] = host.lookSlider(slider.mKind, slider.mControl);
                result[index++] = row;
            }
            return result;
        }

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
                row["icon"] = text.mIcon;
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
                row["icon"] = text.mIcon;
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
                row["icon"] = trait.mIcon;
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
            // The number of the menu the screen stands for (1060 the SPECIAL book, 1074 the Vigor Tester, 1048 the screen
            // of points and skills) and the points the attributes have to come to
            result["menu"] = chargen.menu();
            result["limit"] = chargen.pointTotal();
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
        // The sex and race clicked also take the head parts off, put the first face of the kind on and change a hair or
        // eyes that the kind does not have (the screen of the face; the older screens keep only the choice)
        api["setFemale"] = [](bool female) { scripts().lookSetSex(female); };
        api["setRace"] = [](std::string_view race) { scripts().lookSetRace(parseId(race)); };
        // Gives (or with a negative amount takes) points of an attribute; false when they can not be given
        api["adjust"] = [](int attribute, int delta) { return scripts().chargen().adjustAttribute(attribute, delta); };
        api["toggleTag"] = [](int skill) { return scripts().chargen().toggleTag(skill); };
        api["toggleTrait"] = [](std::string_view trait) { return scripts().chargen().toggleTrait(parseId(trait)); };
        // The look of the player (the screen of the sex, race and face): what it has now, with the lists to pick from
        api["look"] = [lua]() { return look(lua); };
        api["setHair"] = [](std::string_view hair) { scripts().lookSetHair(parseId(hair)); };
        api["setEyes"] = [](std::string_view eyes) { scripts().lookSetEyes(parseId(eyes)); };
        api["setHairColour"] = [](int red, int green, int blue) { scripts().lookSetHairColour(red, green, blue); };
        api["toggleFacialHair"] = [](std::string_view part) { scripts().lookToggleFacialHair(parseId(part)); };
        api["setPreset"] = [](int number) { return scripts().lookSetPreset(number); };
        api["randomize"] = []() { scripts().lookRandomize(); };
        // The sliders of the lists of the face, each with the whole number it shows
        api["sliders"] = [lua]() { return faceSliderRows(lua); };
        api["setSlider"]
            = [](std::string_view kind, int control, int value) { scripts().lookSetSlider(parseKind(kind), control, value); };
        // The age slider (1 to 10): moves the whole face along the age directions of the face file; false when the file
        // has none
        api["hasAge"] = []() { return scripts().lookHasAge(); };
        api["setAge"] = [](int value) { return scripts().lookSetAge(value); };
        // The screen that is open says (every frame) that Escape opens the pause menu over it, as it does in the game;
        // without the claim Escape leaves the mode of the screen
        api["claimEscape"] = [](bool claimed) { scripts().setLuaEscapeClaim(claimed); };
        // The player is done with the screen: what it picked becomes the character and the next screen opens
        api["finish"] = []() { scripts().finishChargenPage(); };
        return LuaUtil::makeReadOnly(api);
    }
}

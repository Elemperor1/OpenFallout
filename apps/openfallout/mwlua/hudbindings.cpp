#include "hudbindings.hpp"

#include <stdexcept>

#include <components/esm/refid.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwscript/falloutscripts.hpp"

#include "context.hpp"
#include "object.hpp"

namespace OFLua
{
    namespace
    {
        OFScript::FalloutScripts& scripts()
        {
            OFScript::FalloutScripts* host = OFBase::Environment::get().getFalloutScripts();
            if (host == nullptr)
                throw std::runtime_error("core.hud is only available with the content of Fallout 3 and New Vegas");
            return *host;
        }

        const char* sneakName(FalloutScript::Hud::Sneak sneak)
        {
            switch (sneak)
            {
                case FalloutScript::Hud::Sneak::Hidden:
                    return "hidden";
                case FalloutScript::Hud::Sneak::Caution:
                    return "caution";
                case FalloutScript::Hud::Sneak::Detected:
                    return "detected";
                case FalloutScript::Hud::Sneak::Danger:
                    return "danger";
            }
            return "hidden";
        }

        const char* markerKind(OFScript::FalloutScripts::HudMarker::Kind kind)
        {
            switch (kind)
            {
                case OFScript::FalloutScripts::HudMarker::Quest:
                    return "quest";
                case OFScript::FalloutScripts::HudMarker::Landmark:
                    return "landmark";
                case OFScript::FalloutScripts::HudMarker::Enemy:
                    return "enemy";
            }
            return "landmark";
        }
    }

    sol::table initCoreHudBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        // The messages that were printed since this was last called, oldest first: the text, the path of the picture
        // that goes with it ("" for none) and the seconds it stays
        api["takeMessages"] = [lua]() {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const FalloutScript::Hud::Message& message : scripts().hudMessages().take())
            {
                sol::table row(lua, sol::create);
                row["text"] = message.mText;
                row["icon"] = message.mIcon;
                row["seconds"] = message.mSeconds;
                rows[index++] = row;
            }
            return rows;
        };

        // The radiation (rads and the most the meter shows), whether the player sneaks and what the characters around
        // make of it ("hidden", "caution", "detected" or "danger", with the word the games print for it), the
        // textures (names under Interface\Stats) of the limbs that are crippled, and whether the weapon is drawn
        api["status"] = [lua]() {
            const OFScript::FalloutScripts::HudStatus status = scripts().hudStatus();
            sol::table row(lua, sol::create);
            row["rads"] = status.mRads;
            row["radsMax"] = status.mRadsMax;
            row["sneaking"] = status.mSneaking;
            row["sneak"] = sneakName(status.mSneak);
            row["sneakText"] = status.mSneakText;
            row["weaponDrawn"] = status.mWeaponDrawn;
            sol::table limbs(lua, sol::create);
            int index = 1;
            for (const std::string& limb : status.mCrippled)
                limbs[index++] = limb;
            row["crippled"] = limbs;
            return row;
        };

        // The character or creature that an object is: its name, health, most health, whether it is dead and whether
        // it fights the player; nothing for any other object
        api["target"] = [lua](sol::optional<LObject> object) -> sol::object {
            if (!object)
                return sol::nil;
            OFScript::FalloutScripts::HudTarget target;
            if (!scripts().hudTarget(object->ptr(), target))
                return sol::nil;
            sol::table row(lua, sol::create);
            row["name"] = target.mName;
            row["health"] = target.mHealth;
            row["maxHealth"] = target.mMaxHealth;
            row["dead"] = target.mDead;
            row["hostile"] = target.mHostile;
            return row;
        };

        // The way the player faces (degrees clockwise from north) and the things in the window of the compass: the kind
        // ("quest", "landmark" or "enemy"), where in the window it is (0 at the left edge, 1 at the right), how far
        // away it is along the floor and its name
        api["compass"] = [lua]() {
            double heading = 0;
            const std::vector<OFScript::FalloutScripts::HudMarker> found = scripts().hudCompass(heading);
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::HudMarker& marker : found)
            {
                sol::table row(lua, sol::create);
                row["kind"] = markerKind(marker.mKind);
                row["share"] = marker.mShare;
                row["distance"] = marker.mDistance;
                row["name"] = marker.mName;
                rows[index++] = row;
            }
            sol::table result(lua, sol::create);
            result["heading"] = heading;
            result["markers"] = rows;
            return result;
        };

        // The text of a note of text (the form id of the note as `core.getFormId` writes one), with the names of keys
        // and the like in it filled in
        api["noteText"] = [](std::string_view note) -> sol::optional<std::string> {
            const ESM::RefId id = ESM::RefId::deserializeText(note);
            const ESM::FormId* form = id.getIf<ESM::FormId>();
            if (form == nullptr)
                throw std::runtime_error("\"" + std::string(note) + "\" is not a form id");
            const std::optional<std::string> text = scripts().noteText(form->toUint32());
            if (!text)
                return sol::nullopt;
            return *text;
        };
        return LuaUtil::makeReadOnly(api);
    }
}

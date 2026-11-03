#include "hudbindings.hpp"

#include <iterator>
#include <stdexcept>

#include <components/esm/refid.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwgui/tooltips.hpp"
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

        // The lines that characters said to the player since this was last called, oldest first, for the subtitles: the
        // text, how long it stays, how far the speaker is, whether a script forced it and whose line it is (0 the
        // player's own, 2 another character's)
        api["takeSubtitles"] = [lua]() {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const FalloutScript::Hud::Subtitle& line : scripts().hudSubtitles().take())
            {
                sol::table row(lua, sol::create);
                row["text"] = line.mText;
                row["seconds"] = line.mSeconds;
                row["distance"] = line.mDistance;
                row["forced"] = line.mForced;
                row["kind"] = line.mKind;
                rows[index++] = row;
            }
            return rows;
        };

        // The texts of the quest tile since this was last called, oldest first: the heading (the word of the games'
        // settings: a quest that was added, completed or failed, a place that was found), the name under it and the
        // name of the sound
        api["takeQuestText"] = [lua]() {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const FalloutScript::Hud::QuestText& text : scripts().hudQuestText().takeTexts())
            {
                sol::table row(lua, sol::create);
                row["heading"] = text.mHeading;
                row["name"] = text.mName;
                row["sound"] = text.mSound;
                row["kind"] = text.mKind;
                rows[index++] = row;
            }
            return rows;
        };

        // The lines of objectives since this was last called, oldest first: the text (a completed one has the word of
        // the games' settings in front), whether it was completed and whether it is the reminder's
        api["takeObjectives"] = [lua]() {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const FalloutScript::Hud::ObjectiveLine& line : scripts().hudQuestText().takeObjectives())
            {
                sol::table row(lua, sol::create);
                row["text"] = line.mText;
                row["completed"] = line.mCompleted;
                row["reminder"] = line.mReminder;
                rows[index++] = row;
            }
            return rows;
        };

        // What the line over the compass is told since this was last called, oldest first: the names of the regions the
        // player came into, and the name of the cell the player came to after a door, a teleport or a load. It looks when
        // the player's cell is another one.
        api["takeRegions"] = [lua]() {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::RegionEvent& event : scripts().takeRegionEvents())
            {
                sol::table row(lua, sol::create);
                row["name"] = event.mName;
                row["cell"] = event.mCell;
                rows[index++] = row;
            }
            return rows;
        };

        // Whether the name box over the object that the player aims at (the old display's, drawn at the top of the
        // screen) is shown: the display from the game's menu file has its own text for the object, and turns it off
        api["setWorldTooltip"] = [](bool show) { OFGui::ToolTips::setWorldTooltipShown(show); };

        // The radiation (rads and the most the meter shows), whether the player sneaks and what the characters around
        // make of it ("hidden", "caution", "detected" or "danger", with the word the games print for it), the
        // textures (names under Interface\Stats) of the limbs that are crippled, whether the weapon is drawn and the
        // controls that are off
        api["status"] = [lua]() {
            const OFScript::FalloutScripts::HudStatus status = scripts().hudStatus();
            sol::table row(lua, sol::create);
            row["rads"] = status.mRads;
            row["radsMax"] = status.mRadsMax;
            row["sneaking"] = status.mSneaking;
            row["sneak"] = sneakName(status.mSneak);
            row["sneakText"] = status.mSneakText;
            row["weaponDrawn"] = status.mWeaponDrawn;
            // The controls a script took away (DisablePlayerControls): the display shows less while the player cannot
            // move (the executable's mode 0xC: only the radiation, the experience and the messages), and no text under
            // the crosshair while the rollover is off
            sol::table controls(lua, sol::create);
            static constexpr const char* controlNames[]
                = { "movement", "pipboy", "fighting", "pointOfView", "looking", "rollover", "sneaking" };
            static_assert(std::size(controlNames) == FalloutScript::playerControlCount,
                "every player control needs a name for Lua");
            for (std::size_t control = 0; control < FalloutScript::playerControlCount; ++control)
                controls[controlNames[control]] = (status.mControlsOff & (1u << control)) != 0;
            row["controlsOff"] = controls;
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

        // What the text under the crosshair says of an object (the one the crosshair is on): its name and how many
        // there are, the setting that holds the word of what the player does with it (`verb`, nothing for no word),
        // whether the weight and value row is there and what they are, whether its lock is broken, whether it is locked
        // (with whether the player has the key, the setting that holds the word of how hard the lock is, and whether
        // only the key opens it) and the name of the key or button that activates. Nothing for an object it says
        // nothing of.
        api["info"] = [lua](sol::optional<LObject> object) -> sol::object {
            if (!object)
                return sol::nil;
            OFScript::FalloutScripts::HudInfo info;
            if (!scripts().hudInfo(object->ptr(), info))
                return sol::nil;
            sol::table row(lua, sol::create);
            row["name"] = info.mName;
            row["count"] = info.mCount;
            if (!info.mVerb.empty())
                row["verb"] = info.mVerb;
            row["stats"] = info.mStats;
            row["weight"] = info.mWeight;
            row["value"] = info.mValue;
            row["broken"] = info.mBroken;
            row["locked"] = info.mLocked;
            row["hasKey"] = info.mHasKey;
            row["lockWord"] = info.mLockWord;
            row["keyOnly"] = info.mKeyOnly;
            row["keyText"] = info.mKeyText;
            row["pad"] = info.mPad;
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

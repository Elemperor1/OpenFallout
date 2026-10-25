#include "interactbindings.hpp"

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
        OFScript::FalloutScripts& scripts()
        {
            OFScript::FalloutScripts* host = OFBase::Environment::get().getFalloutScripts();
            if (host == nullptr)
                throw std::runtime_error("core.interact is only available with the content of Fallout 3 and New Vegas");
            return *host;
        }

        std::string idText(FalloutScript::FormId id)
        {
            return id == 0 ? std::string() : ESM::RefId::formIdRefId(ESM::FormId::fromUint32(id)).serializeText();
        }

        FalloutScript::FormId parseId(std::string_view text)
        {
            if (text.empty())
                return 0;
            const ESM::RefId id = ESM::RefId::deserializeText(text);
            const ESM::FormId* form = id.getIf<ESM::FormId>();
            if (form == nullptr)
                throw std::runtime_error("\"" + std::string(text) + "\" is not a form id");
            return form->toUint32();
        }
    }

    sol::table initCoreInteractBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        // The lock the player has in front of it (a locked door or container it activated): the form id of the object,
        // its name, the word for how hard the lock is, its level, whether only a key opens it, the bobby pins the
        // player has, the Lockpick skill, the chance that a pin opens the lock (0 to 1) and a number that is another
        // for each lock; nothing when no lock is open
        api["lock"] = [lua]() -> sol::object {
            const auto screen = scripts().lockScreen();
            if (!screen)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["reference"] = idText(screen->mReference);
            row["name"] = screen->mName;
            row["word"] = screen->mWord;
            row["level"] = screen->mLevel;
            row["keyOnly"] = screen->mKeyOnly;
            row["pins"] = screen->mPins;
            row["skill"] = screen->mSkill;
            row["chance"] = screen->mChance;
            row["serial"] = screen->mSerial;
            return row;
        };
        // One bobby pin is tried: "opened", "broke", "nopins" or "keyonly" ("" when no lock is open)
        api["pick"] = []() { return scripts().pickLock(); };
        api["closeLock"] = []() { scripts().closeLockScreen(); };

        // The number of the terminal that is open (0 for none); it changes with everything the screen shows
        api["terminalSerial"] = []() { return scripts().terminalSerial(); };
        // The terminal the player has open: its name, the text it shows, the lines of its menu (the number that goes
        // back to `choose` and the text), whether Back goes to a menu above it, and while it is closed the game of
        // hacking: the level, the words, the guesses made (the word and its likeness), the attempts left and the
        // seconds before it can be tried again
        api["terminal"] = [lua]() -> sol::object {
            const auto screen = scripts().terminalScreen();
            if (!screen)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["name"] = screen->mName;
            row["body"] = screen->mBody;
            row["canGoBack"] = screen->mCanGoBack;
            row["hacking"] = screen->mHacking;
            row["hackLevel"] = screen->mHackLevel;
            row["attempts"] = screen->mAttempts;
            row["lockout"] = screen->mLockout;
            row["serial"] = screen->mSerial;
            sol::table lines(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::TerminalLine& line : screen->mLines)
            {
                sol::table entry(lua, sol::create);
                entry["index"] = line.mIndex;
                entry["text"] = line.mText;
                lines[index++] = entry;
            }
            row["lines"] = lines;
            sol::table words(lua, sol::create);
            index = 1;
            for (const std::string& word : screen->mWords)
                words[index++] = word;
            row["words"] = words;
            sol::table guesses(lua, sol::create);
            index = 1;
            for (const OFScript::FalloutScripts::HackGuess& guess : screen->mGuesses)
            {
                sol::table entry(lua, sol::create);
                entry["word"] = guess.mWord;
                entry["likeness"] = guess.mLikeness;
                guesses[index++] = entry;
            }
            row["guesses"] = guesses;
            return row;
        };
        api["choose"] = [](int index) { scripts().terminalChoose(index); };
        api["back"] = []() { scripts().terminalBack(); };
        // A word of the game of hacking is guessed (its place in `words`, from 1): its likeness, -1 when no guess was
        // made
        api["guess"] = [](int word) { return scripts().hackGuess(word - 1); };
        api["closeTerminal"] = []() { scripts().closeTerminal(); };

        // The player wants to wait (the Wait key); a bed asks for sleep by itself
        api["requestWait"] = []() { scripts().requestRest(false, 0); };
        // The screen of waiting or sleeping that was asked for: whether it is for sleep and a number that is another
        // for each; nothing when none is asked for
        api["rest"] = [lua]() -> sol::object {
            const auto screen = scripts().restScreen();
            if (!screen)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["sleeping"] = screen->mSleeping;
            row["serial"] = screen->mSerial;
            return row;
        };
        // Starts to wait or sleep for that many hours (1 to 24): the text of why not, "" when it began
        api["startRest"] = [](double hours) { return scripts().startRest(hours); };
        // How far the rest is: whether it goes on, whether it is sleep, the hours that passed and the hours it is for,
        // why it ended early ("" when it did not) and a number that is another for each rest that ended
        api["restProgress"] = [lua]() {
            const OFScript::FalloutScripts::RestProgress progress = scripts().restProgress();
            sol::table row(lua, sol::create);
            row["active"] = progress.mActive;
            row["sleeping"] = progress.mSleeping;
            row["done"] = progress.mDone;
            row["total"] = progress.mTotal;
            row["stopped"] = progress.mStopped;
            row["ended"] = progress.mEnded;
            return row;
        };
        // Ends the rest where it is, or closes the screen of it
        api["cancelRest"] = []() { scripts().cancelRest(); };

        // The markers of the map the player has found: the form id, the name, where it is, whether the player can
        // travel to it, and how far away it is along the floor (-1 when the player is not in the same world outside)
        api["map"] = [lua]() {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::MapEntry& entry : scripts().mapEntries())
            {
                sol::table row(lua, sol::create);
                row["id"] = idText(entry.mId);
                row["name"] = entry.mName;
                row["x"] = entry.mPosition.x();
                row["y"] = entry.mPosition.y();
                row["travel"] = entry.mTravel;
                row["distance"] = entry.mDistance;
                rows[index++] = row;
            }
            return rows;
        };
        // The player travels to a marker: the text of why not, "" when it did
        api["travel"] = [](std::string_view marker) { return scripts().travelTo(parseId(marker)); };
        return LuaUtil::makeReadOnly(api);
    }
}

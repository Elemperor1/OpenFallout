#include "lockpickbindings.hpp"

#include <memory>
#include <random>

#include <components/falloutscript/lockpick.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "context.hpp"

namespace OFLua
{
    namespace
    {
        using FalloutScript::Lockpick::Event;
        using FalloutScript::Lockpick::Game;

        // The game and the numbers it draws; the Lua side keeps it in a table and gives it back
        struct LockpickGame
        {
            std::mt19937 mEngine{ std::random_device{}() };
            std::unique_ptr<Game> mGame;
        };

        const char* eventName(Event event)
        {
            switch (event)
            {
                case Event::Enter:
                    return "enter";
                case Event::OutOfPicks:
                    return "outOfPicks";
                case Event::Move:
                    return "move";
                case Event::Turn:
                    return "turn";
                case Event::SqueakA:
                    return "squeakA";
                case Event::SqueakB:
                    return "squeakB";
                case Event::Stop:
                    return "stop";
                case Event::TensionStart:
                    return "tensionStart";
                case Event::TensionEnd:
                    return "tensionEnd";
                case Event::Break:
                    return "break";
                case Event::Unlock:
                    return "unlock";
                case Event::ForceFailed:
                    return "forceFailed";
            }
            return "";
        }

        const char* stateName(Game::State state)
        {
            switch (state)
            {
                case Game::State::Intro:
                    return "intro";
                case Game::State::Idle:
                    return "idle";
                case Game::State::Turning:
                    return "turning";
                case Game::State::Binding:
                    return "binding";
                case Game::State::Snapping:
                    return "snapping";
                case Game::State::Waiting:
                    return "waiting";
                case Game::State::Opened:
                    return "opened";
                case Game::State::Failed:
                    return "failed";
            }
            return "";
        }

        sol::table report(sol::state_view lua, const Game& game, const std::vector<Event>& events)
        {
            sol::table result(lua, sol::create);
            sol::table list(lua, sol::create);
            int index = 1;
            for (const Event event : events)
                list[index++] = eventName(event);
            result["events"] = list;
            const Game::View view = game.view();
            result["pick"] = view.mPick;
            result["pickAngle"] = view.mPickAngle;
            result["angle"] = view.mAngle;
            result["lockSequence"] = view.mLockSequence;
            result["lockTime"] = view.mLockTime;
            result["pinSequence"] = view.mPinSequence;
            result["pinTime"] = view.mPinTime;
            result["health"] = view.mHealth;
            result["state"] = stateName(game.state());
            result["picks"] = game.picks();
            result["limit"] = game.limit();
            return result;
        }
    }

    sol::table initCoreLockpickBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        sol::usertype<LockpickGame> type = lua.new_usertype<LockpickGame>("LockpickGame", sol::no_constructor);
        // One step of the game: `seconds` since the last, `move` (how far the mouse went to the right, in the units of
        // the meter) and `turn` (0 to 1, how hard the lock is turned). The result is the table of `view`, and the
        // events of the step in `events`.
        type["step"] = [lua](LockpickGame& self, const sol::table& input) {
            Game::Input in;
            in.mSeconds = input.get_or("seconds", 0.0);
            in.mMove = input.get_or("move", 0.0);
            in.mTurn = input.get_or("turn", 0.0);
            const std::vector<Event> events = self.mGame->step(in);
            return report(lua, *self.mGame, events);
        };
        // The player forces the lock: the roll is drawn here
        type["force"] = [lua](LockpickGame& self) {
            const double roll = std::uniform_real_distribution<double>(0.0, 100.0)(self.mEngine);
            const std::vector<Event> events = self.mGame->force(std::min(roll, 99.999));
            return report(lua, *self.mGame, events);
        };
        type["view"] = [lua](LockpickGame& self) { return report(lua, *self.mGame, {}); };

        // A game for a lock of that class (0 very easy .. 4 very hard) with `skill`, `picks` and the width of the
        // meter in the units of the menu
        api["new"] = [](const sol::table& options) {
            auto self = std::make_shared<LockpickGame>();
            Game::Setup setup;
            setup.mLockClass = options.get_or("class", 2);
            setup.mSkill = options.get_or("skill", 0.0);
            setup.mPicks = options.get_or("picks", 0);
            setup.mMeterWidth = options.get_or("meter", 1000.0);
            LockpickGame* raw = self.get();
            raw->mGame = std::make_unique<Game>(
                setup, [raw] { return std::uniform_real_distribution<double>(0.0, 1.0)(raw->mEngine); });
            return self;
        };
        return LuaUtil::makeReadOnly(api);
    }
}

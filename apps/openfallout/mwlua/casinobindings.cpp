#include "casinobindings.hpp"

#include <stdexcept>

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
                throw std::runtime_error("core.casino is only available with the content of Fallout 3 and New Vegas");
            return *host;
        }

        const char* gameName(OFScript::FalloutScripts::CasinoGame game)
        {
            switch (game)
            {
                case OFScript::FalloutScripts::CasinoGame::Blackjack:
                    return "blackjack";
                case OFScript::FalloutScripts::CasinoGame::Roulette:
                    return "roulette";
                case OFScript::FalloutScripts::CasinoGame::Slots:
                    return "slots";
            }
            return "";
        }

        const char* kindName(FalloutScript::Casino::RouletteArea::Kind kind)
        {
            using Kind = FalloutScript::Casino::RouletteArea::Kind;
            switch (kind)
            {
                case Kind::Straight:
                    return "straight";
                case Kind::Split:
                    return "split";
                case Kind::Street:
                    return "street";
                case Kind::Corner:
                    return "corner";
                case Kind::SixLine:
                    return "sixline";
                case Kind::FiveNumber:
                    return "five";
                case Kind::Dozen:
                    return "dozen";
                case Kind::Column:
                    return "column";
                case Kind::Half:
                    return "half";
                case Kind::Even:
                    return "even";
                case Kind::Odd:
                    return "odd";
                case Kind::Red:
                    return "red";
                case Kind::Black:
                    return "black";
            }
            return "";
        }
    }

    sol::table initCoreCasinoBindings(const Context& context)
    {
        sol::state_view lua = context.mLua->unsafeState();
        sol::table api(lua, sol::create);

        api["available"] = []() { return OFBase::Environment::get().getFalloutScripts() != nullptr; };

        // The game that is open: which ("roulette"), the casino's name, the least and the most the table takes, what
        // the player has won in the casino, and a number that is another for each game that is opened
        api["screen"] = [lua]() -> sol::object {
            const auto screen = scripts().casinoScreen();
            if (!screen)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["game"] = gameName(screen->mGame);
            row["name"] = screen->mName;
            row["minBet"] = screen->mMinBet;
            row["maxBet"] = screen->mMaxBet;
            row["earnings"] = screen->mEarnings;
            row["serial"] = screen->mSerial;
            return row;
        };
        api["close"] = []() { scripts().closeCasino(); };

        sol::table roulette(lua, sol::create);
        // The bets of the table: for each (counted from 1, a place in the list is the area's number less one in the
        // calls below, which count from 0) its kind, the pockets it wins on (0 to 36, 37 is the double zero) and what
        // it pays on top of the stake
        roulette["areas"] = [lua]() {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const FalloutScript::Casino::RouletteArea& area : FalloutScript::Casino::rouletteAreas())
            {
                sol::table row(lua, sol::create);
                row["kind"] = kindName(area.mKind);
                row["multiplier"] = area.mMultiplier;
                sol::table numbers(lua, sol::create);
                int number = 1;
                for (const int pocket : area.mNumbers)
                    numbers[number++] = pocket;
                row["numbers"] = numbers;
                rows[index++] = row;
            }
            return rows;
        };
        // The pockets in the order they are on the wheel, and whether each is red or black
        roulette["wheel"] = [lua]() {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const int pocket : FalloutScript::Casino::rouletteWheel())
            {
                sol::table row(lua, sol::create);
                row["pocket"] = pocket;
                row["red"] = FalloutScript::Casino::pocketIsRed(pocket);
                row["black"] = FalloutScript::Casino::pocketIsBlack(pocket);
                rows[index++] = row;
            }
            return rows;
        };
        // The amount the player bets with, what it has put on the table and the chips it has; the stakes are the
        // places (from 1) of the areas that have one, with their stake
        roulette["state"] = [lua]() -> sol::object {
            const auto view = scripts().rouletteView();
            if (!view)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["amount"] = view->mAmount;
            row["total"] = view->mTotal;
            row["chips"] = view->mChips;
            row["bets"] = view->mBets;
            row["minBet"] = view->mMinBet;
            row["maxTotal"] = view->mMaxTotal;
            row["canSpin"] = view->mCanSpin;
            row["earnings"] = view->mEarnings;
            row["closes"] = view->mCloses;
            row["broke"] = view->mBroke;
            sol::table stakes(lua, sol::create);
            for (const auto& [area, stake] : view->mStakes)
                stakes[area + 1] = stake;
            row["stakes"] = stakes;
            return row;
        };
        roulette["increase"] = []() { return scripts().rouletteAmount(true); };
        roulette["decrease"] = []() { return scripts().rouletteAmount(false); };
        // The areas are counted from 1 here
        roulette["place"] = [](int area) { return scripts().roulettePlace(area - 1); };
        roulette["remove"] = [](int area) { return scripts().rouletteRemove(area - 1); };
        roulette["spin"] = [lua]() -> sol::object {
            const auto result = scripts().rouletteSpin();
            if (!result)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["pocket"] = result->mPocket;
            row["spinType"] = result->mSpinType;
            row["payout"] = result->mPayout;
            row["net"] = result->mNet;
            row["lucky"] = result->mLucky;
            row["unlucky"] = result->mUnlucky;
            return row;
        };
        api["roulette"] = LuaUtil::makeReadOnly(roulette);

        sol::table slots(lua, sol::create);
        // The machine: the bet, the chips, the stops the reels show (0 to 13: the even numbers are the seven symbols,
        // 12 the special one, the odd ones the blank after a symbol), whether it can be pulled, the model of its screen
        // (`scene`) and the pictures of the seven symbols (`symbols`), both files of the casino's record
        slots["state"] = [lua]() -> sol::object {
            const auto view = scripts().slotsView();
            if (!view)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["bet"] = view->mBet;
            row["chips"] = view->mChips;
            row["minBet"] = view->mMinBet;
            row["maxBet"] = view->mMaxBet;
            row["earnings"] = view->mEarnings;
            sol::table stops(lua, sol::create);
            for (std::size_t reel = 0; reel < view->mStops.size(); ++reel)
                stops[reel + 1] = view->mStops[reel];
            row["stops"] = stops;
            row["canSpin"] = view->mCanSpin;
            row["broke"] = view->mBroke;
            row["closes"] = view->mCloses;
            // The model of the machine's screen and the pictures of the seven symbols (files of the casino's record)
            row["scene"] = view->mScene;
            sol::table symbols(lua, sol::create);
            for (std::size_t symbol = 0; symbol < view->mSymbols.size(); ++symbol)
                symbols[symbol + 1] = view->mSymbols[symbol];
            row["symbols"] = symbols;
            return row;
        };
        using SlotsMove = OFScript::FalloutScripts::SlotsMove;
        slots["increase"] = []() { return scripts().slotsMove(SlotsMove::Increase); };
        slots["decrease"] = []() { return scripts().slotsMove(SlotsMove::Decrease); };
        slots["highest"] = []() { return scripts().slotsMove(SlotsMove::Highest); };
        // One pull: the stops, the class of the line (0 nothing, 1 a pair, 4 one special symbol, 5 two, 6 three alike),
        // what it pays on top of the stake, what the player has more or less afterwards, and whether Luck helped
        slots["spin"] = [lua]() -> sol::object {
            const auto result = scripts().slotsSpin();
            if (!result)
                return sol::nil;
            sol::table row(lua, sol::create);
            sol::table stops(lua, sol::create);
            for (std::size_t reel = 0; reel < result->mStops.size(); ++reel)
                stops[reel + 1] = result->mStops[reel];
            row["stops"] = stops;
            row["class"] = static_cast<int>(result->mClass);
            row["multiplier"] = result->mMultiplier;
            row["net"] = result->mNet;
            row["lucky"] = result->mLucky;
            return row;
        };
        api["slots"] = LuaUtil::makeReadOnly(slots);

        sol::table blackjack(lua, sol::create);
        // The table of the round: the cards (rank 0 to 8 are the twos to the tens, 9 to 11 the court cards, 12 the ace;
        // suit 0 to 3), the hands of the player with their bets and results (1 lose, 2 push, 3 win, 4 bust, 5
        // blackjack, 6 surrender), and what the player can do now
        blackjack["state"] = [lua]() -> sol::object {
            const auto view = scripts().blackjackView();
            if (!view)
                return sol::nil;
            const auto cardTable = [&lua](const std::vector<OFScript::FalloutScripts::BlackjackView::Card>& cards) {
                sol::table rows(lua, sol::create);
                int index = 1;
                for (const auto& card : cards)
                {
                    sol::table row(lua, sol::create);
                    row["rank"] = card.mRank;
                    row["suit"] = card.mSuit;
                    rows[index++] = row;
                }
                return rows;
            };
            sol::table row(lua, sol::create);
            using State = OFScript::FalloutScripts::BlackjackView::State;
            row["state"] = view->mState == State::Betting ? "betting"
                : view->mState == State::Playing          ? "playing"
                                                          : "finished";
            row["bet"] = view->mBet;
            row["chips"] = view->mChips;
            row["minBet"] = view->mMinBet;
            row["maxBet"] = view->mMaxBet;
            row["earnings"] = view->mEarnings;
            row["dealer"] = cardTable(view->mDealer);
            row["holeHidden"] = view->mHoleHidden;
            sol::table hands(lua, sol::create);
            int handIndex = 1;
            for (const auto& hand : view->mHands)
            {
                sol::table shown(lua, sol::create);
                shown["cards"] = cardTable(hand.mCards);
                shown["bet"] = hand.mBet;
                shown["result"] = hand.mResult;
                shown["doubled"] = hand.mDoubled;
                shown["done"] = hand.mDone;
                hands[handIndex++] = shown;
            }
            row["hands"] = hands;
            row["active"] = view->mActive + 1;
            row["canDeal"] = view->mCanDeal;
            row["canHit"] = view->mCanHit;
            row["canStay"] = view->mCanStay;
            row["canDouble"] = view->mCanDouble;
            row["canSplit"] = view->mCanSplit;
            row["canSurrender"] = view->mCanSurrender;
            row["canSwitch"] = view->mCanSwitch;
            row["net"] = view->mNet;
            row["dealerHelped"] = view->mDealerHelped;
            row["playerHelped"] = view->mPlayerHelped;
            row["broke"] = view->mBroke;
            row["closes"] = view->mCloses;
            return row;
        };
        using Move = OFScript::FalloutScripts::BlackjackMove;
        blackjack["increase"] = []() { return scripts().blackjackMove(Move::Increase); };
        blackjack["decrease"] = []() { return scripts().blackjackMove(Move::Decrease); };
        blackjack["highest"] = []() { return scripts().blackjackMove(Move::Highest); };
        blackjack["deal"] = []() { return scripts().blackjackMove(Move::Deal); };
        blackjack["hit"] = []() { return scripts().blackjackMove(Move::Hit); };
        blackjack["stay"] = []() { return scripts().blackjackMove(Move::Stay); };
        blackjack["double"] = []() { return scripts().blackjackMove(Move::Double); };
        blackjack["split"] = []() { return scripts().blackjackMove(Move::Split); };
        blackjack["surrender"] = []() { return scripts().blackjackMove(Move::Surrender); };
        blackjack["switch"] = []() { return scripts().blackjackMove(Move::Switch); };
        api["blackjack"] = LuaUtil::makeReadOnly(blackjack);
        return LuaUtil::makeReadOnly(api);
    }
}

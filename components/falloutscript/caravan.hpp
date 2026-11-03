#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CARAVAN_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CARAVAN_H

#include <array>
#include <utility>
#include <vector>

#include "casino.hpp"

namespace FalloutScript::Casino
{
    /// The card game Caravan of New Vegas (FalloutNV.exe, the menu 0x43B: the match 0x752E20, the placement 0x74F6C0,
    /// the opponent 0x74FDC0, the money 0x73D650 and 0x74CC70; read in the Mac round 12AS E3). Each side builds three
    /// caravans (tracks) of up to seven rows, a row being a number card (an ace to a ten) with up to three face cards
    /// attached; a caravan is sold when its rows add up to 21 to 26; the player's track i faces the opponent's track 5
    /// - i, and the side that has the better of two or three pairs wins.
    class Caravan
    {
    public:
        static constexpr int tracks = 6;
        static constexpr int rows = 7;
        static constexpr int handSize = 8;
        /// A face card's value (the records have no 11): the Jack removes a row, the Queen sets the direction and the
        /// suit of a caravan, the King doubles a row, the Joker removes the rows that are alike
        static constexpr int jack = 12;
        static constexpr int queen = 13;
        static constexpr int king = 14;
        static constexpr int joker = 15;
        /// The suit of a caravan that has no card
        static constexpr int noSuit = 5;

        struct Card
        {
            /// The form of the card's record (0 for a card that is made up, as a test's)
            FormId mForm = 0;
            /// 1 to 10 for a number card (the ace is 1), 12 to 15 for a face card
            int mValue = 0;
            /// 1 to 4, the face cards of the Joker's suit 5
            int mSuit = 0;
            bool isFace() const { return mValue > 10; }
            bool operator==(const Card& other) const
            {
                return mForm == other.mForm && mValue == other.mValue && mSuit == other.mSuit;
            }
        };

        enum class Side
        {
            Player = 0,
            Opponent = 1,
        };

        enum class Phase
        {
            /// Each side puts a number card on the first row of its three caravans
            Start,
            Play,
            Over,
        };

        /// One row: the number card, then the face cards that are attached to it (empty for a row that is not there)
        using Row = std::vector<Card>;

        /// What the caravans come to (0x74EE70): the direction the numbers go (+1 up, -1 down, 0 for a caravan with
        /// fewer than two rows), its suit (the last number card's, or the last Queen's) and its total
        struct Track
        {
            std::array<Row, rows> mRows;
            int mDirection = 0;
            int mSuit = noSuit;
            int mTotal = 0;
        };

        /// What a move of the opponent is (the numbers of the executable's move record, 0x746744)
        enum class MoveType
        {
            /// A number card or a King
            Place = 0,
            Queen = 1,
            /// A Jack on a caravan of its own that is over 26
            Jack = 2,
            Joker = 3,
            /// A number card of the start
            Start = 4,
            DiscardTrack = 5,
            DiscardCard = 6,
            /// A face card on a caravan of the player's
            FaceCard = 7,
        };

        struct Move
        {
            MoveType mType = MoveType::Place;
            int mTrack = 0;
            int mRow = 0;
            int mHand = 0;
            int mPriority = 0;
        };

        /// The whole of a match, for a test or for a match that is taken up in the middle
        struct Position
        {
            std::array<std::array<Row, rows>, tracks> mBoard;
            std::array<std::vector<Card>, 2> mHands;
            std::array<std::vector<Card>, 2> mPiles;
            Phase mPhase = Phase::Play;
            Side mTurn = Side::Player;
            int mAiType = 0;
        };

        /// A new match: each side deals itself eight cards from its pile, chosen at random without putting any back,
        /// and the opponent moves first. `aiType` is the number of ShowCaravanMenu (0 and 1 are what the game uses).
        Caravan(std::vector<Card> playerPile, std::vector<Card> opponentPile, int aiType, Random random);
        Caravan(const Position& position, Random random);

        Phase phase() const { return mPhase; }
        Side turn() const { return mTurn; }
        int aiType() const { return mAiType; }
        bool over() const { return mPhase == Phase::Over; }
        /// Whether the player won (meaningful when the match is over)
        bool playerWon() const { return mPlayerWon; }

        const Track& track(int index) const { return mTracks[index]; }
        const std::vector<Card>& hand(Side side) const { return mHands[static_cast<int>(side)]; }
        const std::vector<Card>& pile(Side side) const { return mPiles[static_cast<int>(side)]; }

        /// The number of rows a caravan has
        int rowCount(int track) const;
        /// A row's value: its number card times two for each King on it (0 for a row that is not there)
        int rowValue(int track, int row) const;
        /// Whether a caravan of this total is sold: 21 to 26
        static bool sold(int total) { return total >= 21 && total <= 26; }

        /// Whether the card of the hand can go on the row (0x74F6C0): in the start only a number card on the first row
        /// of the side's own caravan that has none; later a number card on the next row of the side's own caravan,
        /// going the way the caravan goes (a card of the caravan's suit may go the other way) and not the value of the
        /// row below; a face card on any caravan of either side on a row that has a number card and room for it (the
        /// Queen only on the last row of a caravan).
        bool canPlace(int track, int row, int handIndex, Side side) const;
        /// Puts the card there and ends the turn (an effect of a face card, then the hand is filled from the pile).
        /// False when the card cannot go there or it is not the side's turn.
        bool place(Side side, int track, int row, int handIndex);
        /// Takes the card out of the hand for good and fills the hand from the pile; in the play it ends the turn, in
        /// the start it does not. The player may only when its pile is not empty.
        bool discardCard(Side side, int handIndex);
        /// Empties one of the side's own caravans; it ends the turn. Not in the start.
        bool discardTrack(Side side, int track);
        /// The player gives the match up: it is a loss
        void forfeit();

        /// The move the opponent makes now (0x74FDC0): every move there is a use for is given a number and the highest
        /// wins, the first made among equals
        Move chooseOpponentMove() const;
        /// Chooses and makes the opponent's move; the move that was made. A match that is over or a turn that is not
        /// the opponent's makes none (the priority is then 0 and the type DiscardCard with no card).
        Move opponentMove();

    private:
        void recompute();
        void recomputeTrack(int track);
        void fill(Side side);
        void endTurn();
        /// 0x752E20: the end of the match, if the caravans or the cards say so
        bool decide();
        bool pileEmpty(Side side) const { return mPiles[static_cast<int>(side)].empty(); }
        int jokerLoss(int track, int row, int lossTrack) const;
        void removeRows(const std::array<std::array<bool, rows>, tracks>& marked);
        Move startMove() const;

        std::array<Track, tracks> mTracks;
        std::array<std::vector<Card>, 2> mHands;
        std::array<std::vector<Card>, 2> mPiles;
        Phase mPhase = Phase::Start;
        Side mTurn = Side::Opponent;
        int mAiType = 0;
        bool mPlayerWon = false;
        Random mRandom;
    };

    /// What the two sides stake before a match (0x73D650 and the menu's actions 9, 0xB and 0xC): the opponent opens
    /// with the share of what it has that its opponent's record says, the player matches it, may raise, and the pot is
    /// the smaller of the two bets.
    class CaravanStake
    {
    public:
        /// `funds` are the player's and `opponentFunds` the opponent's (caps and caravan money together), `fraction` is
        /// the share of the opponent's the ante is, `barter` the player's Barter skill
        CaravanStake(int funds, int opponentFunds, float fraction, int barter);

        int funds() const { return mFunds; }
        int ante() const { return mAnte; }
        int bet() const { return mBet; }
        /// The most the opponent will follow a raise to
        int ceiling() const { return mCeiling; }
        int pot() const { return mPot; }

        /// The bet goes to what the opponent asks, or to all the player has when that is less
        bool canMatch() const;
        void match();
        /// The player may raise by up to what it has left
        int mostRaise() const { return mFunds - mBet > 0 ? mFunds - mBet : 0; }
        /// A raise: the ante follows it up to the ceiling. False for an amount that is not between 1 and mostRaise().
        bool raise(int amount);
        bool canAccept() const { return mBet > 0 || mAnte == 0; }
        /// The pot is the smaller of the two bets; nothing moves yet. False when the stake cannot be accepted.
        bool accept();

    private:
        void updateCeiling();

        int mFunds;
        int mOpponentFunds;
        float mFraction;
        int mBarter;
        int mAnte;
        int mBet = 0;
        int mCeiling = 0;
        int mPot = 0;
    };

    /// What a side that lost pays when the match is over (0x74CC70): its caravan money, item by item in the order of
    /// its pack and each stack only as far as it goes whole into what is still owed, then the rest in caps.
    struct CaravanPayment
    {
        struct Item
        {
            FormId mForm = 0;
            int mValue = 0;
            int mCount = 0;
        };
        /// The money items and how many of each move
        std::vector<std::pair<FormId, int>> mItems;
        /// What the payer loses in caps (never more than it has)
        int mPayerCaps = 0;
        /// What the receiver is given in caps (all of the rest, even what the payer lacks)
        int mReceiverCaps = 0;
    };
    CaravanPayment settleCaravan(int pot, const std::vector<CaravanPayment::Item>& money, int caps);
}

#endif

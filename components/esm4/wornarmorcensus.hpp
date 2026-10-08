#ifndef OPENFALLOUT_COMPONENTS_ESM4_WORNARMORCENSUS_H
#define OPENFALLOUT_COMPONENTS_ESM4_WORNARMORCENSUS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <components/esm/formid.hpp>

#include "loadarmo.hpp"
#include "loadglob.hpp"
#include "loadlvli.hpp"
#include "loadlvln.hpp"
#include "loadnpc.hpp"
#include "wornarmor.hpp"

namespace ESM4
{
    class Reader;

    // Counts what the characters of one or more Fallout 3 and New Vegas plugins wear, with the same code as the game
    // (wornarmor.hpp): the armour that the inventory of each character and its levelled lists give, and which of the
    // pieces show on the body. It is for checking that code against the real files, so it keeps counts only.
    //
    // The files must be read in load order, each with its mod index and the indices of its masters set on the reader
    // (Reader::setModIndex and Reader::updateModIndices). A record that a later file defines again counts once, as
    // the later file has it, and one that a later file deletes, or that cannot be read, does not count.
    class WornArmorCensus final : public WornArmorSource
    {
    public:
        // The most pieces counted apart; a character with more counts as having this many.
        static constexpr std::size_t maxPieces = 5;

        // The level of the player that the game assumes, and other levels that the census tries as well
        static constexpr int assumedPlayerLevel = 5;
        static constexpr std::array<int, 4> triedLevels{ 1, 5, 15, 30 };

        struct Summary
        {
            WornArmorTrace mTrace;
            std::size_t mCharacters = 0;
            std::size_t mWithoutTraits = 0; // no record of the chain has the traits, so the character is not drawn
            std::size_t mWomen = 0;
            std::array<std::size_t, maxPieces + 1> mPieces{}; // characters by the number of pieces that show
            std::size_t mWithArmorButNoneShows = 0; // the inventory gave armour and none of it shows
            std::size_t mCoverUpperBody = 0;
            std::size_t mCoverHands = 0; // a hand at least
            std::size_t mCoverHead = 0;
            std::size_t mCoverHair = 0;
            std::size_t mPiecesWithoutModel = 0;
            std::size_t mPiecesWithoutSlots = 0;
            std::size_t mPiecesOverlapping = 0;
            std::size_t mWomenWearingMaleModels = 0; // pieces with no female model, worn by a woman

            // Characters with at least one piece showing, by the level of the player that is assumed
            std::array<std::size_t, triedLevels.size()> mDressedByLevel{};
        };

        void collect(Reader& reader);

        Summary summarize() const;

        const std::vector<std::string>& getFatalErrors() const { return mFatalErrors; }

        void write(std::ostream& stream) const;

        const Npc* findNpc(ESM::FormId id) const override;
        const LevelledNpc* findLevelledNpc(ESM::FormId id) const override;
        const Armor* findArmor(ESM::FormId id) const override;
        const LevelledItem* findLevelledItem(ESM::FormId id) const override;
        const GlobalVariable* findGlobal(ESM::FormId id) const override;

    private:
        std::unordered_map<ESM::FormId, Npc> mNpcs;
        std::unordered_map<ESM::FormId, LevelledNpc> mLevelledNpcs;
        std::unordered_map<ESM::FormId, Armor> mArmor;
        std::unordered_map<ESM::FormId, LevelledItem> mLevelledItems;
        std::unordered_map<ESM::FormId, GlobalVariable> mGlobals;
        std::vector<std::string> mFatalErrors;
    };
}

#endif

#ifndef OPENFALLOUT_COMPONENTS_ESM4_EQUIPMENTCENSUS_H
#define OPENFALLOUT_COMPONENTS_ESM4_EQUIPMENTCENSUS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    // Counts what the characters of one or more Fallout 3 and New Vegas plugins can wear: how many pieces of armour
    // they list in their inventory, how many of those pieces cover the same part of the body, and what the pieces
    // cover and have a model for. It is for choosing which of the pieces a character wears, so what it keeps is record
    // types, parts of the body and counts, never record contents. Only the 8 byte BMDT of ARMO that these two games
    // write is read; a piece of armour of another game has no part of the body here.
    //
    // The files must be read in load order, each with its mod index and the indices of its masters set on the reader
    // (Reader::setModIndex and Reader::updateModIndices), so that a form ID means the same record in every file. A
    // record that a later file defines again counts once, as the later file has it, and one that a later file deletes
    // does not count. A record that cannot be read does not count either.
    class EquipmentCensus
    {
    public:
        // The parts of the body that BMDT names, one bit for each, and the name of the bit.
        static constexpr std::size_t bodyPartCount = 20;
        static const std::array<const char*, bodyPartCount>& bodyPartNames();

        // A character with more pieces than this counts as having this many.
        static constexpr std::size_t maxPieces = 8;

        // How many of the entries of the levelled lists name a record of a type.
        struct ListEntries
        {
            std::size_t mArmour = 0;
            std::size_t mLists = 0; // levelled lists
            std::size_t mOther = 0; // another type of record
            std::size_t mUnknown = 0; // no file read has a record for it
        };

        struct Summary
        {
            // The ARMO records
            std::size_t mArmour = 0;
            std::size_t mNoBodyPart = 0;
            std::size_t mNoMaleModel = 0;
            std::size_t mNoFemaleModel = 0;
            std::size_t mNonPlayable = 0;
            std::size_t mPowerArmour = 0;
            std::array<std::size_t, bodyPartCount> mCoveringBodyPart{}; // pieces that cover the part

            // The NPC_ records
            std::size_t mCharacters = 0;
            std::size_t mInventoryFromTemplate = 0; // they list items, but the game takes those of the template
            std::size_t mNoItems = 0;
            std::size_t mWithArmour = 0;
            std::size_t mWithLevelledList = 0;
            // The characters that list n pieces of armour (distinct records), n = maxPieces for that many or more.
            std::array<std::size_t, maxPieces + 1> mPieces{};
            std::size_t mWithOverlap = 0; // two pieces cover the same part of the body
            std::array<std::size_t, bodyPartCount> mOverlapPart{}; // characters whose pieces share the part
            // Pieces that characters list, each piece counted once for every character that lists it
            std::size_t mListedPieces = 0;
            std::size_t mListedNoBodyPart = 0;
            std::size_t mListedNoModel = 0; // no male model
            std::size_t mListedNonPlayable = 0;

            // The LVLI records
            std::size_t mLists = 0;
            ListEntries mEntries;
        };

        // Reads every remaining record of the file. An error that leaves the reader in an unknown state stops the file
        // and is kept in getFatalErrors().
        void collect(Reader& reader);

        Summary summarize() const;

        const std::vector<std::string>& getFatalErrors() const { return mFatalErrors; }

        void write(std::ostream& stream) const;

    private:
        struct Armour
        {
            std::uint32_t mBodyParts = 0;
            std::uint32_t mFlags = 0;
            bool mMaleModel = false;
            bool mFemaleModel = false;
        };

        struct Character
        {
            std::vector<ESM::FormId> mItems;
            bool mInventoryFromTemplate = false;
        };

        std::unordered_map<ESM::FormId, std::uint32_t> mTypes; // record type of every record but a character
        std::unordered_map<ESM::FormId, Armour> mArmour;
        std::unordered_map<ESM::FormId, std::vector<ESM::FormId>> mLists;
        std::unordered_map<ESM::FormId, Character> mCharacters;
        std::vector<std::string> mFatalErrors;
    };
}

#endif

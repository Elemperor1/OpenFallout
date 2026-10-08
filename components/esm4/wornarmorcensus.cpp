#include "wornarmorcensus.hpp"

#include <algorithm>
#include <exception>
#include <iomanip>
#include <ostream>

#include "censusreading.hpp"
#include "common.hpp"
#include "reader.hpp"
#include "readerutils.hpp"

namespace ESM4
{
    using namespace CensusReading;

    namespace
    {
        template <class Record>
        const Record* find(const std::unordered_map<ESM::FormId, Record>& records, ESM::FormId id)
        {
            const auto found = records.find(id);
            return found == records.end() ? nullptr : &found->second;
        }

        // Who the dice of a character come from: the same character is dressed the same each time the census runs
        std::uint32_t seedOf(ESM::FormId id)
        {
            return id.mIndex * 2654435761u + static_cast<std::uint32_t>(id.mContentFile);
        }
    }

    const Npc* WornArmorCensus::findNpc(ESM::FormId id) const
    {
        return find(mNpcs, id);
    }

    const LevelledNpc* WornArmorCensus::findLevelledNpc(ESM::FormId id) const
    {
        return find(mLevelledNpcs, id);
    }

    const Armor* WornArmorCensus::findArmor(ESM::FormId id) const
    {
        return find(mArmor, id);
    }

    const LevelledItem* WornArmorCensus::findLevelledItem(ESM::FormId id) const
    {
        return find(mLevelledItems, id);
    }

    void WornArmorCensus::collect(Reader& reader)
    {
        auto visitRecord = [&](Reader& r) {
            const std::uint32_t type = r.hdr().record.typeId;
            if (type != REC_NPC_ && type != REC_ARMO && type != REC_LVLI && type != REC_LVLN)
                return false;

            const ESM::FormId id = r.getFormIdFromHeader();
            const ReaderContext recordStart = r.getContext();
            mNpcs.erase(id);
            mLevelledNpcs.erase(id);
            mArmor.erase(id);
            mLevelledItems.erase(id);

            if ((r.hdr().record.flags & Rec_Deleted) == 0)
            {
                try
                {
                    r.getRecordData();
                    if (type == REC_NPC_)
                    {
                        Npc& record = mNpcs[id];
                        record.load(r);
                    }
                    else if (type == REC_LVLN)
                    {
                        LevelledNpc& record = mLevelledNpcs[id];
                        record.load(r);
                    }
                    else if (type == REC_ARMO)
                    {
                        Armor& record = mArmor[id];
                        record.load(r);
                    }
                    else
                    {
                        LevelledItem& record = mLevelledItems[id];
                        record.load(r);
                    }
                }
                catch (const std::exception&)
                {
                    // The record cannot be read, so it counts for nothing, not even as the one an earlier file had.
                    mNpcs.erase(id);
                    mLevelledNpcs.erase(id);
                    mArmor.erase(id);
                    mLevelledItems.erase(id);
                }
            }
            r.skipFailedRecord(recordStart);
            return true;
        };

        try
        {
            ReaderUtils::readAll(reader, visitRecord, [](Reader&) {});
        }
        catch (const std::exception& e)
        {
            // Raised by the reader while it walks record and group headers, so it holds offsets and sizes only.
            mFatalErrors.push_back(firstLine(e.what()));
        }
    }

    WornArmorCensus::Summary WornArmorCensus::summarize() const
    {
        Summary summary;
        for (const auto& [id, npc] : mNpcs)
        {
            // Only a character that the game would draw is counted: the traits are the ones of a record of the chain
            const std::vector<const Npc*> chain = templateChain(*this, npc, assumedPlayerLevel, seedOf(id));
            const Npc* traits = templateOwner(chain, Npc::Template_UseTraits);
            ++summary.mCharacters;
            if (traits == nullptr)
            {
                ++summary.mWithoutTraits;
                continue;
            }
            const bool isFemale = traits->mIsFONV && (traits->mBaseConfig.fo3.flags & Npc::FO3_Female) != 0;
            if (isFemale)
                ++summary.mWomen;

            const std::vector<const Armor*> armor
                = wornArmor(*this, npc, assumedPlayerLevel, seedOf(id), &summary.mTrace);
            const WornPieces worn = wornPieces(armor, isFemale);

            ++summary.mPieces[std::min(worn.mPieces.size(), maxPieces)];
            if (!armor.empty() && worn.mPieces.empty())
                ++summary.mWithArmorButNoneShows;
            if ((worn.mCovered & Armor::FO3_UpperBody) != 0)
                ++summary.mCoverUpperBody;
            if ((worn.mCovered & (Armor::FO3_LeftHand | Armor::FO3_RightHand)) != 0)
                ++summary.mCoverHands;
            if ((worn.mCovered & Armor::FO3_Head) != 0)
                ++summary.mCoverHead;
            if ((worn.mCovered & Armor::FO3_Hair) != 0)
                ++summary.mCoverHair;
            summary.mPiecesWithoutModel += worn.mWithoutModel;
            summary.mPiecesWithoutSlots += worn.mWithoutSlots;
            summary.mPiecesOverlapping += worn.mOverlapping;
            if (isFemale)
                for (const WornPiece& piece : worn.mPieces)
                    if (piece.mModel == &piece.mArmor->mModelMale)
                        ++summary.mWomenWearingMaleModels;

            for (std::size_t i = 0; i < triedLevels.size(); ++i)
            {
                const std::vector<const Armor*> atLevel = wornArmor(*this, npc, triedLevels[i], seedOf(id));
                if (!wornPieces(atLevel, isFemale).mPieces.empty())
                    ++summary.mDressedByLevel[i];
            }
        }
        return summary;
    }

    void WornArmorCensus::write(std::ostream& stream) const
    {
        const Summary summary = summarize();
        const WornArmorTrace& trace = summary.mTrace;
        constexpr int countWidth = 9;

        stream << "Characters (NPC_ records): " << summary.mCharacters << '\n';
        stream << "  that the game would not draw (no record of their template chain has the traits): "
               << summary.mWithoutTraits << '\n';
        stream << "  women: " << summary.mWomen << '\n';
        stream << "Player level assumed: " << assumedPlayerLevel << '\n';

        stream << "\nInventories of the characters that are drawn\n";
        stream << "  taken from a template: " << trace.mInventoryFromTemplate << '\n';
        stream << "  no record of the chain has an inventory: " << trace.mNoInventory << '\n';
        stream << "  entries: " << trace.mItems << '\n';
        stream << "    a piece of armour: " << trace.mArmorListed << '\n';
        stream << "    a levelled list: " << trace.mListsEntered << '\n';
        stream << "    another kind of record, or none: " << trace.mOtherItems << '\n';

        stream << "\nLevelled lists of items\n";
        stream << "  pieces of armour that they gave: " << trace.mArmorFromLists << '\n';
        stream << "  that gave nothing by chance: " << trace.mListsEmptyByChance << '\n';
        stream << "  that gave nothing for no entry at or below the level: " << trace.mListsEmptyByLevel << '\n';
        stream << "  that use all entries: " << trace.mListsUsingAll << '\n';
        stream << "  nested too deep: " << trace.mListsTooDeep << '\n';
        stream << "Levelled lists of characters that gave none: " << trace.mTemplateListsEmpty << '\n';

        stream << "\nCharacters by the pieces of armour that show\n";
        for (std::size_t number = 0; number <= maxPieces; ++number)
            stream << "  " << number << (number == maxPieces ? " or more" : "") << ": " << std::setw(countWidth)
                   << summary.mPieces[number] << '\n';
        stream << "  characters whose inventory gave armour and none shows: " << summary.mWithArmorButNoneShows << '\n';

        stream << "\nCharacters with a piece that covers\n";
        stream << "  the upper body: " << summary.mCoverUpperBody << '\n';
        stream << "  a hand: " << summary.mCoverHands << '\n';
        stream << "  the head: " << summary.mCoverHead << '\n';
        stream << "  the hair: " << summary.mCoverHair << '\n';

        stream << "\nPieces left out\n";
        stream << "  with no model: " << summary.mPiecesWithoutModel << '\n';
        stream << "  covering no part of the body: " << summary.mPiecesWithoutSlots << '\n';
        stream << "  covering what a piece before them covers: " << summary.mPiecesOverlapping << '\n';
        stream << "Pieces that a woman wears with the model of a man (no female model): "
               << summary.mWomenWearingMaleModels << '\n';

        stream << "\nCharacters with at least one piece showing, by the level of the player\n";
        for (std::size_t i = 0; i < triedLevels.size(); ++i)
            stream << "  level " << std::setw(2) << triedLevels[i] << ": " << std::setw(countWidth)
                   << summary.mDressedByLevel[i] << '\n';

        if (!mFatalErrors.empty())
        {
            stream << "\nReading stopped early\n";
            for (const std::string& error : mFatalErrors)
                stream << "  " << error << '\n';
        }
    }
}

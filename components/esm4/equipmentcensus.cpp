#include "equipmentcensus.hpp"

#include <algorithm>
#include <exception>
#include <iomanip>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string_view>

#include <components/esm/common.hpp>

#include "common.hpp"
#include "inventory.hpp"
#include "reader.hpp"
#include "readerutils.hpp"

namespace ESM4
{
    namespace
    {
        // BMDT: the parts of the body in the low 20 bits of the first word, flags of the piece in the first byte of the
        // second
        constexpr std::uint32_t bodyPartMask = 0x000FFFFF;
        constexpr std::uint32_t nonPlayableFlag = 0x40;
        constexpr std::uint32_t powerArmourFlag = 0x20;

        // ACBS: flags, fatigue, barter gold, level, calculated minimum and maximum, speed multiplier, karma, base
        // disposition and the flags of the template
        constexpr std::uint16_t acbsSize = 24;
        constexpr std::size_t acbsTemplateFlagsOffset = 22;
        constexpr std::uint16_t useInventoryFlag = 0x0100;

        std::string firstLine(std::string_view message)
        {
            return std::string(message.substr(0, message.find('\n')));
        }

        // Calls visit with the type of each sub-record of the current record and reads the rest of it when visit
        // returns false. A sub-record that runs past the end of the record, a record that ends inside a sub-record
        // header and a visit that reads past the end of the data of the sub-record are errors.
        template <class Visit>
        void forEachSubRecord(Reader& reader, Visit&& visit)
        {
            while (true)
            {
                const bool found = reader.getSubRecordHeader();
                if (!reader.subRecordFitsRecord())
                    throw std::runtime_error("A sub-record runs past the end of its record");
                if (!found)
                {
                    if (reader.unreadRecordBytes() != 0)
                        throw std::runtime_error("A record ends inside a sub-record header");
                    return;
                }
                if (!visit(reader.subRecordHeader().typeId, reader.subRecordHeader().dataSize))
                    reader.skipSubRecordData();
            }
        }

        template <class T>
        void readValue(Reader& reader, T& value)
        {
            if (!reader.getExact(value))
                throw std::runtime_error("The file ends inside a sub-record");
        }

        std::uint32_t bodyPartsOf(std::uint32_t flags)
        {
            return flags & bodyPartMask;
        }
    }

    const std::array<const char*, EquipmentCensus::bodyPartCount>& EquipmentCensus::bodyPartNames()
    {
        static const std::array<const char*, bodyPartCount> names = { "Head", "Hair", "UpperBody", "LeftHand",
            "RightHand", "Weapon", "PipBoy", "Backpack", "Necklace", "Headband", "Hat", "EyeGlasses", "NoseRing",
            "Earrings", "Mask", "Choker", "MouthObject", "BodyAddOn1", "BodyAddOn2", "BodyAddOn3" };
        return names;
    }

    void EquipmentCensus::collect(Reader& reader)
    {
        auto readArmour = [](Reader& r) {
            Armour armour;
            forEachSubRecord(r, [&](std::uint32_t type, std::uint16_t size) {
                if (type == ESM::fourCC("BMDT") && size == 8)
                {
                    std::uint32_t flags = 0;
                    std::uint32_t general = 0;
                    readValue(r, flags);
                    readValue(r, general);
                    armour.mBodyParts = bodyPartsOf(flags);
                    armour.mFlags = general & 0xff;
                    return true;
                }
                if (type == ESM::fourCC("MODL") || type == ESM::fourCC("MOD3"))
                {
                    std::string path;
                    if (!r.getZString(path))
                        throw std::runtime_error("The file ends inside a model");
                    (type == ESM::fourCC("MODL") ? armour.mMaleModel : armour.mFemaleModel) = !path.empty();
                    return true;
                }
                return false;
            });
            return armour;
        };

        auto readList = [](Reader& r) {
            std::vector<ESM::FormId> entries;
            forEachSubRecord(r, [&](std::uint32_t type, std::uint16_t size) {
                if (type != ESM::fourCC("LVLO") || size != sizeof(LVLO))
                    return false;
                LVLO entry;
                if (!r.getExact(entry))
                    throw std::runtime_error("The file ends inside a levelled list entry");
                r.adjustFormId(entry.item);
                entries.push_back(ESM::FormId::fromUint32(entry.item));
                return true;
            });
            return entries;
        };

        auto readCharacter = [](Reader& r) {
            Character character;
            forEachSubRecord(r, [&](std::uint32_t type, std::uint16_t size) {
                if (type == ESM::fourCC("CNTO") && size == sizeof(InventoryItem))
                {
                    InventoryItem item;
                    if (!r.getExact(item))
                        throw std::runtime_error("The file ends inside an inventory item");
                    r.adjustFormId(item.item);
                    character.mItems.push_back(ESM::FormId::fromUint32(item.item));
                    return true;
                }
                if (type == ESM::fourCC("ACBS") && size == acbsSize)
                {
                    std::array<char, acbsSize> data;
                    if (!r.get(data.data(), data.size()))
                        throw std::runtime_error("The file ends inside the base configuration");
                    std::uint16_t templateFlags = 0;
                    std::copy_n(data.data() + acbsTemplateFlagsOffset, sizeof(templateFlags),
                        reinterpret_cast<char*>(&templateFlags));
                    character.mInventoryFromTemplate = (templateFlags & useInventoryFlag) != 0;
                    return true;
                }
                return false;
            });
            return character;
        };

        auto visitRecord = [&](Reader& r) {
            const std::uint32_t type = r.hdr().record.typeId;
            const ESM::FormId id = r.getFormIdFromHeader();
            const std::uint32_t flags = r.hdr().record.flags;

            const bool wanted = type == REC_ARMO || type == REC_LVLI || type == REC_NPC_;
            if (!wanted)
            {
                // Which type a record is tells what a levelled list or an inventory names; a deleted record names
                // nothing.
                if ((flags & Rec_Deleted) != 0)
                    mTypes.erase(id);
                else
                    mTypes[id] = type;
                return false;
            }

            const ReaderContext recordStart = r.getContext();
            mArmour.erase(id);
            mLists.erase(id);
            mCharacters.erase(id);
            if (type == REC_NPC_)
                mTypes.erase(id);
            else if ((flags & Rec_Deleted) != 0)
                mTypes.erase(id);
            else
                mTypes[id] = type;

            if ((flags & Rec_Deleted) == 0)
            {
                try
                {
                    r.getRecordData();
                    if (type == REC_ARMO)
                        mArmour[id] = readArmour(r);
                    else if (type == REC_LVLI)
                        mLists[id] = readList(r);
                    else
                        mCharacters[id] = readCharacter(r);
                }
                catch (const std::exception&)
                {
                    // The record cannot be read, so it counts for nothing, not even as the one an earlier file had.
                    mArmour.erase(id);
                    mLists.erase(id);
                    mCharacters.erase(id);
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

    EquipmentCensus::Summary EquipmentCensus::summarize() const
    {
        Summary summary;

        for (const auto& [id, armour] : mArmour)
        {
            ++summary.mArmour;
            if (armour.mBodyParts == 0)
                ++summary.mNoBodyPart;
            if (!armour.mMaleModel)
                ++summary.mNoMaleModel;
            if (!armour.mFemaleModel)
                ++summary.mNoFemaleModel;
            if ((armour.mFlags & nonPlayableFlag) != 0)
                ++summary.mNonPlayable;
            if ((armour.mFlags & powerArmourFlag) != 0)
                ++summary.mPowerArmour;
            for (std::size_t part = 0; part < bodyPartCount; ++part)
                if ((armour.mBodyParts & (1u << part)) != 0)
                    ++summary.mCoveringBodyPart[part];
        }

        for (const auto& [id, list] : mLists)
        {
            ++summary.mLists;
            for (const ESM::FormId entry : list)
            {
                const auto type = mTypes.find(entry);
                if (mArmour.contains(entry))
                    ++summary.mEntries.mArmour;
                else if (type == mTypes.end())
                    ++summary.mEntries.mUnknown;
                else if (type->second == REC_LVLI)
                    ++summary.mEntries.mLists;
                else
                    ++summary.mEntries.mOther;
            }
        }

        for (const auto& [id, character] : mCharacters)
        {
            ++summary.mCharacters;
            if (character.mInventoryFromTemplate)
            {
                ++summary.mInventoryFromTemplate;
                continue;
            }
            if (character.mItems.empty())
            {
                ++summary.mNoItems;
                continue;
            }

            // The pieces of armour the character lists, each once, and the parts of the body that two of them share
            std::set<ESM::FormId> pieces;
            bool levelled = false;
            for (const ESM::FormId item : character.mItems)
            {
                if (mArmour.contains(item))
                    pieces.insert(item);
                else if (const auto type = mTypes.find(item); type != mTypes.end() && type->second == REC_LVLI)
                    levelled = true;
            }
            if (levelled)
                ++summary.mWithLevelledList;
            if (!pieces.empty())
                ++summary.mWithArmour;
            ++summary.mPieces[std::min(pieces.size(), maxPieces)];

            std::uint32_t covered = 0;
            std::uint32_t shared = 0;
            for (const ESM::FormId piece : pieces)
            {
                const Armour& armour = mArmour.at(piece);
                ++summary.mListedPieces;
                if (armour.mBodyParts == 0)
                    ++summary.mListedNoBodyPart;
                if (!armour.mMaleModel)
                    ++summary.mListedNoModel;
                if ((armour.mFlags & nonPlayableFlag) != 0)
                    ++summary.mListedNonPlayable;
                shared |= covered & armour.mBodyParts;
                covered |= armour.mBodyParts;
            }
            if (shared != 0)
            {
                ++summary.mWithOverlap;
                for (std::size_t part = 0; part < bodyPartCount; ++part)
                    if ((shared & (1u << part)) != 0)
                        ++summary.mOverlapPart[part];
            }
        }

        return summary;
    }

    void EquipmentCensus::write(std::ostream& stream) const
    {
        const Summary summary = summarize();
        const auto& names = bodyPartNames();
        constexpr int nameWidth = 14;
        constexpr int countWidth = 9;

        stream << "Armour records: " << summary.mArmour << '\n';
        stream << "  with no part of the body: " << summary.mNoBodyPart << '\n';
        stream << "  with no male model: " << summary.mNoMaleModel << '\n';
        stream << "  with no female model: " << summary.mNoFemaleModel << '\n';
        stream << "  not playable: " << summary.mNonPlayable << '\n';
        stream << "  power armour: " << summary.mPowerArmour << '\n';

        stream << "\nArmour records that cover a part of the body (a piece covers several)\n";
        for (std::size_t part = 0; part < bodyPartCount; ++part)
            stream << "  " << std::left << std::setw(nameWidth) << names[part] << std::right << std::setw(countWidth)
                   << summary.mCoveringBodyPart[part] << '\n';

        stream << "\nCharacters (NPC_ records): " << summary.mCharacters << '\n';
        stream << "  whose items are those of their template: " << summary.mInventoryFromTemplate << '\n';
        stream << "  with no items: " << summary.mNoItems << '\n';
        stream << "  with a piece of armour: " << summary.mWithArmour << '\n';
        stream << "  with a levelled list: " << summary.mWithLevelledList << '\n';
        stream << "  with two pieces that cover the same part of the body: " << summary.mWithOverlap << '\n';

        stream << "\nCharacters with items of their own, by the pieces of armour they list\n";
        for (std::size_t pieces = 0; pieces <= maxPieces; ++pieces)
            stream << "  " << pieces << (pieces == maxPieces ? " or more" : "") << ": " << summary.mPieces[pieces]
                   << '\n';

        stream << "\nCharacters with two pieces that cover a part of the body\n";
        for (std::size_t part = 0; part < bodyPartCount; ++part)
            if (summary.mOverlapPart[part] != 0)
                stream << "  " << std::left << std::setw(nameWidth) << names[part] << std::right
                       << std::setw(countWidth) << summary.mOverlapPart[part] << '\n';

        stream << "\nPieces of armour that characters list (once for every character)\n";
        stream << "  all: " << summary.mListedPieces << '\n';
        stream << "  with no part of the body: " << summary.mListedNoBodyPart << '\n';
        stream << "  with no male model: " << summary.mListedNoModel << '\n';
        stream << "  not playable: " << summary.mListedNonPlayable << '\n';

        stream << "\nLevelled lists (LVLI records): " << summary.mLists << '\n';
        stream << "  entries that name armour: " << summary.mEntries.mArmour << '\n';
        stream << "  entries that name another levelled list: " << summary.mEntries.mLists << '\n';
        stream << "  entries that name another type of record: " << summary.mEntries.mOther << '\n';
        stream << "  entries that name no record that was read: " << summary.mEntries.mUnknown << '\n';

        if (!mFatalErrors.empty())
        {
            stream << "\nReading stopped early\n";
            for (const std::string& error : mFatalErrors)
                stream << "  " << error << '\n';
        }
    }
}

#include "loadfact.hpp"

#include <stdexcept>

#include "reader.hpp"

namespace ESM4
{
    namespace
    {
        [[noreturn]] void fail(const char* message)
        {
            throw std::runtime_error(std::string("ESM4::FACT::load - ") + message);
        }

        // The reader cannot read a zero-terminated string from a sub-record that has no bytes, so that is an empty one.
        void readString(Reader& reader, std::string& value)
        {
            if (reader.subRecordHeader().dataSize == 0)
                value.clear();
            else if (!reader.getZString(value))
                fail("sub-record is shorter than its size");
        }
    }

    void Faction::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        while (reader.getSubRecordHeader())
        {
            const SubRecordHeader& subHdr = reader.subRecordHeader();
            if (!reader.subRecordFitsRecord())
                fail("sub-record is longer than its record");

            switch (subHdr.typeId)
            {
                case ESM::fourCC("EDID"):
                    readString(reader, mEditorId);
                    break;
                case ESM::fourCC("FULL"):
                    // Fallout 3 and New Vegas plugins are never localized, and getLocalizedString hides a short read.
                    readString(reader, mFullName);
                    break;
                case ESM::fourCC("XNAM"):
                {
                    // Oblivion stops after the modifier. Every XNAM in the Fallout 3 and New Vegas plugins is 12 bytes.
                    if (subHdr.dataSize != 8 && subHdr.dataSize != 12)
                        fail("XNAM has an unexpected size");
                    Relation& relation = mRelations.emplace_back();
                    if (!reader.getFormId(relation.mTarget) || !reader.getExact(relation.mModifier))
                        fail("XNAM is shorter than its size");
                    if (subHdr.dataSize == 12 && !reader.getExact(relation.mGroupCombatReaction))
                        fail("XNAM is shorter than its size");
                    break;
                }
                case ESM::fourCC("DATA"):
                {
                    // The plugins of both games have one byte or four: two bytes of flags and two unused ones. The
                    // flags come first, so a size in between is read the same way.
                    if (subHdr.dataSize < 1 || subHdr.dataSize > 4)
                        fail("DATA has an unexpected size");
                    std::uint8_t data[4] = {};
                    if (!reader.get(data, subHdr.dataSize))
                        fail("DATA is shorter than its size");
                    mFactionFlags = data[0];
                    mFactionFlags2 = data[1];
                    break;
                }
                case ESM::fourCC("CNAM"):
                    if (subHdr.dataSize != sizeof(mCrimeGoldMultiplier))
                        fail("CNAM has an unexpected size");
                    if (!reader.getExact(mCrimeGoldMultiplier))
                        fail("CNAM is shorter than its size");
                    break;
                case ESM::fourCC("RNAM"):
                    if (subHdr.dataSize != sizeof(std::int32_t))
                        fail("RNAM has an unexpected size");
                    if (!reader.getExact(mRanks.emplace_back().mIndex))
                        fail("RNAM is shorter than its size");
                    break;
                case ESM::fourCC("MNAM"):
                case ESM::fourCC("FNAM"):
                case ESM::fourCC("INAM"):
                {
                    if (mRanks.empty())
                        fail("a rank title comes before RNAM");
                    Rank& rank = mRanks.back();
                    if (subHdr.typeId == ESM::fourCC("MNAM"))
                        readString(reader, rank.mMaleTitle);
                    else if (subHdr.typeId == ESM::fourCC("FNAM"))
                        readString(reader, rank.mFemaleTitle);
                    else
                        readString(reader, rank.mInsignia);
                    break;
                }
                case ESM::fourCC("WMI1"):
                    if (subHdr.dataSize != sizeof(ESM::FormId32))
                        fail("WMI1 has an unexpected size");
                    if (!reader.getFormId(mReputation))
                        fail("WMI1 is shorter than its size");
                    break;
                default:
                    throw std::runtime_error("ESM4::FACT::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
            }
        }
    }
}

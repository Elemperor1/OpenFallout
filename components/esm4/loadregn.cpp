/*
  Copyright (C) 2015-2016, 2018, 2020-2021 cc9cii

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  cc9cii cc9c@iinet.net.au

  Much of the information on the data structures are based on the information
  from Tes4Mod:Mod_File_Format and Tes5Mod:File_Formats but also refined by
  trial & error.  See http://en.uesp.net/wiki for details.

*/
#include "loadregn.hpp"

#include <cstring>

#include "reader.hpp"
// #include "writer.hpp"

bool ESM4::Region::Area::contains(float x, float y) const
{
    const std::size_t count = mPoints.size() / 2;
    if (count < 3)
        return false;
    bool inside = false;
    for (std::size_t i = 0, j = count - 1; i < count; j = i++)
    {
        const float xi = mPoints[2 * i];
        const float yi = mPoints[2 * i + 1];
        const float xj = mPoints[2 * j];
        const float yj = mPoints[2 * j + 1];
        // An edge that spans the height of the point, and crosses it to the right of the point
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
            inside = !inside;
    }
    return inside;
}

bool ESM4::Region::covers(ESM::FormId worldspace, float x, float y) const
{
    if ((mFlags & Rec_Deleted) != 0)
        return false;
    if (!mWorldId.isZeroOrUnset() && mWorldId != worldspace)
        return false;
    for (const Area& area : mAreas)
        if (area.contains(x, y))
            return true;
    return false;
}

void ESM4::Region::load(ESM4::Reader& reader)
{
    mId = reader.getFormIdFromHeader();
    mFlags = reader.hdr().record.flags;
    // The points of an area follow its falloff; the points that come without one are an area of their own
    bool pointsExpected = false;

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case ESM::fourCC("EDID"):
                reader.getZString(mEditorId);
                break;
            case ESM::fourCC("RCLR"):
                reader.get(mColour);
                break;
            case ESM::fourCC("WNAM"):
                reader.getFormId(mWorldId);
                break;
            case ESM::fourCC("ICON"):
                reader.getZString(mShader);
                break;
            case ESM::fourCC("RPLI"):
                reader.get(mEdgeFalloff);
                mAreas.emplace_back().mEdgeFalloff = mEdgeFalloff;
                pointsExpected = true;
                break;
            case ESM::fourCC("RPLD"):
            {
                mRPLD.resize(subHdr.dataSize / sizeof(std::uint32_t));
                for (std::vector<std::uint32_t>::iterator it = mRPLD.begin(); it != mRPLD.end(); ++it)
                {
                    reader.get(*it);
#if 0
                    std::string padding;
                    padding.insert(0, reader.stackSize()*2, ' ');
                    std::cout << padding  << "RPLD: 0x" << std::hex << *it << std::endl;
#endif
                }
                // A payload that is not a whole number of floats ends in bytes that are no corner: skipped, so that the
                // next subrecord header is read from where it is
                if (const std::uint32_t rest = subHdr.dataSize % sizeof(std::uint32_t); rest != 0)
                    reader.skipSubRecordData(rest);
                if (!pointsExpected)
                    mAreas.emplace_back();
                pointsExpected = false;
                std::vector<float>& points = mAreas.back().mPoints;
                points.resize(mRPLD.size());
                if (!mRPLD.empty())
                    std::memcpy(points.data(), mRPLD.data(), mRPLD.size() * sizeof(std::uint32_t));
                break;
            }
            case ESM::fourCC("RDAT"):
                reader.get(mData);
                break;
            case ESM::fourCC("RDMP"):
            {
                // Only the areas matter to the engine: a name that is out of place is skipped, not an error
                if (mData.type != RDAT_Map)
                {
                    reader.skipSubRecordData();
                    break;
                }
                reader.getLocalizedString(mMapName);
                break;
            }
            // FO3 only 2: DemoMegatonSound and DC01 (both 0 RDMD)
            // FONV none
            case ESM::fourCC("RDMD"): // music type; 0 default, 1 public, 2 dungeon
            {
#if 0
                int dummy;
                reader.get(dummy);
                std::cout << "REGN " << mEditorId << " " << dummy << std::endl;
#else
                reader.skipSubRecordData();
#endif
                break;
            }
            case ESM::fourCC("RDMO"): // not seen in FO3/FONV?
            {
                // std::cout << "REGN " << ESM::printName(subHdr.typeId) << " skipping..."
                // << subHdr.dataSize << std::endl;
                reader.skipSubRecordData();
                break;
            }
            case ESM::fourCC("RDSD"): // Possibly the same as RDSA
            {
                if (mData.type != RDAT_Sound)
                {
                    reader.skipSubRecordData();
                    break;
                }

                std::size_t numSounds = subHdr.dataSize / sizeof(RegionSound);
                mSounds.resize(numSounds);
                for (std::size_t i = 0; i < numSounds; ++i)
                    reader.get(mSounds.at(i));

                break;
            }
            case ESM::fourCC("RDGS"): // Only in Oblivion? (ToddTestRegion1) // formId
            case ESM::fourCC("RDSA"):
            case ESM::fourCC("RDWT"): // formId
            case ESM::fourCC("RDOT"): // formId
            case ESM::fourCC("MICO"): // small icon, FO3 and FONV
            case ESM::fourCC("RDID"): // FONV
            case ESM::fourCC("RDSB"): // FONV
            case ESM::fourCC("RDSI"): // FONV
            case ESM::fourCC("NVMI"): // TES5
            case ESM::fourCC("ANAM"): // FO4
            case ESM::fourCC("RLDM"): // FO4
                // RDAT skipping... following is a map
                // RDMP skipping... map name
                //
                // RDAT skipping... following is weather
                // RDWT skipping... weather data
                //
                // RDAT skipping... following is sound
                // RDMD skipping... unknown, maybe music data
                //
                // RDSD skipping... unknown, maybe sound data
                //
                // RDAT skipping... following is grass
                // RDGS skipping... unknown, maybe grass

                reader.skipSubRecordData();
                break;
            default:
                // The engine reads a region for its areas and its worldspace; what it does not know is of no use to it,
                // and a plugin must not fail to load because of it
                reader.skipSubRecordData();
                break;
        }
    }
}

// void ESM4::Region::save(ESM4::Writer& writer) const
//{
// }

void ESM4::Region::blank() {}

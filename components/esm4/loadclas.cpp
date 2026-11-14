/*
  Copyright (C) 2016, 2018, 2021 cc9cii

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
#include "loadclas.hpp"

#include <algorithm>
#include <cstring>

#include <stdexcept>

#include "reader.hpp"
//#include "writer.hpp"

void ESM4::Class::load(ESM4::Reader& reader)
{
    mId = reader.getFormIdFromHeader();
    mFlags = reader.hdr().record.flags;

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case ESM::fourCC("EDID"):
                reader.getZString(mEditorId);
                break;
            case ESM::fourCC("FULL"):
                reader.getLocalizedString(mFullName);
                break;
            case ESM::fourCC("DESC"):
                reader.getLocalizedString(mDesc);
                break;
            case ESM::fourCC("ICON"):
                reader.getZString(mIcon);
                break;
            case ESM::fourCC("DATA"):
            {
                // The DATA of Fallout 3 and New Vegas is 28 bytes: four tag skills (4 bytes each, -1 for none), the flags (4
                // bytes: 1 playable, 2 guard), the services and the trainer. The executable keeps the flags at +0x54 of the
                // class, which is the 16th byte of DATA (Fallout3.exe 0x54D490 tests bit 1 of it for IsGuard). The files of
                // the other games are laid out another way and keep their data unread
                constexpr std::uint32_t flagsOffset = 16;
                std::uint8_t data[flagsOffset + sizeof(mFo3Flags)] = {};
                const std::uint32_t size = std::min<std::uint32_t>(subHdr.dataSize, sizeof(data));
                if (size == 0 || !reader.get(data, size))
                {
                    reader.skipSubRecordData();
                    break;
                }
                if (subHdr.dataSize > size)
                    reader.skipSubRecordData(subHdr.dataSize - size);
                if (size >= sizeof(data))
                    std::memcpy(&mFo3Flags, data + flagsOffset, sizeof(mFo3Flags));
                break;
            }
            case ESM::fourCC("ATTR"):
            case ESM::fourCC("PRPS"):
                reader.skipSubRecordData();
                break;
            default:
                throw std::runtime_error("ESM4::CLAS::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }
}

// void ESM4::Class::save(ESM4::Writer& writer) const
//{
// }

// void ESM4::Class::blank()
//{
// }

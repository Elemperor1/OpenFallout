/*
  Copyright (C) 2020-2021 cc9cii

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

  Also see https://tes5edit.github.io/fopdoc/ for FO3/FONV specific details.

*/
#include "loaddobj.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>

#include "reader.hpp"
//#include "writer.hpp"

void ESM4::DefaultObj::load(ESM4::Reader& reader)
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
                break; // "DefaultObjectManager"
            case ESM::fourCC("DATA"):
            {
                // The objects in the order the games keep them (the ones from the doctor's bag on are New Vegas only);
                // a record with fewer or more is read as far as it goes
                ESM::FormId* slots[] = { &mData.stimpack, &mData.superStimpack, &mData.radX, &mData.radAway,
                    &mData.morphine, &mData.perkParalysis, &mData.playerFaction, &mData.mysteriousStrangerNPC,
                    &mData.mysteriousStrangerFaction, &mData.defaultMusic, &mData.battleMusic, &mData.deathMusic,
                    &mData.successMusic, &mData.levelUpMusic, &mData.playerVoiceMale, &mData.playerVoiceMaleChild,
                    &mData.playerVoiceFemale, &mData.playerVoiceFemaleChild, &mData.eatPackageDefaultFood,
                    &mData.everyActorAbility, &mData.drugWearsOffImageSpace, &mData.doctorsBag, &mData.missFortuneNPC,
                    &mData.missFortuneFaction, &mData.meltdownExplosion, &mData.unarmedForwardPA,
                    &mData.unarmedBackwardPA, &mData.unarmedLeftPA, &mData.unarmedRightPA, &mData.unarmedCrouchPA,
                    &mData.unarmedCounterPA, &mData.spotterEffect, &mData.itemDetectedEfect,
                    &mData.cateyeMobileEffectNYI };
                const std::size_t count = std::min<std::size_t>(subHdr.dataSize / 4, std::size(slots));
                for (std::size_t i = 0; i < count; ++i)
                    reader.getFormId(*slots[i]);
                if (subHdr.dataSize > count * 4)
                    reader.skipSubRecordData(static_cast<std::uint32_t>(subHdr.dataSize - count * 4));
                break;
            }
            case ESM::fourCC("DNAM"):
                reader.skipSubRecordData();
                break;
            default:
                reader.skipSubRecordData();
                break;
        }
    }
}

// void ESM4::DefaultObj::save(ESM4::Writer& writer) const
//{
// }

// void ESM4::DefaultObj::blank()
//{
// }

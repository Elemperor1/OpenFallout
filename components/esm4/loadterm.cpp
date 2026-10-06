/*
  Copyright (C) 2019-2021 cc9cii

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
#include "loadterm.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

#include "reader.hpp"
#include "recordreader.hpp"
// #include "writer.hpp"

void ESM4::Terminal::load(ESM4::Reader& reader)
{
    mId = reader.getFormIdFromHeader();
    mFlags = reader.hdr().record.flags;

    // The terminals of Fallout 3 and New Vegas are read by size and by what they follow. Only the helpers are used,
    // because the loader is shared with the later games, whose menu items are different.
    RecordReader in(reader, "TERM");

    // The sub-records of a menu item come in this order: ITXT, RNAM, ANAM, INAM, TNAM, the script and CTDA. ITXT is
    // optional in the format reference, so a sub-record that is not after the one that the item last had starts the
    // next item.
    enum ItemPart
    {
        Part_Text,
        Part_Result,
        Part_Flags,
        Part_Note,
        Part_SubMenu,
        Part_Script,
        Part_Condition,
    };
    int lastPart = Part_Condition;
    const auto startItem = [&](ItemPart part) -> MenuItem& {
        if (mMenuItems.empty() || part <= lastPart)
            mMenuItems.emplace_back();
        lastPart = part;
        return mMenuItems.back();
    };
    // A string that can be localized. The reader cannot read a string that has no bytes at all, which is empty.
    const auto itemString = [&](std::string& value) {
        if (in.size() == 0)
            value.clear();
        else
            reader.getLocalizedString(value);
    };

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        // The script of a menu item follows its ANAM and INAM
        switch (subHdr.typeId)
        {
            case ESM::fourCC("SCHR"):
                // A script has one header, so another one is the script of the next item.
                if (subHdr.dataSize == sizeof(ScriptHeader))
                    startItem(Part_Script);
                break;
            case ESM::fourCC("SCDA"):
            case ESM::fourCC("SCTX"):
            case ESM::fourCC("SLSD"):
            case ESM::fourCC("SCVR"):
            case ESM::fourCC("SCRO"):
            case ESM::fourCC("SCRV"):
                if (!mMenuItems.empty())
                    lastPart = std::max<int>(lastPart, Part_Script);
                break;
            default:
                break;
        }
        if (!mMenuItems.empty() && mMenuItems.back().mScript.loadSubRecord(reader))
            continue;

        switch (subHdr.typeId)
        {
            case ESM::fourCC("EDID"):
                reader.getZString(mEditorId);
                break;
            case ESM::fourCC("FULL"):
                itemString(mFullName);
                break;
            case ESM::fourCC("DESC"):
                itemString(mText);
                break;
            case ESM::fourCC("SCRI"):
                reader.getFormId(mScriptId);
                break;
            case ESM::fourCC("PNAM"):
                reader.getFormId(mPasswordNote);
                break;
            case ESM::fourCC("SNAM"):
                if (subHdr.dataSize == 4)
                    reader.getFormId(mSound);
                // FIXME: FO4 sound marker params
                else
                    reader.skipSubRecordData();
                break;
            case ESM::fourCC("MODL"):
                reader.getZString(mModel);
                break;
            case ESM::fourCC("ITXT"):
                mMenuItems.emplace_back();
                lastPart = Part_Text;
                itemString(mMenuItems.back().mText);
                break;
            case ESM::fourCC("RNAM"):
            {
                MenuItem& item = startItem(Part_Result);
                in.string(mResultText);
                item.mResultText = mResultText;
                break;
            }
            case ESM::fourCC("ANAM"):
                if (subHdr.dataSize == sizeof(std::uint8_t))
                    reader.get(startItem(Part_Flags).mFlags);
                else
                    reader.skipSubRecordData();
                break;
            case ESM::fourCC("INAM"):
                if (subHdr.dataSize == sizeof(ESM::FormId32))
                    in.formId(startItem(Part_Note).mDisplayNote);
                else
                    reader.skipSubRecordData();
                break;
            case ESM::fourCC("TNAM"):
                if (subHdr.dataSize == sizeof(ESM::FormId32))
                    in.formId(startItem(Part_SubMenu).mSubMenu);
                else
                    reader.skipSubRecordData();
                break;
            case ESM::fourCC("CTDA"):
                // The conditions of the record come before its first menu item.
                if (subHdr.dataSize == sizeof(TargetCondition) || subHdr.dataSize == offsetof(TargetCondition, runOn))
                {
                    if (mMenuItems.empty())
                        in.conditionOrOlder(mConditions.emplace_back());
                    else
                    {
                        lastPart = Part_Condition;
                        in.conditionOrOlder(mMenuItems.back().mConditions.emplace_back());
                    }
                }
                else
                    reader.skipSubRecordData();
                break;
            case ESM::fourCC("DNAM"): // difficulty
            case ESM::fourCC("CIS1"):
            case ESM::fourCC("CIS2"):
            case ESM::fourCC("MODT"): // Model data
            case ESM::fourCC("MODC"):
            case ESM::fourCC("MODS"):
            case ESM::fourCC("MODF"): // Model data end
            case ESM::fourCC("MODB"):
            case ESM::fourCC("MODD"):
            case ESM::fourCC("DEST"): // Destruction data
            case ESM::fourCC("DSTD"):
            case ESM::fourCC("DSTF"):
            case ESM::fourCC("DMDL"):
            case ESM::fourCC("DMDT"):
            case ESM::fourCC("DMDS"):
            case ESM::fourCC("SCDA"):
            case ESM::fourCC("SCHR"):
            case ESM::fourCC("SCRO"):
            case ESM::fourCC("SCRV"):
            case ESM::fourCC("SCTX"):
            case ESM::fourCC("SCVR"):
            case ESM::fourCC("SLSD"):
            case ESM::fourCC("OBND"):
            case ESM::fourCC("VMAD"):
            case ESM::fourCC("KSIZ"):
            case ESM::fourCC("KWDA"):
            case ESM::fourCC("BSIZ"): // FO4
            case ESM::fourCC("BTXT"): // FO4
            case ESM::fourCC("COCT"): // FO4
            case ESM::fourCC("CNTO"): // FO4
            case ESM::fourCC("FNAM"): // FO4
            case ESM::fourCC("ISIZ"): // FO4
            case ESM::fourCC("ITID"): // FO4
            case ESM::fourCC("MNAM"): // FO4
            case ESM::fourCC("NAM0"): // FO4
            case ESM::fourCC("PRPS"): // FO4
            case ESM::fourCC("PTRN"): // FO4
            case ESM::fourCC("UNAM"): // FO4
            case ESM::fourCC("VNAM"): // FO4
            case ESM::fourCC("WBDT"): // FO4
            case ESM::fourCC("WNAM"): // FO4
            case ESM::fourCC("XMRK"): // FO4
                reader.skipSubRecordData();
                break;
            default:
                throw std::runtime_error("ESM4::TERM::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }
}

// void ESM4::Terminal::save(ESM4::Writer& writer) const
//{
// }

// void ESM4::Terminal::blank()
//{
// }

/*
  Copyright (C) 2019, 2020 cc9cii

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
#ifndef ESM4_TERM_H
#define ESM4_TERM_H

#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

#include "script.hpp"

namespace ESM4
{
    class Reader;
    class Writer;

    struct Terminal
    {
        /// An entry of the menu of a terminal in Fallout 3 and New Vegas.
        struct MenuItem
        {
            std::string mText; // ITXT
            std::string mResultText; // RNAM, what the terminal shows when the item is chosen
            std::uint8_t mFlags = 0; // ANAM
            ESM::FormId mDisplayNote; // INAM, a NOTE
            ESM::FormId mSubMenu; // TNAM, a TERM
            ScriptDefinition mScript; // SCHR, SCDA, SCTX, SLSD, SCVR, SCRO and SCRV, run when the item is chosen
            std::vector<TargetCondition> mConditions; // CTDA, when the item is shown
        };

        ESM::FormId mId; // from the header
        std::uint32_t mFlags; // from the header, see enum type RecordFlag for details

        std::string mEditorId;
        std::string mFullName;
        std::string mText;

        ESM::Path mModel;
        std::string mResultText; // RNAM, the last of the menu items

        std::vector<MenuItem> mMenuItems;
        std::vector<TargetCondition> mConditions; // CTDA before the first menu item

        ESM::FormId mScriptId;
        ESM::FormId mPasswordNote;
        ESM::FormId mSound;

        void load(ESM4::Reader& reader);
        // void save(ESM4::Writer& writer) const;

        // void blank();
        static constexpr ESM::RecNameInts sRecordId = ESM::RecNameInts::REC_TERM4;
    };
}

#endif // ESM4_TERM_H

/*
  Copyright (C) 2020 cc9cii

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
#ifndef ESM4_INFO_H
#define ESM4_INFO_H

#include <cstdint>
#include <string>
#include <vector>

#include "script.hpp" // TargetCondition
#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;
    class Writer;

    enum InfoFlag
    {
        INFO_Goodbye = 0x0001,
        INFO_Random = 0x0002,
        INFO_SayOnce = 0x0004,
        INFO_RunImmediately = 0x0008,
        INFO_InfoRefusal = 0x0010,
        INFO_RandomEnd = 0x0020,
        INFO_RunForRumors = 0x0040,
        INFO_SpeechChallenge = 0x0080,
        INFO_SayOnceADay = 0x0100,
        INFO_AlwaysDarken = 0x0200,
        INFO_LowIntelligence = 0x1000, // New Vegas: only for characters with a low intelligence
        INFO_HighIntelligence = 0x2000
    };

    /// One line of an INFO record: what is said, how, and with which voice file and animations. A response has the
    /// sub-records TRDT, NAM1, NAM2, NAM3, SNAM and LNAM, and TRDT starts the next one.
    struct DialogResponse
    {
        enum Flags : std::uint8_t
        {
            Flag_UseEmotionAnimation = 0x01
        };

        std::uint32_t mEmotionType = 0; // 0 neutral, 1 anger, 2 disgust, 3 fear, 4 sad, 5 happy, 6 surprise, 7 pained
        std::int32_t mEmotionValue = 0;
        std::uint8_t mNumber = 0; // the number in the name of the voice file
        ESM::FormId mSound; // a SOUN record that replaces the voice file; FO3/FONV
        std::uint8_t mFlags = 0; // FONV (24 bytes of TRDT)
        std::string mText; // NAM1
        std::string mNotes; // NAM2, the notes for the voice actor
        std::string mEdits; // NAM3
        ESM::FormId mSpeakerAnimation; // SNAM, an IDLE record
        ESM::FormId mListenerAnimation; // LNAM, an IDLE record
    };

    struct DialogInfo
    {
        enum NextSpeaker : std::uint8_t
        {
            Speaker_Target = 0,
            Speaker_Self = 1,
            Speaker_Either = 2
        };

        ESM::FormId mId; // from the header
        std::uint32_t mFlags; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // FIXME: no such record for INFO, but keep here to avoid extra work for now

        /// The topic (DIAL) this response belongs to: the label of the group the record is in, else TPIC. Null for a
        /// record that was read outside of a group.
        ESM::FormId mTopic;
        ESM::FormId mQuest;
        ESM::FormId mSound; // unused?
        /// The response before this one in the topic (PNAM), null for the first. The responses of a topic are tried
        /// in this order; the order they are stored in is that of the files.
        ESM::FormId mPreviousInfo;

        /// What is said, in order. TES4 and Skyrim records have one.
        std::vector<DialogResponse> mResponses;
        /// The first response, for the code that reads one
        TargetResponseData mResponseData;
        std::string mResponse;
        std::string mNotes;
        std::string mEdits;

        std::uint8_t mDialType = 0; // DialType
        std::uint8_t mNextSpeaker = 0; // NextSpeaker
        std::uint16_t mInfoFlags = 0; // see above enum

        /// The conditions (CTDA) that say when this response can be given, in order; they combine as in a script
        std::vector<TargetCondition> mConditions;
        ESM::FormId mParam3; // TES5 only

        /// Topics that the player can pick after this response (TCLT), and topics this response is linked from
        /// (TCLF); the topics a response adds to the player's list (NAME); responses it continues with (TCFU, New
        /// Vegas)
        std::vector<ESM::FormId> mChoices;
        std::vector<ESM::FormId> mLinkedFrom;
        std::vector<ESM::FormId> mAddTopics;
        std::vector<ESM::FormId> mFollowUps;

        std::string mPrompt; // RNAM: replaces the name of the topic in the list the player picks from
        ESM::FormId mSpeaker; // ANAM: the only character, or creature, that says it
        ESM::FormId mChallengeStat; // KNAM: the actor value or perk a speech challenge is on
        std::uint32_t mChallenge = 0; // DNAM: 0 none, 1 very easy ... 5 very hard

        ScriptDefinition mScript; // runs when the response starts (FO3/FONV)
        ScriptDefinition mEndScript; // the script after the NEXT sub-record, runs when the response ends (FO3/FONV)

        void load(ESM4::Reader& reader);
        // void save(ESM4::Writer& writer) const;

        // void blank();
        static constexpr ESM::RecNameInts sRecordId = ESM::RecNameInts::REC_INFO4;

    private:
        /// The responses of Fallout 3 and New Vegas, read strictly: an unknown sub-record, a size no game uses and
        /// bytes that no sub-record accounts for throw.
        void loadFallout(ESM4::Reader& reader);
    };
}

#endif // ESM4_INFO_H

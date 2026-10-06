#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADFACT_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADFACT_H

#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// A faction of Fallout 3 and Fallout: New Vegas: who its members are friendly or hostile to, what its ranks are
    /// called, and, in New Vegas, which reputation it keeps.
    struct Faction
    {
        // The first byte of DATA.
        enum Flags
        {
            Flag_HiddenFromPlayer = 0x01,
            Flag_Evil = 0x02,
            Flag_SpecialCombat = 0x04
        };

        // The second byte of DATA.
        enum Flags2
        {
            Flag_TrackCrime = 0x01,
            Flag_AllowSell = 0x02
        };

        // How the members of a faction react to a fight that involves a faction they know. Only New Vegas has it.
        enum GroupCombatReaction : std::uint32_t
        {
            Reaction_Neutral = 0,
            Reaction_Enemy = 1,
            Reaction_Ally = 2,
            Reaction_Friend = 3
        };

        // XNAM: how this faction feels about another one, or about a race.
        struct Relation
        {
            ESM::FormId mTarget; // a FACT or a RACE
            std::int32_t mModifier = 0;
            std::uint32_t mGroupCombatReaction = Reaction_Neutral; // not in Fallout 3
        };

        // RNAM starts a rank, MNAM, FNAM and INAM that follow belong to it.
        struct Rank
        {
            std::int32_t mIndex = 0;
            std::string mMaleTitle;
            std::string mFemaleTitle;
            std::string mInsignia; // a path to an icon, unused by the games
        };

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        std::vector<Relation> mRelations; // XNAM
        std::uint8_t mFactionFlags = 0; // DATA, see Flags
        std::uint8_t mFactionFlags2 = 0; // DATA, see Flags2
        float mCrimeGoldMultiplier = 0.f; // CNAM, unused in Fallout 3 and New Vegas
        std::vector<Rank> mRanks; // RNAM, MNAM, FNAM and INAM
        ESM::FormId mReputation; // WMI1, a REPU record, New Vegas only

        /// Throws on unknown subrecords, on sizes that no known version of the record has, and on a rank title that
        /// has no RNAM before it.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_FACT4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADFACT_H

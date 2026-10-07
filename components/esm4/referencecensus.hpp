#ifndef OPENFALLOUT_COMPONENTS_ESM4_REFERENCECENSUS_H
#define OPENFALLOUT_COMPONENTS_ESM4_REFERENCECENSUS_H

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    // Counts the references that the records of one or more TES4-format plugins place in the world, by the type of
    // record of the object they place. It is for finding which placed things the game world would silently leave out,
    // so what it keeps is record types and counts, never record contents.
    //
    // The files must be read in load order, each with its mod index and the indices of its masters set on the reader
    // (Reader::setModIndex and Reader::updateModIndices), so that a form ID means the same record in every file. A
    // reference that a later file places again (an override) counts once, as the later file has it, and one that a
    // later file deletes does not count.
    class ReferenceCensus
    {
    public:
        // The references of one record type (REFR, ACHR, ACRE, PGRE, PMIS or PBEA) that place one type of object.
        struct Count
        {
            std::size_t mTotal = 0;
            std::size_t mDisabled = 0; // initially disabled, whatever enables them later
        };

        // What a reference with no base object is counted under, and one whose base object no file read has a record
        // for (a master that was not given).
        static constexpr const char* noBase = "none";
        static constexpr const char* unknownBase = "unknown";

        // Reads every remaining record of the file. An error that leaves the reader in an unknown state stops the file
        // and is kept in getFatalErrors().
        void collect(Reader& reader);

        // Keyed by the four character code of the record type of the base object, then by the one of the reference.
        // The base is looked up when this is called, so a file read after the reference still counts.
        std::map<std::string, std::map<std::string, Count>> getCounts() const;

        // References placed by an earlier file that a later file deletes.
        std::size_t getDeleted() const { return mDeleted; }

        const std::vector<std::string>& getFatalErrors() const { return mFatalErrors; }

        void write(std::ostream& stream) const;

    private:
        struct Placed
        {
            std::uint32_t mType = 0; // record type of the reference
            ESM::FormId mBase;
            bool mDisabled = false;
        };

        std::unordered_map<ESM::FormId, std::uint32_t> mRecordTypes;
        std::unordered_map<ESM::FormId, Placed> mPlaced;
        std::size_t mDeleted = 0;
        std::vector<std::string> mFatalErrors;
    };
}

#endif

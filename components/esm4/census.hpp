#ifndef OPENMW_COMPONENTS_ESM4_CENSUS_H
#define OPENMW_COMPONENTS_ESM4_CENSUS_H

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <map>
#include <string>
#include <string_view>

namespace ESM4
{
    class Reader;

    enum class CensusOutcome
    {
        Parsed, // a loader exists and read the record without error
        NoParser, // no loader exists for this record type
        Failed, // a loader exists but rejected the record
    };

    struct CensusRecord
    {
        std::size_t mTotal = 0;
        std::size_t mParsed = 0;
        std::size_t mNoParser = 0;
        std::size_t mFailed = 0;
        // Failure message, with how many records produced it.
        std::map<std::string, std::size_t> mFailures;
    };

    // Counts the records of a TES4-format plugin by type and remembers what could not be read.
    // Only record types, counts and loader error messages are kept, never record contents.
    class Census
    {
    public:
        // Keyed by the four character record code.
        const std::map<std::string, CensusRecord>& getRecords() const { return mRecords; }

        // Empty unless reading stopped before the end of the file.
        const std::string& getFatalError() const { return mFatalError; }

        void add(const std::string& type, CensusOutcome outcome, std::string_view failure = {});

        // Reads every remaining record of the file. `parse` must return true when a loader handled the record,
        // false when there is none, and throw when the loader rejects the record. A rejected record does not stop
        // the census; an error that leaves the reader in an unknown state does.
        void collect(Reader& reader, const std::function<bool(Reader&)>& parse);

        void write(std::ostream& stream) const;

    private:
        std::map<std::string, CensusRecord> mRecords;
        std::string mFatalError;
    };
}

#endif

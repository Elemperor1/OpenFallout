#ifndef OPENFALLOUT_COMPONENTS_ESM4_CENSUS_H
#define OPENFALLOUT_COMPONENTS_ESM4_CENSUS_H

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <map>
#include <string>
#include <string_view>

#include "script.hpp"

namespace ESM4
{
    class Reader;
    struct AIPackage;
    struct DialogInfo;
    struct Quest;
    struct Script;
    struct Terminal;

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

    // What the census knows about the scripts that records of one type carry in line.
    struct CensusScripts
    {
        std::size_t mCount = 0; // scripts that hold anything; an empty script block is not counted
        std::size_t mBytecode = 0; // bytes of compiled script, all scripts together
        // Scripts whose SCHR header disagrees with what was read, see ScriptDefinition. A script can be in several.
        std::size_t mWrongSize = 0;
        std::size_t mWrongReferences = 0;
        std::size_t mWrongVariables = 0;
    };

    // Counts the records of a TES4-format plugin by type and remembers what could not be read.
    // Only record types, counts and loader failures are kept, never record contents. A loader's error message is
    // kept only when it names a loader and an unknown subrecord code; any other message is replaced.
    class Census
    {
    public:
        // Keyed by the four character record code.
        const std::map<std::string, CensusRecord>& getRecords() const { return mRecords; }

        // Empty unless reading stopped before the end of the file.
        const std::string& getFatalError() const { return mFatalError; }

        /// Return script statistics keyed by the owning record type's four-character code.
        /// The map is empty until addScripts() encounters a nonempty script.
        const std::map<std::string, CensusScripts>& getScripts() const { return mScripts; }

        void add(const std::string& type, CensusOutcome outcome, std::string_view failure = {});

        // Reads every remaining record of the file. `parse` must return true when a loader handled the record,
        // false when there is none, and throw when the loader rejects the record. A rejected record does not stop
        // the census; an error that leaves the reader in an unknown state does.
        void collect(Reader& reader, const std::function<bool(Reader&)>& parse);

        /// Count the loaded SCPT script, retaining only sizes and counts.
        void addScripts(const Script& record);
        /// Count the loaded INFO begin and end scripts, retaining only sizes and counts.
        void addScripts(const DialogInfo& record);
        /// Count each loaded QUST stage log entry's script, retaining only sizes and counts.
        void addScripts(const Quest& record);
        /// Count the loaded PACK begin, end and change scripts, retaining only sizes and counts.
        void addScripts(const AIPackage& record);
        /// Count the script of each loaded TERM menu item, retaining only sizes and counts.
        void addScripts(const Terminal& record);

        void write(std::ostream& stream) const;

    private:
        /// Accumulate statistics for one nonempty script under its owning record type.
        void addScript(const std::string& type, const ScriptDefinition& script);

        std::map<std::string, CensusRecord> mRecords;
        std::map<std::string, CensusScripts> mScripts;
        std::string mFatalError;
    };
}

#endif

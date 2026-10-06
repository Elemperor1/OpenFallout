#ifndef OPENFALLOUT_COMPONENTS_ESM4_SURVEY_H
#define OPENFALLOUT_COMPONENTS_ESM4_SURVEY_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ESM4
{
    class Reader;

    // What one four character sub-record code looks like inside one record type.
    struct SurveySubRecord
    {
        std::size_t mRecords = 0; // records of the type that hold it
        std::size_t mTotal = 0; // how many there are, all records together
        std::size_t mLeast = std::numeric_limits<std::size_t>::max(); // fewest in a record that holds it
        std::size_t mMost = 0; // most in one record
        std::map<std::uint32_t, std::size_t> mSizes; // data size, with how many have it
    };

    struct SurveyType
    {
        std::size_t mTotal = 0;
        std::size_t mParsed = 0;
        std::size_t mNoParser = 0;
        std::size_t mFailed = 0;
        std::size_t mSurveyed = 0; // records whose sub-records were listed, all of them unless only failures count
        std::size_t mOverrunning = 0; // surveyed records with a sub-record that runs past the end of the record
        std::map<std::string, SurveySubRecord> mSubRecords;
        // The sub-record codes of a record in file order, a run of the same code written as CODE*count, with how
        // many surveyed records have exactly that order.
        std::map<std::string, std::size_t> mOrders;
    };

    // Lists which sub-records the records of a TES4-format plugin hold: the codes, how often, which data sizes and in
    // which order. It is for writing the loader of a record type that has none and for finding out why a loader
    // rejects a record, without the game files leaving the machine. Only codes and numbers are kept, never record
    // contents or loader messages.
    class Survey
    {
    public:
        // `types` are four character record codes, empty means every type. With `failedOnly` only the records that
        // `parse` rejects are listed (they are all counted).
        explicit Survey(std::set<std::string> types = {}, bool failedOnly = false);

        // Reads every remaining record of the file and adds it to the survey, so one survey can take many files.
        // `parse` is the same as in Census::collect, and an empty one means no loader exists. An error that leaves
        // the reader in an unknown state stops the file and is kept in getFatalErrors().
        void collect(Reader& reader, const std::function<bool(Reader&)>& parse);

        const std::map<std::string, SurveyType>& getTypes() const { return mTypes; }
        const std::vector<std::string>& getFatalErrors() const { return mFatalErrors; }

        void write(std::ostream& stream) const;

    private:
        const std::set<std::string> mSelected;
        const bool mFailedOnly;
        std::map<std::string, SurveyType> mTypes;
        std::vector<std::string> mFatalErrors;
    };
}

#endif

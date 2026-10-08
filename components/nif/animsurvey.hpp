#ifndef OPENFALLOUT_COMPONENTS_NIF_ANIMSURVEY_HPP
#define OPENFALLOUT_COMPONENTS_NIF_ANIMSURVEY_HPP

#include <cstddef>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Nif
{
    struct NIFFile;

    /// What the animation data of the files it was given holds, as counts: for each question (a section, such as
    /// "Interpolator of a controlled block") the answers that were given, with how many times and the names of a few
    /// files that gave each. It holds numbers, names of files, nodes and animation events, never model data, so a run
    /// on game files that must not leave a machine can be reported. Not thread safe.
    class AnimSurvey
    {
    public:
        /// @param examples how many file names to keep for each answer
        /// @param maxAnswers how many answers to print for a section, the commonest first, 0 for all of them
        explicit AnimSurvey(std::size_t examples = 3, std::size_t maxAnswers = 0)
            : mExamples(examples)
            , mMaxAnswers(maxAnswers)
        {
        }

        void add(std::string_view section, std::string_view answer, std::string_view file);

        /// How many times the answer was given, 0 when it never was
        std::size_t count(std::string_view section, std::string_view answer) const;

        /// Count what a parsed file holds. @param path the path of the file in its archive, which says whether it is a
        /// skeleton (a file named skeleton*.nif) and which folder it belongs to
        void addFile(const NIFFile& file, std::string_view path);

        /// Compare the nodes that animation files drive with the nodes of the skeleton of their folder, which needs
        /// every file to have been added. Called by print.
        void compareNames();

        void print(std::ostream& out);

    private:
        struct Answer
        {
            std::size_t mCount = 0;
            std::vector<std::string> mExamples;
        };

        struct Driven
        {
            std::string mFile;
            std::set<std::string> mNodes;
        };

        // The sections in the order they were first used, the answers by their text
        std::vector<std::pair<std::string, std::map<std::string, Answer>>> mSections;
        std::size_t mExamples;
        std::size_t mMaxAnswers;

        // The nodes of each skeleton file by folder, and the nodes that each animation file drives, both lower case
        std::map<std::string, std::set<std::string>> mSkeletons;
        std::vector<Driven> mDriven;
        bool mCompared = false;
    };

    /// The folder of a path with a slash at the end, lower case, empty for a path with no folder
    std::string animSurveyFolder(std::string_view path);

    /// An answer that is a number, with leading zeros so that the answers of a section sort as numbers
    std::string animSurveyNumberAnswer(std::size_t number, std::size_t width = 3);

    /// The answer for a count of things, in the buckets none, one, 2 to 9, 10 to 99, 100 to 999 and 1000 or more
    std::string animSurveyCountAnswer(std::size_t count);

    /// The answer for a length of time in seconds, in buckets
    std::string animSurveyTimeAnswer(float seconds);
}

#endif

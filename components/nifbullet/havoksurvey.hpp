#ifndef OPENFALLOUT_COMPONENTS_NIFBULLET_HAVOKSURVEY_HPP
#define OPENFALLOUT_COMPONENTS_NIFBULLET_HAVOKSURVEY_HPP

#include <cstddef>
#include <iosfwd>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace NifBullet
{
    /// What the loader found in the Havok data of the meshes it was given, as counts: for each question (a section,
    /// such as "Bodies by layer") the answers that were given, with how many times and the names of a few files that
    /// gave each. It holds numbers and file names, never model data, so a run on game files that must not leave a
    /// machine can be reported. Not thread safe.
    class HavokSurvey
    {
    public:
        explicit HavokSurvey(std::size_t examples = 3)
            : mExamples(examples)
        {
        }

        void add(std::string_view section, std::string_view answer, std::string_view file);

        /// How many times the answer was given, 0 when it never was
        std::size_t count(std::string_view section, std::string_view answer) const;

        void print(std::ostream& out) const;

    private:
        struct Answer
        {
            std::size_t mCount = 0;
            std::vector<std::string> mExamples;
        };

        // The sections in the order they were first used, the answers by their text
        std::vector<std::pair<std::string, std::map<std::string, Answer>>> mSections;
        std::size_t mExamples;
    };

    /// An answer that is a number, with a leading zero so that the answers of a section sort as numbers
    std::string havokNumberAnswer(unsigned number);

    /// The answer for the size of Havok shape over the size of the geometry that is drawn (the longest extents), which
    /// is about 1 when the scale of the Havok units is the right one
    std::string havokSizeRatioAnswer(float ratio);

    /// The answer for the distance of the centres of the two, in sizes of the geometry that is drawn
    std::string havokOffsetAnswer(float offset);
}

#endif

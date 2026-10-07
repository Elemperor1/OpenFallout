#include "havoksurvey.hpp"

#include <algorithm>
#include <iomanip>
#include <ostream>

namespace NifBullet
{
    void HavokSurvey::add(std::string_view section, std::string_view answer, std::string_view file)
    {
        auto it = std::ranges::find_if(mSections, [&](const auto& entry) { return entry.first == section; });
        if (it == mSections.end())
            it = mSections.insert(mSections.end(), { std::string(section), {} });

        Answer& entry = it->second[std::string(answer)];
        ++entry.mCount;
        // A file that has several bodies gives the same answer several times
        if (entry.mExamples.size() < mExamples && (entry.mExamples.empty() || entry.mExamples.back() != file))
            entry.mExamples.emplace_back(file);
    }

    std::size_t HavokSurvey::count(std::string_view section, std::string_view answer) const
    {
        const auto it = std::ranges::find_if(mSections, [&](const auto& entry) { return entry.first == section; });
        if (it == mSections.end())
            return 0;
        const auto answerIt = it->second.find(std::string(answer));
        return answerIt == it->second.end() ? 0 : answerIt->second.mCount;
    }

    void HavokSurvey::print(std::ostream& out) const
    {
        const std::ios_base::fmtflags flags = out.flags();
        const std::streamsize precision = out.precision();
        for (const auto& [section, answers] : mSections)
        {
            std::size_t total = 0;
            for (const auto& [text, answer] : answers)
                total += answer.mCount;

            out << "\n" << section << " (" << total << ")\n";
            for (const auto& [text, answer] : answers)
            {
                out << std::setw(8) << answer.mCount << std::setw(7) << std::fixed << std::setprecision(1)
                    << (100.0 * answer.mCount / total) << "%  " << text;
                for (std::size_t i = 0; i < answer.mExamples.size(); ++i)
                    out << (i == 0 ? "   e.g. " : ", ") << answer.mExamples[i];
                out << "\n";
            }
        }
        out.flags(flags);
        out.precision(precision);
        out.flush();
    }

    std::string havokNumberAnswer(unsigned number)
    {
        std::string result = std::to_string(number);
        if (result.size() < 2)
            result.insert(0, 2 - result.size(), '0');
        return result;
    }

    std::string havokSizeRatioAnswer(float ratio)
    {
        if (!(ratio >= 0.25f))
            return "1: under 0.25";
        if (ratio < 0.5f)
            return "2: 0.25 to 0.5";
        if (ratio < 0.8f)
            return "3: 0.5 to 0.8";
        if (ratio <= 1.25f)
            return "4: 0.8 to 1.25";
        if (ratio <= 2.f)
            return "5: 1.25 to 2";
        if (ratio <= 4.f)
            return "6: 2 to 4";
        return "7: over 4";
    }

    std::string havokOffsetAnswer(float offset)
    {
        if (!(offset > 0.1f))
            return "1: up to 0.1";
        if (offset <= 0.25f)
            return "2: 0.1 to 0.25";
        if (offset <= 0.5f)
            return "3: 0.25 to 0.5";
        return "4: over 0.5";
    }
}

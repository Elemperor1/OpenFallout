#include "creaturecensus.hpp"

#include <algorithm>
#include <cstring>
#include <exception>
#include <iomanip>
#include <ostream>
#include <stdexcept>

#include <components/esm/common.hpp>

#include "censusreading.hpp"
#include "common.hpp"
#include "reader.hpp"
#include "readerutils.hpp"

namespace ESM4
{
    using namespace CensusReading;

    namespace
    {
        // DATA of Fallout 3 and New Vegas: the type of creature, then skills, health, damage and attributes
        constexpr std::uint16_t dataSize = 17;

        // ACBS: flags, fatigue, barter gold, level, calculated minimum and maximum, speed multiplier, karma, base
        // disposition and the flags of the template
        constexpr std::uint16_t acbsSize = 24;
        constexpr std::size_t acbsTemplateFlagsOffset = 22;
        constexpr std::uint16_t useModelFlag = 0x0040;

        bool hasFolder(const std::string& name)
        {
            return name.find_first_of("/\\") != std::string::npos;
        }

        std::string lowerFileName(const std::string& path)
        {
            const std::size_t slash = path.find_last_of("/\\");
            std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
            std::transform(name.begin(), name.end(), name.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return name;
        }

        // A list with no data is read as a list of one empty name
        void dropEmptyNames(std::vector<std::string>& names)
        {
            names.erase(std::remove(names.begin(), names.end(), std::string()), names.end());
        }

        void add(CreatureCensus::Answer& answer, const std::string& example)
        {
            ++answer.mCount;
            if (answer.mExamples.size() < CreatureCensus::maxExamples
                && std::find(answer.mExamples.begin(), answer.mExamples.end(), example) == answer.mExamples.end())
                answer.mExamples.push_back(example);
        }
    }

    std::string CreatureCensus::creatureTypeName(int type)
    {
        static const char* const names[] = { "Animal", "MutatedAnimal", "MutatedInsect", "Abomination", "SuperMutant",
            "FeralGhoul", "Robot", "Giant" };
        if (type == -1)
            return "none";
        if (type >= 0 && static_cast<std::size_t>(type) < std::size(names))
            return names[type];
        return "type " + std::to_string(type);
    }

    void CreatureCensus::collect(Reader& reader)
    {
        auto readCreature = [](Reader& r) {
            Creature creature;
            forEachSubRecord(r, [&](std::uint32_t type, std::uint16_t size) {
                switch (type)
                {
                    case ESM::fourCC("MODL"):
                        if (!r.getZString(creature.mModel))
                            throw std::runtime_error("The file ends inside a model");
                        return true;
                    case ESM::fourCC("NIFZ"):
                        if (!r.getZeroTerminatedStringArray(creature.mBodyFiles))
                            throw std::runtime_error("The file ends inside a list of models");
                        dropEmptyNames(creature.mBodyFiles);
                        return true;
                    case ESM::fourCC("KFFZ"):
                        if (!r.getZeroTerminatedStringArray(creature.mAnimationFiles))
                            throw std::runtime_error("The file ends inside a list of animations");
                        dropEmptyNames(creature.mAnimationFiles);
                        return true;
                    case ESM::fourCC("DATA"):
                    {
                        if (size != dataSize)
                            return false;
                        std::array<char, dataSize> data;
                        readBytes(r, data, data.size());
                        creature.mType = static_cast<std::uint8_t>(data[0]);
                        return true;
                    }
                    case ESM::fourCC("ACBS"):
                    {
                        if (size != acbsSize)
                            return false;
                        std::array<char, acbsSize> data;
                        readBytes(r, data, data.size());
                        std::uint16_t templateFlags = 0;
                        std::memcpy(&templateFlags, data.data() + acbsTemplateFlagsOffset, sizeof(templateFlags));
                        creature.mUseModel = (templateFlags & useModelFlag) != 0;
                        return true;
                    }
                    case ESM::fourCC("TPLT"):
                        creature.mTemplate = true;
                        return false;
                    case ESM::fourCC("PNAM"):
                        creature.mBodyParts = true;
                        return false;
                    default:
                        return false;
                }
            });
            return creature;
        };

        auto visitRecord = [&](Reader& r) {
            if (r.hdr().record.typeId != REC_CREA)
                return false;

            const ESM::FormId id = r.getFormIdFromHeader();
            const ReaderContext recordStart = r.getContext();
            mCreatures.erase(id);

            if ((r.hdr().record.flags & Rec_Deleted) == 0)
            {
                try
                {
                    r.getRecordData();
                    mCreatures[id] = readCreature(r);
                }
                catch (const std::exception&)
                {
                    // The record cannot be read, so it counts for nothing, not even as the one an earlier file had.
                    mCreatures.erase(id);
                }
            }
            r.skipFailedRecord(recordStart);
            return true;
        };

        try
        {
            ReaderUtils::readAll(reader, visitRecord, [](Reader&) {});
        }
        catch (const std::exception& e)
        {
            // Raised by the reader while it walks record and group headers, so it holds offsets and sizes only.
            mFatalErrors.push_back(firstLine(e.what()));
        }
    }

    CreatureCensus::Summary CreatureCensus::summarize() const
    {
        Summary summary;
        for (const auto& [id, creature] : mCreatures)
        {
            ++summary.mCreatures;
            ++summary.mTypes[creature.mType];
            summary.mWithTemplate += creature.mTemplate ? 1 : 0;
            summary.mWithBodyParts += creature.mBodyParts ? 1 : 0;
            summary.mUseModelFromTemplate += creature.mUseModel ? 1 : 0;

            if (creature.mModel.empty())
                add(summary.mNoModel, creature.mModel);
            else
            {
                add(lowerFileName(creature.mModel) == "skeleton.nif" ? summary.mSkeleton : summary.mOtherModel,
                    creature.mModel);
                add(hasFolder(creature.mModel) ? summary.mModelWithFolder : summary.mModelWithoutFolder,
                    creature.mModel);
            }

            ++summary.mBodyFiles[std::min(creature.mBodyFiles.size(), maxFiles)];
            for (const std::string& name : creature.mBodyFiles)
                add(hasFolder(name) ? summary.mBodyFileWithFolder : summary.mBodyFileWithoutFolder, name);
            ++summary.mAnimationFiles[std::min(creature.mAnimationFiles.size(), maxFiles)];
            for (const std::string& name : creature.mAnimationFiles)
                add(hasFolder(name) ? summary.mAnimationFileWithFolder : summary.mAnimationFileWithoutFolder, name);
        }
        return summary;
    }

    void CreatureCensus::write(std::ostream& stream) const
    {
        const Summary summary = summarize();
        constexpr int nameWidth = 16;
        constexpr int countWidth = 9;

        const auto writeAnswer = [&](const char* label, const Answer& answer) {
            stream << "  " << label << ": " << answer.mCount;
            for (std::size_t i = 0; i < answer.mExamples.size(); ++i)
                stream << (i == 0 ? "   e.g. " : ", ") << answer.mExamples[i];
            stream << '\n';
        };
        const auto writeHistogram = [&](const std::array<std::size_t, maxFiles + 1>& histogram) {
            for (std::size_t number = 0; number <= maxFiles; ++number)
                stream << "  " << number << (number == maxFiles ? " or more" : "") << ": " << histogram[number] << '\n';
        };

        stream << "Creatures (CREA records): " << summary.mCreatures << '\n';
        stream << "  with a template: " << summary.mWithTemplate << '\n';
        stream << "  that use the model of their template: " << summary.mUseModelFromTemplate << '\n';
        stream << "  with body part data: " << summary.mWithBodyParts << '\n';

        stream << "\nCreatures by type (DATA)\n";
        for (const auto& [type, number] : summary.mTypes)
            stream << "  " << std::left << std::setw(nameWidth) << creatureTypeName(type) << std::right
                   << std::setw(countWidth) << number << '\n';

        stream << "\nThe model of the record (MODL)\n";
        writeAnswer("no model", summary.mNoModel);
        writeAnswer("a file called skeleton.nif", summary.mSkeleton);
        writeAnswer("another file", summary.mOtherModel);
        writeAnswer("with a folder in the name", summary.mModelWithFolder);
        writeAnswer("with no folder in the name", summary.mModelWithoutFolder);

        stream << "\nCreatures by how many models of the body they list (NIFZ)\n";
        writeHistogram(summary.mBodyFiles);
        writeAnswer("names with a folder", summary.mBodyFileWithFolder);
        writeAnswer("names with no folder", summary.mBodyFileWithoutFolder);

        stream << "\nCreatures by how many animation files they list (KFFZ)\n";
        writeHistogram(summary.mAnimationFiles);
        writeAnswer("names with a folder", summary.mAnimationFileWithFolder);
        writeAnswer("names with no folder", summary.mAnimationFileWithoutFolder);

        if (!mFatalErrors.empty())
        {
            stream << "\nReading stopped early\n";
            for (const std::string& error : mFatalErrors)
                stream << "  " << error << '\n';
        }
    }
}

#include "packagecensus.hpp"

#include <algorithm>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iterator>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>

#include <components/esm/common.hpp>

#include "censusreading.hpp"
#include "common.hpp"
#include "packageschedule.hpp"
#include "reader.hpp"
#include "readerutils.hpp"

namespace ESM4
{
    using namespace CensusReading;

    namespace
    {
        // PKDT: the flags, then the type of the package in the first byte after them
        constexpr std::size_t packageTypeOffset = 4;
        constexpr std::size_t packageDataMinimum = packageTypeOffset + 1;

        // PSDT: month, day of the week, date, hour and duration. A month, day and hour of 0xff are any, and so is
        // the date 0.
        constexpr std::uint16_t scheduleSize = 8;
        constexpr std::uint8_t any = 0xff;

        // The first four bytes of PLDT and PTDT are the type of the location and of the target
        constexpr std::size_t typeSize = sizeof(std::int32_t);

        // ACBS: flags, fatigue, barter gold, level, calculated minimum and maximum, speed multiplier, karma, base
        // disposition and the flags of the template
        constexpr std::uint16_t acbsSize = 24;
        constexpr std::size_t acbsTemplateFlagsOffset = 22;
        constexpr std::uint16_t useAIPackageFlag = 0x0020;

        int readType(Reader& r, std::uint16_t size)
        {
            std::array<char, 16> data{};
            readBytes(r, data, std::min<std::size_t>(size, data.size()));
            if (size > data.size())
                r.skipSubRecordData(static_cast<std::uint32_t>(size - data.size()));
            std::int32_t type;
            std::memcpy(&type, data.data(), sizeof(type));
            return type;
        }
    }

    std::string PackageCensus::packageTypeName(int type)
    {
        static const char* const names[]
            = { "Find", "Follow", "Escort", "Eat", "Sleep", "Wander", "Travel", "Accompany", "UseItemAt", "Ambush",
                  "FleeNotCombat", "CastMagic", "Sandbox", "Patrol", "Guard", "Dialogue", "UseWeapon" };
        if (type == noType)
            return "none";
        if (type >= 0 && static_cast<std::size_t>(type) < std::size(names))
            return names[type];
        return "type " + std::to_string(type);
    }

    void PackageCensus::collect(Reader& reader)
    {
        auto readPackage = [](Reader& r) {
            Package package;
            forEachSubRecord(r, [&](std::uint32_t type, std::uint16_t size) {
                switch (type)
                {
                    case ESM::fourCC("PKDT"):
                    {
                        if (size < packageDataMinimum)
                            return false;
                        std::array<char, 16> data{};
                        readBytes(r, data, std::min<std::size_t>(size, data.size()));
                        if (size > data.size())
                            r.skipSubRecordData(static_cast<std::uint32_t>(size - data.size()));
                        package.mType = static_cast<std::uint8_t>(data[packageTypeOffset]);
                        return true;
                    }
                    case ESM::fourCC("PSDT"):
                    {
                        if (size != scheduleSize)
                            return false;
                        std::array<char, scheduleSize> data;
                        readBytes(r, data, data.size());
                        package.mMonth = static_cast<std::uint8_t>(data[0]) != any;
                        package.mDay = static_cast<std::uint8_t>(data[1]) != any;
                        package.mDate = data[2] != 0;
                        package.mTime = static_cast<std::uint8_t>(data[3]) != any;
                        package.mSchedule.month = static_cast<std::uint8_t>(data[0]);
                        package.mSchedule.dayOfWeek = static_cast<std::uint8_t>(data[1]);
                        package.mSchedule.date = static_cast<std::uint8_t>(data[2]);
                        package.mSchedule.time = static_cast<std::uint8_t>(data[3]);
                        std::memcpy(&package.mSchedule.duration, data.data() + 4, sizeof(std::uint32_t));
                        return true;
                    }
                    case ESM::fourCC("PLDT"):
                        if (size < typeSize)
                            return false;
                        package.mLocation = readType(r, size);
                        return true;
                    case ESM::fourCC("PTDT"):
                        if (size < typeSize)
                            return false;
                        package.mTarget = readType(r, size);
                        return true;
                    case ESM::fourCC("CTDA"):
                        ++package.mConditions;
                        return false;
                    case ESM::fourCC("SCHR"):
                    case ESM::fourCC("INAM"):
                    case ESM::fourCC("TNAM"):
                        package.mEvents = true;
                        return false;
                    default:
                        return false;
                }
            });
            return package;
        };

        auto readCharacter = [](Reader& r) {
            Character character;
            forEachSubRecord(r, [&](std::uint32_t type, std::uint16_t size) {
                if (type == ESM::fourCC("PKID") && size == sizeof(std::uint32_t))
                {
                    ESM::FormId id;
                    if (!r.getFormId(id))
                        throw std::runtime_error("The file ends inside a package");
                    character.mPackages.push_back(id);
                    return true;
                }
                if (type == ESM::fourCC("ACBS") && size == acbsSize)
                {
                    std::array<char, acbsSize> data;
                    readBytes(r, data, data.size());
                    std::uint16_t templateFlags = 0;
                    std::memcpy(&templateFlags, data.data() + acbsTemplateFlagsOffset, sizeof(templateFlags));
                    character.mPackagesFromTemplate = (templateFlags & useAIPackageFlag) != 0;
                    return true;
                }
                return false;
            });
            return character;
        };

        auto visitRecord = [&](Reader& r) {
            const std::uint32_t type = r.hdr().record.typeId;
            if (type != REC_PACK && type != REC_NPC_)
                return false;

            const ESM::FormId id = r.getFormIdFromHeader();
            const std::uint32_t flags = r.hdr().record.flags;
            const ReaderContext recordStart = r.getContext();
            mPackages.erase(id);
            mCharacters.erase(id);

            if ((flags & Rec_Deleted) == 0)
            {
                try
                {
                    r.getRecordData();
                    if (type == REC_PACK)
                        mPackages[id] = readPackage(r);
                    else
                        mCharacters[id] = readCharacter(r);
                }
                catch (const std::exception&)
                {
                    // The record cannot be read, so it counts for nothing, not even as the one an earlier file had.
                    mPackages.erase(id);
                    mCharacters.erase(id);
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

    PackageCensus::Summary PackageCensus::summarize() const
    {
        Summary summary;

        for (const auto& [id, package] : mPackages)
        {
            ++summary.mPackages;
            ++summary.mTypes[package.mType];
            ++summary.mLocations[package.mLocation];
            ++summary.mTargets[package.mTarget];
            summary.mWithTime += package.mTime ? 1 : 0;
            summary.mWithDay += package.mDay ? 1 : 0;
            summary.mWithMonth += package.mMonth ? 1 : 0;
            summary.mWithDate += package.mDate ? 1 : 0;
            summary.mWithConditions += package.mConditions != 0 ? 1 : 0;
            summary.mWithEvents += package.mEvents ? 1 : 0;
            if (package.mTime)
                ++summary.mDurations[package.mSchedule.duration];
        }

        for (const auto& [id, character] : mCharacters)
        {
            ++summary.mCharacters;
            if (character.mPackagesFromTemplate)
            {
                ++summary.mPackagesFromTemplate;
                continue;
            }
            if (character.mPackages.empty())
            {
                ++summary.mNoPackages;
                continue;
            }

            ++summary.mListed[std::min(character.mPackages.size(), maxPackages)];
            std::set<int> types;
            bool timed = false;
            bool first = true;
            for (const ESM::FormId listed : character.mPackages)
            {
                const auto it = mPackages.find(listed);
                if (it == mPackages.end())
                {
                    ++summary.mListedNoRecord;
                    first = false;
                    continue;
                }
                if (first)
                    ++summary.mFirstType[it->second.mType];
                first = false;
                types.insert(it->second.mType);
                timed = timed || it->second.mTime;
            }
            for (const int type : types)
                ++summary.mAnyType[type];
            if (timed)
                ++summary.mCharactersWithTime;

            for (int hour = 0; hour < hoursInDay; ++hour)
            {
                PackageClock clock;
                clock.mHour = static_cast<float>(hour);
                clock.mDayOfWeek = 1;
                int followed = followsNone;
                for (const ESM::FormId listed : character.mPackages)
                {
                    const auto it = mPackages.find(listed);
                    if (it != mPackages.end()
                        && packageSkip(it->second.mType, it->second.mSchedule, it->second.mConditions != 0, clock)
                            == PackageSkip::None)
                    {
                        followed = it->second.mType;
                        break;
                    }
                }
                ++summary.mFollowedByHour[hour][followed];
            }
        }

        return summary;
    }

    void PackageCensus::write(std::ostream& stream) const
    {
        const Summary summary = summarize();
        constexpr int nameWidth = 16;
        constexpr int countWidth = 9;

        const auto writeMap = [&](const std::map<int, std::size_t>& counts, bool packageType) {
            for (const auto& [type, number] : counts)
                stream << "  " << std::left << std::setw(nameWidth)
                       << (packageType ? packageTypeName(type) : std::to_string(type)) << std::right
                       << std::setw(countWidth) << number << '\n';
        };

        stream << "Package records: " << summary.mPackages << '\n';
        stream << "  with an hour of the day: " << summary.mWithTime << '\n';
        stream << "  with a day of the week: " << summary.mWithDay << '\n';
        stream << "  with a month: " << summary.mWithMonth << '\n';
        stream << "  with a date: " << summary.mWithDate << '\n';
        stream << "  with conditions: " << summary.mWithConditions << '\n';
        stream << "  with an idle, a script or a topic for when it begins, ends or changes: " << summary.mWithEvents
               << '\n';

        stream << "\nPackages that start at an hour of the day, by the hours they last\n";
        for (const auto& [hours, number] : summary.mDurations)
            stream << "  " << std::left << std::setw(nameWidth) << hours << std::right << std::setw(countWidth)
                   << number << '\n';

        stream << "\nPackages by type\n";
        writeMap(summary.mTypes, true);
        stream << "\nPackages by the type of their location (-1 is no location)\n";
        writeMap(summary.mLocations, false);
        stream << "\nPackages by the type of their target (-1 is no target)\n";
        writeMap(summary.mTargets, false);

        stream << "\nCharacters (NPC_ records): " << summary.mCharacters << '\n';
        stream << "  whose packages are those of their template: " << summary.mPackagesFromTemplate << '\n';
        stream << "  with no packages: " << summary.mNoPackages << '\n';
        stream << "  with a package that has an hour of the day: " << summary.mCharactersWithTime << '\n';

        stream << "\nCharacters with packages of their own, by how many they list\n";
        for (std::size_t number = 1; number <= maxPackages; ++number)
            stream << "  " << number << (number == maxPackages ? " or more" : "") << ": " << summary.mListed[number]
                   << '\n';
        stream << "  packages with no record that was read: " << summary.mListedNoRecord << '\n';

        stream << "\nCharacters by the type of their first package (the one with the highest priority)\n";
        writeMap(summary.mFirstType, true);
        stream << "\nCharacters that have a package of the type\n";
        writeMap(summary.mAnyType, true);

        stream << "\nThe package that characters with packages of their own follow, by the hour of the day on a Monday "
                  "(the first of their packages that is on, has no conditions and is of a kind the game does something "
                  "for)\n";
        for (int hour = 0; hour < hoursInDay; ++hour)
        {
            stream << "  " << std::setfill('0') << std::setw(2) << hour << std::setfill(' ') << ":00";
            for (const auto& [type, number] : summary.mFollowedByHour[hour])
                stream << "  " << (type == followsNone ? "nothing" : packageTypeName(type)) << ' ' << number;
            stream << '\n';
        }

        if (!mFatalErrors.empty())
        {
            stream << "\nReading stopped early\n";
            for (const std::string& error : mFatalErrors)
                stream << "  " << error << '\n';
        }
    }
}

#ifndef OPENFALLOUT_COMPONENTS_ESM4_PACKAGECENSUS_H
#define OPENFALLOUT_COMPONENTS_ESM4_PACKAGECENSUS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <components/esm/formid.hpp>

#include "loadpack.hpp"

namespace ESM4
{
    class Reader;

    // Counts the AI packages of one or more Fallout 3 and New Vegas plugins and the packages that their characters
    // list: the kinds of package, how many of them have a schedule, a location or a target and conditions, and how
    // many packages the characters have and of which kinds. It is for choosing what a character does and when, so what
    // it keeps is record types, the codes of the kinds and counts, never record contents.
    //
    // The files must be read in load order, each with its mod index and the indices of its masters set on the reader
    // (Reader::setModIndex and Reader::updateModIndices). A record that a later file defines again counts once, as the
    // later file has it, and one that a later file deletes, or that cannot be read, does not count.
    class PackageCensus
    {
    public:
        // The kind of a package is the byte after the flags of PKDT. What the games name by number is in
        // packageTypeName().
        static constexpr int noType = -1; // no PKDT of 5 bytes or more
        static constexpr int noLocation = -1; // no PLDT
        static constexpr int noTarget = -1; // no PTDT
        // In the packages followed at an hour: the character follows none of the packages that it lists
        static constexpr int followsNone = -2;
        static constexpr int hoursInDay = 24;
        static std::string packageTypeName(int type);

        // A character with more packages than this counts as having this many.
        static constexpr std::size_t maxPackages = 8;

        struct Summary
        {
            // The PACK records
            std::size_t mPackages = 0;
            std::map<int, std::size_t> mTypes;
            std::size_t mWithTime = 0; // starts at a time of day
            std::size_t mWithDay = 0; // on a day of the week
            std::size_t mWithMonth = 0;
            std::size_t mWithDate = 0;
            std::map<int, std::size_t> mLocations; // by the type in PLDT
            std::map<int, std::size_t> mTargets; // by the type in PTDT
            std::size_t mWithConditions = 0;
            std::size_t mWithEvents = 0; // runs something when it begins, ends or changes
            std::map<std::uint32_t, std::size_t> mDurations; // by the hours they last, those that start at a time

            // The NPC_ records
            std::size_t mCharacters = 0;
            std::size_t mPackagesFromTemplate = 0; // they list packages, but the game takes those of the template
            std::size_t mNoPackages = 0;
            std::array<std::size_t, maxPackages + 1> mListed{}; // characters that list n packages
            std::size_t mListedNoRecord = 0; // listed packages that no file read has a record for
            std::map<int, std::size_t> mFirstType; // the type of the first package, the one with the highest priority
            std::map<int, std::size_t> mAnyType; // characters that have a package of the type
            std::size_t mCharactersWithTime = 0; // characters that have a package that starts at a time of day
            // The package that characters with packages of their own follow on a Monday at each hour, by its type
            // (see ESM4::choosePackage): followsNone for those that follow none, because no package of theirs is on,
            // has conditions or is of a kind that the game does nothing for.
            std::array<std::map<int, std::size_t>, hoursInDay> mFollowedByHour;
        };

        void collect(Reader& reader);

        Summary summarize() const;

        const std::vector<std::string>& getFatalErrors() const { return mFatalErrors; }

        void write(std::ostream& stream) const;

    private:
        struct Package
        {
            int mType = noType;
            int mLocation = noLocation;
            int mTarget = noTarget;
            bool mTime = false;
            bool mDay = false;
            bool mMonth = false;
            bool mDate = false;
            AIPackage::PSDT mSchedule;
            std::size_t mConditions = 0;
            bool mEvents = false;
        };

        struct Character
        {
            std::vector<ESM::FormId> mPackages;
            bool mPackagesFromTemplate = false;
        };

        std::unordered_map<ESM::FormId, Package> mPackages;
        std::unordered_map<ESM::FormId, Character> mCharacters;
        std::vector<std::string> mFatalErrors;
    };
}

#endif

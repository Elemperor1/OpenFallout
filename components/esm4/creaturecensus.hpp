#ifndef OPENFALLOUT_COMPONENTS_ESM4_CREATURECENSUS_H
#define OPENFALLOUT_COMPONENTS_ESM4_CREATURECENSUS_H

#include <array>
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

    // Counts what the CREA records of one or more Fallout 3 and New Vegas plugins say about how a creature is made:
    // the kind of creature, the model that the record names (in these games a skeleton, with the models of the body
    // listed apart in NIFZ and the animation files in KFFZ), how many of each there are, and whether the names carry a
    // folder. It is for building the body of a creature from them, so what it keeps is counts and, for the model
    // and the file names, a few example names of each answer (file names of the game, never other record contents).
    //
    // The files must be read in load order, each with its mod index and the indices of its masters set on the reader
    // (Reader::setModIndex and Reader::updateModIndices). A record that a later file defines again counts once, as
    // the later file has it, and one that a later file deletes, or that cannot be read, does not count.
    class CreatureCensus
    {
    public:
        // A creature with more files than this in a list counts as having this many.
        static constexpr std::size_t maxFiles = 8;

        // The most example names kept for an answer
        static constexpr std::size_t maxExamples = 3;

        struct Answer
        {
            std::size_t mCount = 0;
            std::vector<std::string> mExamples;
        };

        struct Summary
        {
            std::size_t mCreatures = 0;
            std::map<int, std::size_t> mTypes; // the type in DATA (-1 for a record without the 17 bytes of Fallout)
            std::size_t mWithTemplate = 0; // names a template (TPLT)
            std::size_t mWithBodyParts = 0; // names body part data (PNAM)
            std::size_t mUseModelFromTemplate = 0; // the model of the template is used, and not its own

            // What the model (MODL) of the record is
            Answer mNoModel;
            Answer mSkeleton; // a file called skeleton.nif
            Answer mOtherModel;
            Answer mModelWithFolder; // the model has a folder in its name (of those that have a model)
            Answer mModelWithoutFolder;

            // NIFZ and KFFZ: the number of creatures by how many files they list, and what the names are like
            std::array<std::size_t, maxFiles + 1> mBodyFiles{};
            Answer mBodyFileWithFolder;
            Answer mBodyFileWithoutFolder;
            std::array<std::size_t, maxFiles + 1> mAnimationFiles{};
            Answer mAnimationFileWithFolder;
            Answer mAnimationFileWithoutFolder;
        };

        void collect(Reader& reader);

        Summary summarize() const;

        const std::vector<std::string>& getFatalErrors() const { return mFatalErrors; }

        void write(std::ostream& stream) const;

        static std::string creatureTypeName(int type);

    private:
        struct Creature
        {
            int mType = -1;
            bool mTemplate = false;
            bool mUseModel = false;
            bool mBodyParts = false;
            std::string mModel;
            std::vector<std::string> mBodyFiles;
            std::vector<std::string> mAnimationFiles;
        };

        std::unordered_map<ESM::FormId, Creature> mCreatures;
        std::vector<std::string> mFatalErrors;
    };
}

#endif

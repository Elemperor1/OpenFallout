#ifndef OPENFALLOUT_ESMTOOL_TES4_H
#define OPENFALLOUT_ESMTOOL_TES4_H

#include <fstream>
#include <iosfwd>
#include <memory>

namespace EsmTool
{
    struct Arguments;

    int loadTes4(const Arguments& info, std::unique_ptr<std::ifstream>&& stream);

    int censusTes4(const Arguments& info, std::unique_ptr<std::ifstream>&& stream);

    // Surveys every file named in info.inputFiles and prints one report.
    int surveyTes4(const Arguments& info);

    // Counts the references placed by every file named in info.inputFiles, read in that order, and prints one report.
    int referencesTes4(const Arguments& info);

    // Counts the armour that the characters of every Fallout file named in info.inputFiles list, read in that order,
    // and prints one report.
    int equipmentTes4(const Arguments& info);

    // Counts the AI packages of every Fallout file named in info.inputFiles, and the packages that their characters
    // list, read in that order, and prints one report.
    int packagesTes4(const Arguments& info);

    // Counts how the creatures of every Fallout file named in info.inputFiles are made (model, body models and
    // animation files), read in that order, and prints one report.
    int creaturesTes4(const Arguments& info);
}

#endif

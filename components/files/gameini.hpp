#ifndef OPENFALLOUT_COMPONENTS_FILES_GAMEINI_H
#define OPENFALLOUT_COMPONENTS_FILES_GAMEINI_H

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Files
{
    /// The settings of an ini file of Fallout 3 or New Vegas (Fallout_default.ini, Fallout.ini, FalloutNV.ini):
    /// sections in brackets, `key=value` lines, lines that start with a semicolon are comments. Sections and keys are
    /// compared without regard to case. Where a key appears twice in a section the first one counts, as the Windows
    /// call the games read their ini files with gives it (the retail Fallout_default.ini of New Vegas lists sTestFile1
    /// twice, the second time empty). A later file merged over this one replaces a value whatever it is.
    class GameIni
    {
    public:
        /// Reads the settings of the text of an ini file. It never fails: a line it does not understand is skipped.
        static GameIni parse(std::string_view text);

        /// The value of a key (without the spaces around it), or nothing where the file does not have the key
        std::optional<std::string> get(std::string_view section, std::string_view key) const;

        /// The settings of the other file, which take the place of the ones this file has by the same section and key.
        /// The games read a default ini and then the one of the user, whose settings replace the default ones.
        void merge(const GameIni& other);

        /// The entries of a value that is a list, separated by commas: without the spaces around each one, empty ones
        /// left out.
        static std::vector<std::string> splitList(std::string_view value);

        /// The ini files at the paths, read in the order given and merged, so that a later file takes the place of the
        /// settings of an earlier one. A file that cannot be read is skipped.
        static GameIni load(const std::vector<std::filesystem::path>& paths);

    private:
        std::map<std::string, std::string> mValues; // "section\n key" in lower case
    };

    /// The ini files that sit in the folder above each data directory (the folder of the game, where the games ship
    /// Fallout_default.ini): Fallout_default.ini, then Fallout.ini and FalloutPrefs.ini where there are such. The
    /// directories are in the order of increasing priority, and so are the files. Names are compared without case.
    std::vector<std::filesystem::path> findGameInis(const std::vector<std::filesystem::path>& dataDirs);
}

#endif

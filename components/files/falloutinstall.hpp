#ifndef OPENFALLOUT_COMPONENTS_FILES_FALLOUTINSTALL_H
#define OPENFALLOUT_COMPONENTS_FILES_FALLOUTINSTALL_H

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Files
{
    enum class FalloutGame
    {
        Fallout3,
        NewVegas,
    };

    /// A copy of Fallout 3 or Fallout: New Vegas on this computer: the Data folder that holds the master file
    struct FalloutInstall
    {
        FalloutGame mGame;
        std::filesystem::path mData;
    };

    /// The paths of the libraries that the text of a Steam `libraryfolders.vdf` names. Both layouts are read: the
    /// current one, where each library is a block with a "path" in it, and the old one, where a number is followed
    /// by the path as a string. Backslashes written as `\\` are unescaped.
    std::vector<std::filesystem::path> parseSteamLibraries(std::string_view libraryFolders);

    /// The Data folder of the game in `folder`, which is either the folder of the game (with a Data folder in it) or
    /// a folder that has the master file itself (the Data folder, or the output folder of Tale of Two Wastelands).
    /// Folder and file names are compared without regard to case, as the games' files may have been copied to a file
    /// system that cares about it. Nothing where there is no Fallout3.esm or FalloutNV.esm. New Vegas wins where a
    /// folder has both (a Tale of Two Wastelands output folder is that).
    std::optional<FalloutInstall> checkGameFolder(const std::filesystem::path& folder);

    /// The copies of the games in the places given. `steamRoots` are Steam installations: their libraries are read
    /// from steamapps/libraryfolders.vdf and the folders of steamapps/common with "fallout" in their name are looked
    /// at. So are the folders with "fallout" in their name in each of `parents` (that is how a game installed by GOG,
    /// from a disc or by hand is found). New Vegas comes before Fallout 3 in the result, and nothing is listed twice.
    std::vector<FalloutInstall> findFalloutInstalls(
        const std::vector<std::filesystem::path>& steamRoots, const std::vector<std::filesystem::path>& parents);

    /// The places a game is usually installed to on this computer and `home`
    void defaultSearchPlaces(const std::filesystem::path& home, std::vector<std::filesystem::path>& steamRoots,
        std::vector<std::filesystem::path>& parents);

    /// The copies of the games in the usual places of this computer
    std::vector<FalloutInstall> findFalloutInstalls();

    const char* falloutGameName(FalloutGame game);
}

#endif

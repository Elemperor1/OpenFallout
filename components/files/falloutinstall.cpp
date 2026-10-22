#include "falloutinstall.hpp"

#include <components/files/conversion.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <system_error>

#if defined(_WIN32) || defined(__WINDOWS__)
#include <components/misc/windows.hpp>

#include <winreg.h>
#endif

namespace Files
{
    namespace
    {
        constexpr std::string_view sFalloutNVMaster = "FalloutNV.esm";
        constexpr std::string_view sFallout3Master = "Fallout3.esm";

        struct VdfToken
        {
            enum class Kind
            {
                String,
                Open,
                Close,
            };

            Kind mKind;
            std::string mText;
        };

        /// The strings and braces of a Valve data file. Comments (`//` to the end of a line) are skipped.
        std::vector<VdfToken> tokenizeVdf(std::string_view text)
        {
            std::vector<VdfToken> tokens;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char c = text[i];
                if (c == '{')
                    tokens.push_back({ VdfToken::Kind::Open, {} });
                else if (c == '}')
                    tokens.push_back({ VdfToken::Kind::Close, {} });
                else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/')
                {
                    while (i < text.size() && text[i] != '\n')
                        ++i;
                }
                else if (c == '"')
                {
                    std::string value;
                    for (++i; i < text.size() && text[i] != '"'; ++i)
                    {
                        if (text[i] == '\\' && i + 1 < text.size())
                            ++i;
                        value += text[i];
                    }
                    tokens.push_back({ VdfToken::Kind::String, std::move(value) });
                }
            }
            return tokens;
        }

        bool isNumber(const std::string& text)
        {
            return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
        }

        std::vector<std::filesystem::path> childFolders(const std::filesystem::path& folder)
        {
            std::vector<std::filesystem::path> result;
            std::error_code error;
            for (const auto& entry : std::filesystem::directory_iterator(folder, error))
            {
                std::error_code typeError;
                if (entry.is_directory(typeError))
                    result.push_back(entry.path());
            }
            std::sort(result.begin(), result.end());
            return result;
        }

        /// The child of `folder` that is called `name`, whatever the case of either
        std::optional<std::filesystem::path> findChild(
            const std::filesystem::path& folder, std::string_view name, bool wantFolder)
        {
            std::error_code error;
            for (const auto& entry : std::filesystem::directory_iterator(folder, error))
            {
                std::error_code typeError;
                if (!Misc::StringUtils::ciEqual(pathToUnicodeString(entry.path().filename()), name))
                    continue;
                if (wantFolder ? entry.is_directory(typeError) : entry.is_regular_file(typeError))
                    return entry.path();
            }
            return std::nullopt;
        }

        std::optional<FalloutGame> masterIn(const std::filesystem::path& dataFolder)
        {
            if (findChild(dataFolder, sFalloutNVMaster, false))
                return FalloutGame::NewVegas;
            if (findChild(dataFolder, sFallout3Master, false))
                return FalloutGame::Fallout3;
            return std::nullopt;
        }

        bool nameHasFallout(const std::filesystem::path& folder)
        {
            const std::string name = pathToUnicodeString(folder.filename());
            return Misc::StringUtils::lowerCase(name).find("fallout") != std::string::npos;
        }

        void addIfNew(std::vector<FalloutInstall>& installs, FalloutInstall install)
        {
            for (const FalloutInstall& known : installs)
            {
                std::error_code error;
                if (std::filesystem::equivalent(known.mData, install.mData, error))
                    return;
            }
            installs.push_back(std::move(install));
        }

        void addFolder(std::vector<std::filesystem::path>& folders, std::filesystem::path folder)
        {
            if (std::find(folders.begin(), folders.end(), folder) == folders.end())
                folders.push_back(std::move(folder));
        }

        void addIfFolder(std::vector<std::filesystem::path>& folders, const std::filesystem::path& folder)
        {
            std::error_code error;
            if (std::filesystem::is_directory(folder, error))
                addFolder(folders, folder);
        }

        /// The Wine prefix (the folder with a drive_c in it) that `path` is in, if it is in one
        std::optional<std::filesystem::path> winePrefixOf(std::filesystem::path path)
        {
            for (; path.has_relative_path(); path = path.parent_path())
            {
                std::error_code error;
                if (std::filesystem::is_directory(path / "drive_c", error))
                    return path;
            }
            return std::nullopt;
        }

        /// A library that Steam, running in the Wine prefix `prefix`, wrote as `D:\SteamLibrary`, as the folder of this
        /// computer: the C drive is the drive_c of the prefix, the other drives are what its dosdevices/d: lead to
        std::filesystem::path windowsPathInPrefix(
            const std::filesystem::path& prefix, const std::filesystem::path& library)
        {
            const std::string text = pathToUnicodeString(library);
            if (text.size() < 2 || text[1] != ':' || !std::isalpha(static_cast<unsigned char>(text[0])))
                return library;
            const char letter = static_cast<char>(std::tolower(static_cast<unsigned char>(text[0])));
            std::string rest = text.substr(2);
            std::replace(rest.begin(), rest.end(), '\\', '/');
            const std::filesystem::path drive
                = letter == 'c' ? prefix / "drive_c" : prefix / "dosdevices" / (std::string(1, letter) + ':');
            const std::size_t start = rest.find_first_not_of('/');
            return start == std::string::npos ? drive : drive / pathFromUnicodeString(rest.substr(start));
        }

        /// The libraries of the Steam installation in `root`, `root` itself first
        std::vector<std::filesystem::path> steamLibraries(const std::filesystem::path& root)
        {
            std::vector<std::filesystem::path> libraries;
            addFolder(libraries, root);
            const std::optional<std::filesystem::path> steamapps = findChild(root, "steamapps", true);
            if (!steamapps)
                return libraries;
            if (const auto file = findChild(*steamapps, "libraryfolders.vdf", false))
            {
                std::error_code error;
                const auto size = std::filesystem::file_size(*file, error);
                if (!error && size < 1u << 20)
                {
                    std::string text(static_cast<std::size_t>(size), '\0');
                    if (std::ifstream stream(*file, std::ios::binary); stream.read(text.data(), text.size()))
                    {
                        // A Steam that runs in Wine names its libraries by the drives of Windows
                        const std::optional<std::filesystem::path> prefix = winePrefixOf(root);
                        for (std::filesystem::path& library : parseSteamLibraries(text))
                            addFolder(libraries, prefix ? windowsPathInPrefix(*prefix, library) : std::move(library));
                    }
                }
            }
            return libraries;
        }

#if defined(_WIN32) || defined(__WINDOWS__)
        std::filesystem::path readRegistryString(HKEY root, const wchar_t* subKey, const wchar_t* valueName)
        {
            DWORD size = 0;
            if (RegGetValueW(root, subKey, valueName, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS
                || size < sizeof(wchar_t))
                return {};
            std::wstring buffer(size / sizeof(wchar_t), L'\0');
            if (RegGetValueW(root, subKey, valueName, RRF_RT_REG_SZ, nullptr, buffer.data(), &size) != ERROR_SUCCESS)
                return {};
            while (!buffer.empty() && buffer.back() == L'\0')
                buffer.pop_back();
            return std::filesystem::path(buffer);
        }
#endif
    }

    std::vector<std::filesystem::path> parseSteamLibraries(std::string_view libraryFolders)
    {
        std::vector<std::filesystem::path> libraries;
        const std::vector<VdfToken> tokens = tokenizeVdf(libraryFolders);
        int depth = 0;
        for (std::size_t i = 0; i < tokens.size(); ++i)
        {
            const VdfToken& token = tokens[i];
            if (token.mKind == VdfToken::Kind::Open)
                ++depth;
            else if (token.mKind == VdfToken::Kind::Close)
                --depth;
            else if (i + 1 < tokens.size() && tokens[i + 1].mKind == VdfToken::Kind::String)
            {
                const std::string& value = tokens[i + 1].mText;
                // The current layout has "path" in a block for each library, the old one "1" "D:\\Library" in the
                // block of the file
                if (!value.empty() && (token.mText == "path" || (depth == 1 && isNumber(token.mText))))
                    libraries.push_back(pathFromUnicodeString(value));
                ++i;
            }
        }
        return libraries;
    }

    std::optional<FalloutInstall> checkGameFolder(const std::filesystem::path& folder)
    {
        std::vector<std::filesystem::path> candidates;
        if (const auto data = findChild(folder, "Data", true))
            candidates.push_back(*data);
        candidates.push_back(folder);
        for (const std::filesystem::path& candidate : candidates)
            if (const auto game = masterIn(candidate))
                return FalloutInstall{ *game, candidate };
        return std::nullopt;
    }

    std::vector<FalloutInstall> findFalloutInstalls(
        const std::vector<std::filesystem::path>& steamRoots, const std::vector<std::filesystem::path>& parents)
    {
        std::vector<std::filesystem::path> folders;
        for (const std::filesystem::path& root : steamRoots)
        {
            for (const std::filesystem::path& library : steamLibraries(root))
            {
                const auto steamapps = findChild(library, "steamapps", true);
                const auto common = steamapps ? findChild(*steamapps, "common", true) : std::nullopt;
                if (!common)
                    continue;
                for (const std::filesystem::path& game : childFolders(*common))
                    if (nameHasFallout(game))
                        addFolder(folders, game);
            }
        }
        for (const std::filesystem::path& parent : parents)
            for (const std::filesystem::path& game : childFolders(parent))
                if (nameHasFallout(game))
                    addFolder(folders, game);

        std::vector<FalloutInstall> installs;
        for (const std::filesystem::path& folder : folders)
            if (std::optional<FalloutInstall> install = checkGameFolder(folder))
                addIfNew(installs, std::move(*install));
        std::stable_sort(installs.begin(), installs.end(),
            [](const FalloutInstall& left, const FalloutInstall& right) { return left.mGame > right.mGame; });
        return installs;
    }

    void defaultSearchPlaces(const std::filesystem::path& home, std::vector<std::filesystem::path>& steamRoots,
        std::vector<std::filesystem::path>& parents)
    {
#if defined(_WIN32) || defined(__WINDOWS__)
        (void)home;
        for (const std::filesystem::path& steam :
            { readRegistryString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath"),
                readRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath") })
            if (!steam.empty())
                addIfFolder(steamRoots, steam);
        const DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; ++i)
        {
            if (!(drives & (1u << i)))
                continue;
            const std::wstring root = { static_cast<wchar_t>(L'A' + i), L':', L'\\' };
            if (GetDriveTypeW(root.c_str()) != DRIVE_FIXED)
                continue;
            const std::filesystem::path drive(root);
            addIfFolder(steamRoots, drive / "Program Files (x86)" / "Steam");
            addIfFolder(steamRoots, drive / "Steam");
            for (const char* parent : { "GOG Games", "Games", "Program Files (x86)", "Program Files",
                     "Program Files (x86)/Bethesda Softworks", "Program Files/Bethesda Softworks",
                     "Program Files (x86)/GOG Galaxy/Games", "Program Files/Epic Games" })
                addIfFolder(parents, drive / parent);
        }
#else
        if (home.empty())
            return;
        // The Steam client of Linux (the games run through Proton, but their files are in the library) and of macOS
        for (const char* steam :
            { ".steam/steam", ".local/share/Steam", ".var/app/com.valvesoftware.Steam/.local/share/Steam",
                "snap/steam/common/.local/share/Steam", "Library/Application Support/Steam" })
            addIfFolder(steamRoots, home / steam);

        // The Windows drives of Wine and the programs built on it, which hold Steam, GOG and Bethesda's installers
        std::vector<std::filesystem::path> drives;
        addIfFolder(drives, home / ".wine/drive_c");
        for (const char* bottles : { "Library/Application Support/CrossOver/Bottles", ".local/share/bottles/bottles",
                 ".var/app/com.usebottles.bottles/data/bottles/bottles", "Games", "Games/Heroic/Prefixes/default" })
            for (const std::filesystem::path& bottle : childFolders(home / bottles))
                addIfFolder(drives, bottle / "drive_c");
        for (const std::filesystem::path& drive : drives)
        {
            addIfFolder(steamRoots, drive / "Program Files (x86)" / "Steam");
            addIfFolder(steamRoots, drive / "Program Files" / "Steam");
            for (const char* parent : { "GOG Games", "Games", "Program Files (x86)", "Program Files",
                     "Program Files (x86)/Bethesda Softworks", "Program Files/Bethesda Softworks",
                     "Program Files (x86)/GOG Galaxy/Games", "Program Files/Epic Games" })
                addIfFolder(parents, drive / parent);
        }

        // Copies of the files that were put in a folder by hand (not Documents or Desktop: macOS asks the user before
        // a program looks into those)
        for (const char* parent : { "Games", "Applications" })
            addIfFolder(parents, home / parent);
#endif
    }

    std::vector<FalloutInstall> findFalloutInstalls()
    {
        std::filesystem::path home;
#if !(defined(_WIN32) || defined(__WINDOWS__))
        if (const char* variable = std::getenv("HOME"))
            home = pathFromUnicodeString(variable);
#endif
        std::vector<std::filesystem::path> steamRoots;
        std::vector<std::filesystem::path> parents;
        defaultSearchPlaces(home, steamRoots, parents);
        return findFalloutInstalls(steamRoots, parents);
    }

    const char* falloutGameName(FalloutGame game)
    {
        return game == FalloutGame::NewVegas ? "Fallout: New Vegas" : "Fallout 3";
    }
}

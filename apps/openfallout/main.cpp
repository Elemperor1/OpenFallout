#include <components/debug/debugging.hpp>
#include <components/esm4/pluginorder.hpp>
#include <components/fallback/fallback.hpp>
#include <components/fallback/validate.hpp>
#include <components/files/collections.hpp>
#include <components/files/configurationmanager.hpp>
#include <components/files/gameini.hpp>
#include <components/files/openfile.hpp>
#include <components/misc/osgpluginchecker.hpp>
#include <components/misc/rng.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/platform/platform.hpp>
#include <components/version/version.hpp>
#include <components/vfs/archiveinvalidation.hpp>
#include <components/vfs/archiveorder.hpp>

#include "mwgui/debugwindow.hpp"
#include "mwscript/falloutscripts.hpp"

#include "engine.hpp"
#include "options.hpp"

#include <boost/program_options/variables_map.hpp>

#if defined(_WIN32)
#include <components/misc/windows.hpp>
// makes __argc and __argv available on windows
#include <cstdlib>

extern "C" __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
#endif

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

#if (defined(__APPLE__) || defined(__linux) || defined(__unix) || defined(__posix))
#include <unistd.h>
#endif

/**
 * \brief Parses application command line and calls \ref Cfg::ConfigurationManager
 * to parse configuration files.
 *
 * Results are directly written to \ref Engine class.
 *
 * \retval true - Everything goes OK
 * \retval false - Error
 */
bool parseOptions(int argc, char** argv, OF::Engine& engine, Files::ConfigurationManager& cfgMgr)
{
    // Create a local alias for brevity
    namespace bpo = boost::program_options;
    typedef std::vector<std::string> StringsVector;

    bpo::options_description desc = OpenMW::makeOptionsDescription();
    bpo::variables_map variables;

    Files::parseArgs(argc, argv, variables, desc);
    bpo::notify(variables);

    if (variables.count("help"))
    {
        Debug::getRawStdout() << desc << std::endl;
        return false;
    }

    if (variables.count("version"))
    {
        Debug::getRawStdout() << Version::getOpenmwVersionDescription() << std::endl;
        return false;
    }

    cfgMgr.processPaths(variables, std::filesystem::current_path());

    cfgMgr.readConfiguration(variables, desc);

    Debug::setupLogging(cfgMgr.getLogPath(), "OpenFallout");
    Log(Debug::Info) << Version::getOpenmwVersionDescription();

    Settings::Manager::load(cfgMgr);

    OFGui::DebugWindow::startLogRecording();

    engine.setGrabMouse(!variables["no-grab"].as<bool>());

    // Font encoding settings
    std::string encoding(variables["encoding"].as<std::string>());
    Log(Debug::Info) << ToUTF8::encodingUsingMessage(encoding);
    engine.setEncoding(ToUTF8::calculateEncoding(encoding));

    Files::PathContainer dataDirs(asPathContainer(variables["data"].as<Files::MaybeQuotedPathContainer>()));

    Files::PathContainer::value_type local(variables["data-local"]
                                               .as<Files::MaybeQuotedPathContainer::value_type>()
                                               .u8string()); // This call to u8string is redundant, but required to
                                                             // build on MSVC 14.26 due to implementation bugs.
    if (!local.empty())
        dataDirs.push_back(std::move(local));

    cfgMgr.filterOutNonExistingPaths(dataDirs);

    engine.setResourceDir(variables["resources"]
                              .as<Files::MaybeQuotedPath>()
                              .u8string()); // This call to u8string is redundant, but required to build on MSVC 14.26
                                            // due to implementation bugs.
    engine.setDataDirs(dataDirs);

    const Files::Collections collections(dataDirs);

    // The ini files of the games: the ones in the folder of each game, then the ones that were given
    std::vector<std::filesystem::path> iniPaths = Files::findGameInis(dataDirs);
    for (const auto& path : variables["game-ini"].as<Files::MaybeQuotedPathContainer>())
        iniPaths.push_back(path);
    const Files::GameIni gameIni = Files::GameIni::load(iniPaths);

    const std::string pluginOrder = variables["plugin-order"].as<std::string>();
    if (!pluginOrder.empty() && pluginOrder != "list" && pluginOrder != "time")
    {
        Log(Debug::Error) << "plugin-order must be list or time, not " << pluginOrder << ". Aborting...";
        return false;
    }

    StringsVector content = variables["content"].as<StringsVector>();
    {
        // The content files named on the command line or in the configuration are listed once each. The active
        // plugins of a Plugins.txt that are listed there too are the same plugin.
        std::set<std::string> contentDedupe{ "builtin.omwscripts" };
        for (const auto& contentFile : content)
        {
            if (!contentDedupe.insert(contentFile).second)
            {
                Log(Debug::Error) << "Content file specified more than once: " << contentFile << ". Aborting...";
                return false;
            }
        }
    }

    std::vector<std::string> pluginsFileNames;
    const std::filesystem::path pluginsFile = variables["plugins-file"].as<Files::MaybeQuotedPath>();
    if (!pluginsFile.empty())
    {
        const auto stream = Files::openBinaryInputFileStream(pluginsFile);
        if (!stream->is_open())
        {
            Log(Debug::Error) << "Failed to open the plugins file " << pluginsFile << ". Aborting...";
            return false;
        }
        const std::string text((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
        pluginsFileNames = ESM4::dropMissingPlugins(ESM4::parsePluginsFile(text), collections);
    }

    // Plugins named with content= (or none: then the games' own rules make some active) and the order they load in
    ESM4::ContentOrder contentOrder = ESM4::ContentOrder::List;
    if (std::any_of(content.begin(), content.end(), ESM4::isPluginName))
    {
        for (std::string& name : pluginsFileNames)
            if (std::none_of(content.begin(), content.end(),
                    [&name](const std::string& listed) { return Misc::StringUtils::ciEqual(listed, name); }))
                content.push_back(std::move(name));
    }
    else
    {
        std::vector<std::string> testFiles;
        for (int i = 1; i <= 10; ++i)
            if (const auto name = gameIni.get("General", "sTestFile" + std::to_string(i)); name && !name->empty())
                testFiles.push_back(*name);
        const std::vector<std::string> active = ESM4::activePlugins(testFiles, pluginsFileNames, collections);
        std::string names;
        for (const std::string& name : active)
            names += (names.empty() ? "" : ", ") + name;
        Log(Debug::Info) << "No plugin is named, so the plugins the games make active load: "
                         << (names.empty() ? "none" : names);
        content.insert(content.begin(), active.begin(), active.end());
        // The games have no other order for the files of their Data folder
        contentOrder = ESM4::ContentOrder::FileTime;
    }
    if (pluginOrder == "list")
        contentOrder = ESM4::ContentOrder::List;
    else if (pluginOrder == "time")
        contentOrder = ESM4::ContentOrder::FileTime;

    if (content.empty())
    {
        Log(Debug::Error) << "No content file given (esm/esp, nor omwgame/omwaddon). Aborting...";
        return false;
    }
    engine.addContentFile("builtin.omwscripts");

    // The games load the masters first and a master before the plugins that list it, whatever order they were named in
    content = ESM4::orderContentFiles(content, collections, contentOrder, 1); // builtin.omwscripts comes first

    for (auto& file : content)
    {
        engine.addContentFile(file);
    }

    // The archives the games open, the order they look in them, and loose files against archives (see
    // components/vfs/archiveorder.hpp): the games treat New Vegas, and so Tale of Two Wastelands, and Fallout 3 alike
    // except for the order of the archives
    const VFS::ArchiveRules archiveRules = OFScript::chooseGame(content) == FalloutScript::Game::Fallout3
        ? VFS::ArchiveRules::Fallout3
        : VFS::ArchiveRules::NewVegas;
    const VFS::ArchiveSettings archiveSettings = VFS::ArchiveSettings::read(gameIni, archiveRules);
    const std::vector<std::string> archives = VFS::orderArchives(
        VFS::archivesToOpen(archiveSettings, archiveRules, variables["fallback-archive"].as<StringsVector>(),
            ESM4::pluginArchives(content, collections), collections),
        archiveRules);
    {
        std::string names;
        for (const std::string& archive : archives)
            names += (names.empty() ? "" : ", ") + archive;
        Log(Debug::Info) << "Archive order (the first one that has a file wins): " << (names.empty() ? "none" : names);
    }
    // The engine takes the archives in the order of increasing priority
    for (auto it = archives.rbegin(); it != archives.rend(); ++it)
        engine.addArchive(*it);

    VFS::ArchiveOptions archiveOptions;
    archiveOptions.mArchivesWin = !archiveSettings.mInvalidateOlderFiles;
    if (archiveSettings.mUseArchives && archiveSettings.mInvalidateOlderFiles
        && collections.doesExist(archiveSettings.mInvalidationFile))
    {
        const auto stream = Files::openBinaryInputFileStream(collections.getPath(archiveSettings.mInvalidationFile));
        const std::string text((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
        auto invalidation = std::make_shared<VFS::ArchiveInvalidation>(VFS::ArchiveInvalidation::parse(text));
        Log(Debug::Info) << "Read " << archiveSettings.mInvalidationFile << ", which invalidates files of archives";
        archiveOptions.mInvalidation = std::move(invalidation);
    }
    engine.setArchiveOptions(std::move(archiveOptions));

    StringsVector groundcover = variables["groundcover"].as<StringsVector>();
    for (auto& file : groundcover)
    {
        engine.addGroundcoverFile(file);
    }

    if (variables.count("lua-scripts"))
    {
        Log(Debug::Warning) << "Lua scripts have been specified via the old lua-scripts option and will not be loaded. "
                               "Please update them to a version which uses the new omwscripts format.";
    }

    // startup-settings
    engine.setCell(variables["start"].as<std::string>());
    engine.setSkipMenu(variables["skip-menu"].as<bool>(), variables["new-game"].as<bool>());
    if (!variables["skip-menu"].as<bool>() && variables["new-game"].as<bool>())
        Log(Debug::Warning) << "Warning: new-game used without skip-menu -> ignoring it";

    // scripts
    engine.setCompileAll(variables["script-all"].as<bool>());
    engine.setCompileAllDialogue(variables["script-all-dialogue"].as<bool>());
    engine.setScriptConsoleMode(variables["script-console"].as<bool>());
    engine.setStartupScript(variables["script-run"].as<std::string>());
    engine.setWarningsMode(variables["script-warn"].as<int>());
    engine.setSaveGameFile(variables["load-savegame"].as<Files::MaybeQuotedPath>().u8string());

    // other settings
    Fallback::Map::init(variables["fallback"].as<Fallback::FallbackMap>().mMap);
    engine.setSoundUsage(!variables["no-sound"].as<bool>());
    engine.setActivationDistanceOverride(variables["activate-dist"].as<int>());
    engine.enableFontExport(variables["export-fonts"].as<bool>());
    engine.setRandomSeed(variables["random-seed"].as<unsigned int>());

    return true;
}

namespace
{
    class OSGLogHandler : public osg::NotifyHandler
    {
        void notify(osg::NotifySeverity severity, const char* msg) override
        {
            // Copy, because osg logging is not thread safe.
            std::string msgCopy(msg);
            if (msgCopy.empty())
                return;

            Debug::Level level;
            switch (severity)
            {
                case osg::ALWAYS:
                case osg::FATAL:
                    level = Debug::Error;
                    break;
                case osg::WARN:
                case osg::NOTICE:
                    level = Debug::Warning;
                    break;
                case osg::INFO:
                    level = Debug::Info;
                    break;
                case osg::DEBUG_INFO:
                case osg::DEBUG_FP:
                default:
                    level = Debug::Debug;
            }
            std::string_view s(msgCopy);
            if (s.size() < 1024)
                Log(level) << (s.back() == '\n' ? s.substr(0, s.size() - 1) : s);
            else
            {
                while (!s.empty())
                {
                    size_t lineSize = 1;
                    while (lineSize < s.size() && s[lineSize - 1] != '\n')
                        lineSize++;
                    Log(level) << s.substr(0, s[lineSize - 1] == '\n' ? lineSize - 1 : lineSize);
                    s = s.substr(lineSize);
                }
            }
        }
    };
}

int runApplication(int argc, char* argv[])
{
    Platform::init();

#ifdef __APPLE__
    setenv("OSG_GL_TEXTURE_STORAGE", "OFF", 0);
#endif

    osg::setNotifyHandler(new OSGLogHandler());
    Files::ConfigurationManager cfgMgr;
    std::unique_ptr<OF::Engine> engine = std::make_unique<OF::Engine>(cfgMgr);

    engine->setRecastMaxLogLevel(Debug::getRecastMaxLogLevel());

    if (parseOptions(argc, argv, *engine, cfgMgr))
    {
        if (!Misc::checkRequiredOSGPluginsArePresent())
            return 1;

        engine->go();
    }

    return 0;
}

#ifdef ANDROID
extern "C" int SDL_main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
    return Debug::wrapApplication(&runApplication, argc, argv, "OpenFallout");
}

// Platform specific for Windows when there is no console built into the executable.
// Windows will call the WinMain function instead of main in this case, the normal
// main function is then called with the __argc and __argv parameters.
#if defined(_WIN32) && !defined(_CONSOLE)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    return main(__argc, __argv);
}
#endif

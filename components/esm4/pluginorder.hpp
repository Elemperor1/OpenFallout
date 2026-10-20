#ifndef OPENFALLOUT_COMPONENTS_ESM4_PLUGINORDER_H
#define OPENFALLOUT_COMPONENTS_ESM4_PLUGINORDER_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Files
{
    class Collections;
}

namespace ESM4
{
    // The order the games load plugins in, and the file that lists the plugins to load.
    //
    // A form ID of a plugin names the plugin by the position of one of its masters, so a plugin only loads when every
    // master it lists is loaded before it. The games also load every plugin flagged as a master (the `.esm` files)
    // before every plugin that is not, whatever the order of the list; within each of the two groups the order of the
    // list stays.

    /// Whether the name of a file is that of a plugin of the games: it ends in .esm, .esp or .esl
    bool isPluginName(std::string_view name);

    /// What the order needs to know of a plugin: the name it is listed by, whether its header flags it as a master and
    /// the names of its masters, in the order the header lists them, each with the size in bytes the header says the
    /// master had when the plugin was made (0 where it says none).
    struct PluginInfo
    {
        std::string mName;
        bool mMaster = false;
        std::vector<std::string> mMasters;
        std::vector<std::uint64_t> mMasterSizes;
    };

    /// Reads the header of a plugin: nothing if the file is not in the format of Fallout and Oblivion (a plugin of
    /// Morrowind is not). Throws std::runtime_error if the file cannot be read or its header is damaged.
    std::optional<PluginInfo> readPluginInfo(const std::filesystem::path& path, std::string name);

    /// The plugins in the order they load in
    struct PluginOrder
    {
        /// The plugins as indices of the list that was given, in load order
        std::vector<std::size_t> mOrder;

        /// One line for each plugin that is not where the list had it, saying why, for the log
        std::vector<std::string> mMoves;
    };

    /// The most plugins that can load together: the top byte of a form ID names the plugin, and 0xFF is the byte of
    /// the objects that the game makes while it runs.
    constexpr std::size_t sMaxPlugins = 0xFF;

    /// Puts the plugins, listed in the order the user gave, in the order the games load them. Throws
    /// std::runtime_error, naming every plugin and master involved, if a plugin is listed twice, a master of a listed
    /// plugin is not listed (\a missingMaster ends the sentence that says so), masters depend on each other in a
    /// circle, or there are more than sMaxPlugins plugins.
    PluginOrder orderPlugins(const std::vector<PluginInfo>& plugins,
        std::string_view missingMaster = "which is not among the plugins to load");

    /// Where the order of the plugins within the masters and within the others comes from.
    enum class ContentOrder
    {
        /// The order of the list. This is how mod managers hand a load order over.
        List,
        /// The time each file was last written, oldest first: the rule the games apply to the files of their Data
        /// folder, and the only one they have. Files written at the same time keep the order of the list.
        FileTime,
    };

    /// The content files of a list in the order the games load them. The plugins (the files in the format of Fallout
    /// and Oblivion, found in the collections) take each other's places in the list, in the order orderPlugins gives
    /// after \a order has been applied; all other content stays where it is. As in the games, a master of a listed
    /// plugin that the list lacks is added when the collections have it (after the last plugin of the list). The log
    /// says what moved or was added. Throws std::runtime_error if a content file is not found or the plugins cannot be
    /// ordered, which includes a master that neither the list nor the collections have.
    std::vector<std::string> orderContentFiles(const std::vector<std::string>& content,
        const Files::Collections& collections, ContentOrder order = ContentOrder::List);

    /// The names the collections have a plugin for, in the order given. The games ignore a line of a Plugins.txt that
    /// names a file that is not in their Data folder, and so does this; the log names each one.
    std::vector<std::string> dropMissingPlugins(
        const std::vector<std::string>& names, const Files::Collections& collections);

    /// The plugins that the games make active when they are asked nothing: the ones the ini file names (the values of
    /// sTestFile1 to sTestFile10 of its [General] section, \a testFiles here; where it names none, FalloutNV.esm, or
    /// Fallout3.esm where there is no such file), the ones a Plugins.txt names (\a pluginsFile, names that are not
    /// there are skipped) and every plugin that has a file of the same name with the extension .NAM next to it. The
    /// games activate the masters of these as well; orderContentFiles does that. Each plugin is named once, as the
    /// list that has it spells it, in that order.
    std::vector<std::string> activePlugins(const std::vector<std::string>& testFiles,
        const std::vector<std::string>& pluginsFile, const Files::Collections& collections);

    /// One line for each master a plugin that is not a master was made for in another version than the one at hand:
    /// the games warn of these and load on. \a sizeOf gives the size of the file of a master, nothing if it has none.
    std::vector<std::string> masterSizeWarnings(const std::vector<PluginInfo>& plugins,
        const std::function<std::optional<std::uint64_t>(const std::string&)>& sizeOf);

    /// The names of the active plugins in the text of a Plugins.txt: one name to a line, blank lines and lines that
    /// start with # ignored. The games' own file lists the active plugins only; the newer format marks them with a
    /// leading * and lists the inactive ones too, and where any line has the mark only the marked ones are active.
    std::vector<std::string> parsePluginsFile(std::string_view text);
}

#endif

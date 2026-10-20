#ifndef OPENFALLOUT_COMPONENTS_ESM4_PLUGINORDER_H
#define OPENFALLOUT_COMPONENTS_ESM4_PLUGINORDER_H

#include <cstddef>
#include <filesystem>
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

    /// What the order needs to know of a plugin: the name it is listed by, whether its header flags it as a master and
    /// the names of its masters, in the order the header lists them.
    struct PluginInfo
    {
        std::string mName;
        bool mMaster = false;
        std::vector<std::string> mMasters;
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
    /// plugin is not listed, masters depend on each other in a circle, or there are more than sMaxPlugins plugins.
    PluginOrder orderPlugins(const std::vector<PluginInfo>& plugins);

    /// The content files of a list in the order the games load them. The plugins (the files in the format of Fallout
    /// and Oblivion, found in the collections) take each other's places in the list, in the order orderPlugins gives;
    /// all other content stays where it is. The log says what moved. Throws std::runtime_error if a content file is not
    /// found or the plugins cannot be ordered.
    std::vector<std::string> orderContentFiles(
        const std::vector<std::string>& content, const Files::Collections& collections);

    /// The names of the active plugins in the text of a Plugins.txt: one name to a line, blank lines and lines that
    /// start with # ignored. The games' own file lists the active plugins only; the newer format marks them with a
    /// leading * and lists the inactive ones too, and where any line has the mark only the marked ones are active.
    std::vector<std::string> parsePluginsFile(std::string_view text);
}

#endif

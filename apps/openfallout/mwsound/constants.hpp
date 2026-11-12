#ifndef OPENFALLOUT_APPS_OPENFALLOUT_MWSOUND_CONSTANTS_H
#define OPENFALLOUT_APPS_OPENFALLOUT_MWSOUND_CONSTANTS_H

#include <components/vfs/pathutil.hpp>

namespace OFSound
{
    constexpr VFS::Path::NormalizedView battlePlaylist("battle");
    constexpr VFS::Path::NormalizedView explorePlaylist("explore");
    // Data/Music/Special/MainTitle.mp3, the title music of both games (FalloutNV.exe 0x1076F3C)
    constexpr VFS::Path::NormalizedView titleMusic("music/special/maintitle.mp3");
    constexpr VFS::Path::NormalizedView triumphMusic("music/special/mw_triumph.mp3");
    constexpr VFS::Path::NormalizedView deathMusic("music/special/mw_death.mp3");
}

#endif

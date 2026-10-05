#ifndef OPENFALLOUT_APPS_OPENFALLOUT_MWSOUND_CONSTANTS_H
#define OPENFALLOUT_APPS_OPENFALLOUT_MWSOUND_CONSTANTS_H

#include <components/vfs/pathutil.hpp>

namespace MWSound
{
    constexpr VFS::Path::NormalizedView battlePlaylist("battle");
    constexpr VFS::Path::NormalizedView explorePlaylist("explore");
    constexpr VFS::Path::NormalizedView titleMusic("music/special/morrowind title.mp3");
    constexpr VFS::Path::NormalizedView triumphMusic("music/special/mw_triumph.mp3");
    constexpr VFS::Path::NormalizedView deathMusic("music/special/mw_death.mp3");
}

#endif

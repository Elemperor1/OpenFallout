#ifndef OPENFALLOUT_APPS_OPENFALLOUT_MWRENDER_ACTORUTIL_H
#define OPENFALLOUT_APPS_OPENFALLOUT_MWRENDER_ACTORUTIL_H

#include <components/vfs/pathutil.hpp>

#include <string>

namespace OFRender
{
    const std::string& getActorSkeleton(bool firstPerson, bool female, bool beast, bool werewolf);
    bool isDefaultActorSkeleton(VFS::Path::NormalizedView model);
    std::string addSuffixBeforeExtension(const std::string& filename, const std::string& suffix);
}

#endif

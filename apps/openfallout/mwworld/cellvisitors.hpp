#ifndef GAME_MWWORLD_CELLVISITORS_H
#define GAME_MWWORLD_CELLVISITORS_H

#include <string>
#include <vector>

#include "ptr.hpp"

namespace OFWorld
{
    struct ListAndResetObjectsVisitor
    {
        std::vector<OFWorld::Ptr> mObjects;

        bool operator()(const OFWorld::Ptr& ptr)
        {
            if (ptr.getRefData().getBaseNode())
            {
                ptr.getRefData().setBaseNode(nullptr);
            }
            mObjects.push_back(ptr);

            return true;
        }
    };

}

#endif

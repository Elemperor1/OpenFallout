#ifndef OPENFALLOUT_MWRENDER_AGENTSPATHS_H
#define OPENFALLOUT_MWRENDER_AGENTSPATHS_H

#include "apps/openfallout/mwworld/ptr.hpp"

#include <osg/ref_ptr>

#include <deque>
#include <map>

namespace osg
{
    class Group;
    class StateSet;
}

namespace DetourNavigator
{
    struct Settings;
    struct AgentBounds;
}

namespace OFRender
{
    class ActorsPaths
    {
    public:
        ActorsPaths(const osg::ref_ptr<osg::Group>& root, bool enabled);
        ~ActorsPaths();

        bool toggle();

        void update(const OFWorld::ConstPtr& actor, const std::deque<osg::Vec3f>& path,
            const DetourNavigator::AgentBounds& agentBounds, const osg::Vec3f& start, const osg::Vec3f& end,
            const DetourNavigator::Settings& settings);

        void remove(const OFWorld::ConstPtr& actor);

        void removeCell(const OFWorld::CellStore* const store);

        void updatePtr(const OFWorld::ConstPtr& old, const OFWorld::ConstPtr& updated);

        void enable();

        void disable();

    private:
        struct Group
        {
            const OFWorld::CellStore* mCell;
            osg::ref_ptr<osg::Group> mNode;
        };

        using Groups = std::map<const OFWorld::LiveCellRefBase*, Group>;

        osg::ref_ptr<osg::Group> mRootNode;
        Groups mGroups;
        bool mEnabled;
        osg::ref_ptr<osg::StateSet> mGroupStateSet;
        osg::ref_ptr<osg::StateSet> mDebugDrawStateSet;
    };
}

#endif

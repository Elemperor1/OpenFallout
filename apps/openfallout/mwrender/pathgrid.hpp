#ifndef GAME_RENDER_MWSCENE_H
#define GAME_RENDER_MWSCENE_H

#include <utility>

#include <map>
#include <vector>

#include <osg/ref_ptr>

namespace ESM
{
    struct Pathgrid;
}

namespace osg
{
    class Group;
    class Geometry;
}

namespace OFWorld
{
    class Ptr;
    class CellStore;
}

namespace OFRender
{
    class Pathgrid
    {
        bool mPathgridEnabled;

        void togglePathgrid();

        typedef std::vector<const OFWorld::CellStore*> CellList;
        CellList mActiveCells;

        osg::ref_ptr<osg::Group> mRootNode;

        osg::ref_ptr<osg::Group> mPathGridRoot;

        typedef std::map<std::pair<int, int>, osg::ref_ptr<osg::Group>> ExteriorPathgridNodes;
        ExteriorPathgridNodes mExteriorPathgridNodes;
        osg::ref_ptr<osg::Group> mInteriorPathgridNode;

        void enableCellPathgrid(const OFWorld::CellStore* store);
        void disableCellPathgrid(const OFWorld::CellStore* store);

    public:
        Pathgrid(osg::ref_ptr<osg::Group> root);
        ~Pathgrid();
        bool toggleRenderMode(int mode);

        void addCell(const OFWorld::CellStore* store);
        void removeCell(const OFWorld::CellStore* store);
    };

}

#endif

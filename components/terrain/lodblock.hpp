#ifndef OPENFALLOUT_COMPONENTS_TERRAIN_LODBLOCK_H
#define OPENFALLOUT_COMPONENTS_TERRAIN_LODBLOCK_H

#include <osg/Node>
#include <osg/Vec3f>
#include <osg/ref_ptr>

#include <array>
#include <cstdint>

namespace Terrain
{
    /// A block of distant objects covers four by four cells. This sorts the triangles below the node into the
    /// cells by the cell their center is in, so that the cells that are loaded can drop their part of the block.
    /// The geometry stays shared with the node. @param firstCellX, firstCellY the cell at the south west corner of
    /// the block. @param cellSize the side of a cell in game units. @return the part of each cell, row by row from
    /// the south west corner, and null for a cell without triangles or one that is not in the bits of `wanted`
    /// (bit y * 4 + x for the cell x, y of the block, counted from its south west cell).
    std::array<osg::ref_ptr<osg::Node>, 16> splitBlockByCell(
        osg::Node& block, int firstCellX, int firstCellY, float cellSize, std::uint16_t wanted = 0xffff);

    /// The tiles and the blocks of the games are placed by the translation of their root node, to the south west
    /// corner of the cells they cover, and their vertices run from zero. The loader of NIF files drops the translation
    /// of a root node. @param cells the side of the square the file covers in cells. @return the move that puts the
    /// node, as it was loaded, where the cell x, y has its south west corner; none if its geometry is in the world
    /// already, which the middle of the geometry tells (the two squares only touch, so a block with geometry in a part
    /// of its square is told as well as a full one).
    osg::Vec3f getLodPlacement(const osg::Node& node, int x, int y, int cells, float cellSize);
}

#endif

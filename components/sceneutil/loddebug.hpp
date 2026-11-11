#ifndef OPENFALLOUT_COMPONENTS_SCENEUTIL_LODDEBUG_H
#define OPENFALLOUT_COMPONENTS_SCENEUTIL_LODDEBUG_H

#include <string>
#include <string_view>
#include <vector>

namespace osg
{
    class Node;
}

namespace SceneUtil
{
    /// Ways to look into how the distant land of the games is drawn, asked for by the setting `game lod debug`: words
    /// separated by spaces or commas.
    struct LodDebug
    {
        /// "flat": no textures, and a colour for what the file is (red, green, blue and yellow for the tiles of
        /// 4, 8, 16 and 32 cells, magenta for a block of objects and cyan for a block of high detail)
        bool mFlat = false;
        /// "wireframe": only the edges of the triangles
        bool mWireframe = false;
        /// "no-skirt": leaves out the triangles that hang under the edge of a tile
        bool mNoSkirt = false;
        /// "objects-only": draws the blocks of objects and nothing of the land
        bool mObjectsOnly = false;
        /// "no-high": leaves out the blocks of high detail
        bool mNoHigh = false;
        /// "no-land": draws no land at all, not the tiles and not the land of the records, whatever the size of the
        /// node, so the loaded cells have none either
        bool mNoLand = false;
        /// "no-records": draws the tiles but not the land of the records, whatever the size of the node, so the loaded
        /// cells have none and the places no tile covers are empty
        bool mNoRecords = false;
        /// "nodes": writes a line to the log, once, for each node of the quad tree within 9 cells of the eye (at most
        /// 1,500) when it is made: its size and centre in cells and what each chunk manager made for it, to tell which
        /// of them draws a place
        bool mNodes = false;
        /// "fine-land": the land of the records is made with the most vertices that its chunks have (the chunk of
        /// 2 cells with as many per cell as the one of half a cell), to tell if what is drawn over it floats because
        /// the land is too coarse
        bool mFineLand = false;
        /// "no-cluster": the land of the records is not left out when its chunk faces away from the eye
        bool mNoCluster = false;

        bool changesFiles() const { return mFlat || mWireframe || mNoSkirt; }
    };

    /// The words that are in the text. The ones that are none of the above are added to `unknown`, if it is given.
    LodDebug parseLodDebug(std::string_view words, std::vector<std::string>* unknown = nullptr);

    /// What a file of the distant land gets before it is made ready to draw. `path` tells the kind of file, from its
    /// name (see the words above). `skirtHeight` is the drop above which a triangle is a skirt.
    void applyLodDebug(osg::Node& node, std::string_view path, const LodDebug& debug, float skirtHeight);
}

#endif

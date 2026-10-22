#ifndef OPENFALLOUT_COMPONENTS_RESOURCE_SPEEDTREE_H
#define OPENFALLOUT_COMPONENTS_RESOURCE_SPEEDTREE_H

#include <string>

#include <osg/Geometry>
#include <osg/Node>
#include <osg/ref_ptr>

namespace Resource
{
    class ImageManager;

    /// The trees of Fallout 3 and New Vegas are SpeedTree files (.spt), which are not read. A tree is drawn from its
    /// record instead: as a few cards that cross at the trunk, with the image that the record has for the tree.
    struct SpeedTreeStandIn
    {
        /// ICON: the image of the tree, with an alpha channel that is its shape
        std::string mTexture;
        /// BNAM: the size of the billboard of the tree in game units (0 when the record has none)
        float mWidth = 0.f;
        float mHeight = 0.f;
    };

    /// The size of a card when the record has none
    constexpr float sSpeedTreeDefaultSize = 512.f;
    /// The number of cards that cross at the trunk
    constexpr unsigned int sSpeedTreeCards = 3;

    /// `cards` upright quads of the given width and height that stand on z = 0, cross at the origin and are turned
    /// evenly about the vertical axis. The coordinates of the image run from its top (v = 0) to its bottom, as in a nif.
    osg::ref_ptr<osg::Geometry> makeCrossedCards(float width, float height, unsigned int cards);

    /// The cards of a tree with the image of its record, which is alpha tested and drawn on both sides. A node
    /// without anything to draw when the image cannot be found.
    osg::ref_ptr<osg::Node> makeSpeedTreeStandIn(const SpeedTreeStandIn& tree, ImageManager& imageManager);
}

#endif

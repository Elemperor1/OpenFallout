#include "speedtree.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include <osg/AlphaFunc>
#include <osg/Group>
#include <osg/Material>
#include <osg/Texture2D>

#include <components/debug/debuglog.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/sceneutil/texturetype.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/pathutil.hpp>

#include "imagemanager.hpp"

namespace Resource
{
    VFS::Path::Normalized findSpeedTreeImage(std::string_view icon, const VFS::Manager& vfs)
    {
        const VFS::Path::Normalized name(icon);
        if (name.value().find(VFS::Path::separator) == std::string::npos)
        {
            for (const std::string_view folder : { "textures/trees/leaves", "textures/trees/billboards" })
            {
                VFS::Path::Normalized candidate(folder);
                candidate /= name.value();
                if (vfs.exists(candidate))
                    return candidate;
            }
        }
        return Misc::ResourceHelpers::correctTexturePath(name, vfs);
    }

    SpeedTreeClusters getSpeedTreeClusters(int imageWidth, int imageHeight)
    {
        return { .mColumns = std::max(1, imageWidth / sSpeedTreeClusterSize),
            .mRows = std::max(1, imageHeight / sSpeedTreeClusterSize) };
    }

    osg::ref_ptr<osg::Geometry> makeCrossedCards(
        float width, float height, unsigned int cards, SpeedTreeClusters clusters)
    {
        clusters.mColumns = std::max(1, clusters.mColumns);
        clusters.mRows = std::max(1, clusters.mRows);
        const auto clusterCount = static_cast<unsigned int>(clusters.mColumns * clusters.mRows);

        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array;
        osg::ref_ptr<osg::Vec2Array> coordinates = new osg::Vec2Array;
        osg::ref_ptr<osg::DrawElementsUShort> triangles = new osg::DrawElementsUShort(GL_TRIANGLES);

        for (unsigned int card = 0; card < cards; ++card)
        {
            const float angle = std::numbers::pi_v<float> * static_cast<float>(card) / static_cast<float>(cards);
            const osg::Vec3f along(std::cos(angle) * width * 0.5f, std::sin(angle) * width * 0.5f, 0.f);
            const auto first = static_cast<unsigned short>(vertices->size());

            vertices->push_back(-along);
            vertices->push_back(along);
            vertices->push_back(along + osg::Vec3f(0.f, 0.f, height));
            vertices->push_back(-along + osg::Vec3f(0.f, 0.f, height));
            const auto cluster = static_cast<int>(card % clusterCount);
            const float left = static_cast<float>(cluster % clusters.mColumns) / static_cast<float>(clusters.mColumns);
            const float right = left + 1.f / static_cast<float>(clusters.mColumns);
            const float top = static_cast<float>(cluster / clusters.mColumns) / static_cast<float>(clusters.mRows);
            const float bottom = top + 1.f / static_cast<float>(clusters.mRows);
            coordinates->push_back(osg::Vec2f(left, bottom));
            coordinates->push_back(osg::Vec2f(right, bottom));
            coordinates->push_back(osg::Vec2f(right, top));
            coordinates->push_back(osg::Vec2f(left, top));
            // lit as the crown of a tree is, from above, whichever side of the card is seen
            for (int i = 0; i < 4; ++i)
                normals->push_back(osg::Vec3f(0.f, 0.f, 1.f));

            for (unsigned short index : { 0, 1, 2, 0, 2, 3 })
                triangles->push_back(first + index);
        }

        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        geometry->setVertexArray(vertices);
        geometry->setNormalArray(normals, osg::Array::BIND_PER_VERTEX);
        geometry->setTexCoordArray(0, coordinates, osg::Array::BIND_PER_VERTEX);
        geometry->addPrimitiveSet(triangles);
        geometry->setUseDisplayList(false);
        geometry->setUseVertexBufferObjects(true);
        return geometry;
    }

    osg::ref_ptr<osg::Node> makeSpeedTreeStandIn(const SpeedTreeStandIn& tree, ImageManager& imageManager)
    {
        osg::ref_ptr<osg::Group> node = new osg::Group;
        if (tree.mTexture.empty())
            return node;

        osg::ref_ptr<osg::Image> image;
        try
        {
            image = imageManager.getImage(findSpeedTreeImage(tree.mTexture, *imageManager.getVFS()));
        }
        catch (const std::exception& error)
        {
            Log(Debug::Warning) << "The image '" << tree.mTexture << "' of a tree cannot be read: " << error.what();
        }
        if (image == nullptr)
            return node;

        const float width = tree.mWidth > 0.f ? tree.mWidth : sSpeedTreeDefaultSize;
        const float height = tree.mHeight > 0.f ? tree.mHeight : sSpeedTreeDefaultSize;
        osg::ref_ptr<osg::Geometry> geometry
            = makeCrossedCards(width, height, sSpeedTreeCards, getSpeedTreeClusters(image->s(), image->t()));

        osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D(image);
        texture->setTextureSize(image->s(), image->t());
        texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

        osg::ref_ptr<osg::StateSet> stateset = geometry->getOrCreateStateSet();
        stateset->setTextureAttributeAndModes(0, texture, osg::StateAttribute::ON);
        stateset->setTextureAttribute(0, new SceneUtil::TextureType("diffuseMap"), osg::StateAttribute::ON);
        stateset->setAttributeAndModes(new osg::AlphaFunc(osg::AlphaFunc::GREATER, 0.5f), osg::StateAttribute::ON);
        stateset->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
        osg::ref_ptr<osg::Material> material = new osg::Material;
        material->setColorMode(osg::Material::OFF);
        material->setAmbient(
            osg::Material::FRONT_AND_BACK, osg::Vec4f(sSpeedTreeLight, sSpeedTreeLight, sSpeedTreeLight, 1.f));
        material->setDiffuse(
            osg::Material::FRONT_AND_BACK, osg::Vec4f(sSpeedTreeLight, sSpeedTreeLight, sSpeedTreeLight, 1.f));
        material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4f(0.f, 0.f, 0.f, 0.f));
        material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4f(0.f, 0.f, 0.f, 0.f));
        stateset->setAttributeAndModes(material, osg::StateAttribute::ON);

        node->addChild(geometry);
        return node;
    }
}

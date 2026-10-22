#include "speedtree.hpp"

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
#include <components/vfs/pathutil.hpp>

#include "imagemanager.hpp"

namespace Resource
{
    osg::ref_ptr<osg::Geometry> makeCrossedCards(float width, float height, unsigned int cards)
    {
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
            coordinates->push_back(osg::Vec2f(0.f, 1.f));
            coordinates->push_back(osg::Vec2f(1.f, 1.f));
            coordinates->push_back(osg::Vec2f(1.f, 0.f));
            coordinates->push_back(osg::Vec2f(0.f, 0.f));
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
            image = imageManager.getImage(
                Misc::ResourceHelpers::correctTexturePath(VFS::Path::Normalized(tree.mTexture), *imageManager.getVFS()));
        }
        catch (const std::exception& error)
        {
            Log(Debug::Warning) << "The image '" << tree.mTexture << "' of a tree cannot be read: " << error.what();
        }
        if (image == nullptr)
            return node;

        const float width = tree.mWidth > 0.f ? tree.mWidth : sSpeedTreeDefaultSize;
        const float height = tree.mHeight > 0.f ? tree.mHeight : sSpeedTreeDefaultSize;
        osg::ref_ptr<osg::Geometry> geometry = makeCrossedCards(width, height, sSpeedTreeCards);

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
        material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4f(1.f, 1.f, 1.f, 1.f));
        material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4f(1.f, 1.f, 1.f, 1.f));
        material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4f(0.f, 0.f, 0.f, 0.f));
        material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4f(0.f, 0.f, 0.f, 0.f));
        stateset->setAttributeAndModes(material, osg::StateAttribute::ON);

        node->addChild(geometry);
        return node;
    }
}

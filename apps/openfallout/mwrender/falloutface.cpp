#include "falloutface.hpp"

#include <algorithm>
#include <iterator>
#include <map>
#include <mutex>
#include <string>

#include <osg/Geometry>
#include <osg/Texture2D>
#include <osg/observer_ptr>

#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

#include <components/debug/debuglog.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/sceneutil/material.hpp>
#include <components/sceneutil/riggeometry.hpp>
#include <components/sceneutil/util.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/pathutil.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

namespace OFRender
{
    namespace
    {
        // A mesh of a part: a plain geometry, or the source of a skinned one
        struct Mesh
        {
            osg::Geometry* mGeometry = nullptr; // null for a skinned mesh
            osg::Group* mParent = nullptr; // the node of the part that has the plain geometry
            SceneUtil::RigGeometry* mRig = nullptr;
            std::size_t mVertices = 0;
        };

        class CollectMeshes : public osg::NodeVisitor
        {
        public:
            CollectMeshes()
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
            {
            }

            void apply(osg::Drawable& drawable) override
            {
                if (SceneUtil::RigGeometry* rig = dynamic_cast<SceneUtil::RigGeometry*>(&drawable))
                {
                    const osg::ref_ptr<osg::Geometry> source = rig->getSourceGeometry();
                    if (source != nullptr && dynamic_cast<const osg::Vec3Array*>(source->getVertexArray()) != nullptr)
                        mMeshes.push_back({ nullptr, nullptr, rig, source->getVertexArray()->getNumElements() });
                    return;
                }
                osg::Geometry* geometry = drawable.asGeometry();
                const osg::NodePath& path = getNodePath();
                if (geometry != nullptr && dynamic_cast<const osg::Vec3Array*>(geometry->getVertexArray()) != nullptr
                    && path.size() >= 2 && path[path.size() - 2]->asGroup() != nullptr)
                    mMeshes.push_back({ geometry, path[path.size() - 2]->asGroup(), nullptr,
                        geometry->getVertexArray()->getNumElements() });
            }

            std::vector<Mesh> mMeshes;
        };

        // A mesh that has its own arrays, so that the vertices can change without changing those of other characters
        osg::ref_ptr<osg::Geometry> copyGeometry(const osg::Geometry& geometry)
        {
            return new osg::Geometry(geometry, osg::CopyOp::DEEP_COPY_ARRAYS | osg::CopyOp::DEEP_COPY_PRIMITIVES);
        }

        class ReplaceDiffuseMap : public osg::NodeVisitor
        {
        public:
            explicit ReplaceDiffuseMap(const DiffuseChooser& choose)
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
                , mChoose(choose)
            {
            }

            void apply(osg::Node& node) override
            {
                replace(node);
                traverse(node);
            }

            void apply(osg::Drawable& drawable) override
            {
                replace(drawable);
                traverse(drawable);
            }

            std::size_t mChanged = 0;

        private:
            void replace(osg::Node& node)
            {
                osg::StateSet* stateset = node.getStateSet();
                if (stateset == nullptr)
                    return;
                const unsigned int units = static_cast<unsigned int>(stateset->getTextureAttributeList().size());
                for (unsigned int unit = 0; unit < units; ++unit)
                {
                    const osg::Texture2D* texture = dynamic_cast<const osg::Texture2D*>(
                        stateset->getTextureAttribute(unit, osg::StateAttribute::TEXTURE));
                    if (texture == nullptr)
                        continue;
                    // As the shader visitor does, take the first texture to be the diffuse map when it has no name
                    const std::string& type = SceneUtil::getTextureType(*stateset, *texture, unit);
                    if (type != "diffuseMap" && !(type.empty() && unit == 0))
                        continue;

                    const osg::ref_ptr<osg::Image> image = mChoose(texture->getImage());
                    if (image == nullptr)
                        return;

                    // The state set and the texture are those of every character with the same part
                    osg::ref_ptr<osg::StateSet> own = new osg::StateSet(*stateset, osg::CopyOp::SHALLOW_COPY);
                    osg::ref_ptr<osg::Texture2D> replacement = new osg::Texture2D(*texture, osg::CopyOp::SHALLOW_COPY);
                    replacement->setImage(image);
                    replacement->setTextureSize(image->s(), image->t());
                    // The format of the new picture is its own (the old one may have been compressed)
                    replacement->setInternalFormatMode(osg::Texture::USE_IMAGE_DATA_FORMAT);
                    own->setTextureAttribute(unit, replacement, osg::StateAttribute::ON);
                    node.setStateSet(own);
                    ++mChanged;
                    return;
                }
            }

            DiffuseChooser mChoose;
        };

        std::mutex sCacheMutex;
        std::map<std::string, osg::observer_ptr<osg::Image>> sTintedSkins;
        std::map<std::string, std::shared_ptr<const ESM4::FaceMorphs>> sMorphCache;

        std::unique_ptr<ESM4::FaceTextureIndex> sTextures;
    }

    std::size_t morphFaceMeshes(osg::Node& part, const ESM4::FaceMorphs& morphs, const std::vector<float>& symmetric,
        const std::vector<float>& asymmetric, bool firstOfLongerFile)
    {
        CollectMeshes collect;
        part.accept(collect);

        // Where the vertices of each mesh start in the file: all start at the first when a mesh has as many vertices
        // as the file, or one after the other when together they have
        std::vector<std::pair<Mesh*, std::size_t>> moves;
        for (Mesh& mesh : collect.mMeshes)
            if (mesh.mVertices == morphs.mVertexCount)
                moves.emplace_back(&mesh, 0);
        if (moves.empty())
        {
            std::size_t total = 0;
            for (const Mesh& mesh : collect.mMeshes)
                total += mesh.mVertices;
            if (firstOfLongerFile && collect.mMeshes.size() == 1 && total < morphs.mVertexCount)
                moves.emplace_back(&collect.mMeshes.front(), 0);
            else if (total != morphs.mVertexCount)
                return 0;
            else
            {
                std::size_t first = 0;
                for (Mesh& mesh : collect.mMeshes)
                {
                    moves.emplace_back(&mesh, first);
                    first += mesh.mVertices;
                }
            }
        }

        std::size_t moved = 0;
        for (const auto& [mesh, first] : moves)
        {
            const osg::Geometry& original = mesh->mRig != nullptr ? *mesh->mRig->getSourceGeometry() : *mesh->mGeometry;
            osg::ref_ptr<osg::Geometry> copy = copyGeometry(original);
            osg::Vec3Array* positions = static_cast<osg::Vec3Array*>(copy->getVertexArray());
            ESM4::applyFaceMorphs(morphs, symmetric, asymmetric, &positions->front().x(), mesh->mVertices, first);
            positions->dirty();
            // The copy has the bounds of the mesh it was made from, which are where the vertices were
            copy->dirtyBound();

            if (mesh->mRig != nullptr)
            {
                // The bounds of a skinned mesh are those of the vertices that each bone moves, from the shared data of
                // the mesh before the face: a vertex that moved out of them would not be drawn at the edge of the view
                const osg::Vec3Array& before = static_cast<const osg::Vec3Array&>(*original.getVertexArray());
                float farthest = 0.f;
                for (std::size_t i = 0; i < mesh->mVertices; ++i)
                    farthest = std::max(farthest, ((*positions)[i] - before[i]).length());
                mesh->mRig->expandBounds(farthest);
                mesh->mRig->setSourceGeometry(copy);
            }
            else
                mesh->mParent->replaceChild(mesh->mGeometry, copy);
            ++moved;
        }
        return moved;
    }

    std::size_t replaceDiffuseMap(osg::Node& part, osg::Image* image)
    {
        if (image == nullptr)
            return 0;
        const osg::ref_ptr<osg::Image> keep(image);
        return replaceDiffuseMap(part, [keep](const osg::Image*) { return keep; });
    }

    std::size_t replaceDiffuseMap(osg::Node& part, const DiffuseChooser& choose)
    {
        if (!choose)
            return 0;
        ReplaceDiffuseMap replace(choose);
        part.accept(replace);
        return replace.mChanged;
    }

    namespace
    {
        bool decodeImage(const osg::Image& image, ESM4::SkinImage& skin)
        {
            ESM4::SkinPixelFormat format;
            switch (image.getPixelFormat())
            {
                case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
                case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                    format = ESM4::SkinPixelFormat::Dxt1;
                    break;
                case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
                    format = ESM4::SkinPixelFormat::Dxt3;
                    break;
                case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                    format = ESM4::SkinPixelFormat::Dxt5;
                    break;
                case GL_RGB:
                    format = ESM4::SkinPixelFormat::Rgb;
                    break;
                case GL_RGBA:
                    format = ESM4::SkinPixelFormat::Rgba;
                    break;
                case GL_BGR:
                    format = ESM4::SkinPixelFormat::Bgr;
                    break;
                case GL_BGRA:
                    format = ESM4::SkinPixelFormat::Bgra;
                    break;
                default:
                    return false;
            }
            if (!image.isCompressed())
            {
                // Bytes for each channel, and the rows packed as close as the pixels are
                const unsigned int bytes
                    = (format == ESM4::SkinPixelFormat::Rgb || format == ESM4::SkinPixelFormat::Bgr) ? 3 : 4;
                if (image.getDataType() != GL_UNSIGNED_BYTE
                    || image.getRowSizeInBytes() != static_cast<unsigned int>(image.s()) * bytes)
                    return false;
            }
            if (image.data() == nullptr || image.r() != 1)
                return false;
            return ESM4::decodeSkinImage(format, image.data(), image.getTotalSizeInBytes(), image.s(), image.t(), skin);
        }

        osg::ref_ptr<osg::Image> makeImage(const ESM4::SkinImage& skin, const std::string& name)
        {
            if (skin.mWidth <= 0 || skin.mHeight <= 0)
                return nullptr;
            osg::ref_ptr<osg::Image> image = new osg::Image;
            image->allocateImage(skin.mWidth, skin.mHeight, 1, GL_RGB, GL_UNSIGNED_BYTE);
            image->setInternalTextureFormat(GL_RGB8);
            image->setOrigin(osg::Image::TOP_LEFT);
            image->setFileName(name);
            std::copy(skin.mPixels.begin(), skin.mPixels.end(), image->data());
            return image;
        }

        // The skin of the editor's textures: twice the tint on the skin
        constexpr float sTintScale = 2.f;

        osg::ref_ptr<osg::Image> tintedSkin(const osg::Image& skin, const osg::Image& tint, bool average)
        {
            const std::string name = skin.getFileName() + (average ? " * average of " : " * ") + tint.getFileName();
            const bool named = !skin.getFileName().empty() && !tint.getFileName().empty();
            if (named)
            {
                const std::lock_guard lock(sCacheMutex);
                const auto found = sTintedSkins.find(name);
                if (found != sTintedSkins.end())
                {
                    osg::ref_ptr<osg::Image> shared;
                    if (found->second.lock(shared))
                        return shared;
                }
            }

            ESM4::SkinImage skinPixels;
            ESM4::SkinImage tintPixels;
            if (!decodeImage(skin, skinPixels) || !decodeImage(tint, tintPixels))
                return nullptr;
            const ESM4::SkinImage tinted = average
                ? ESM4::tintSkin(skinPixels, ESM4::averageColour(tintPixels), sTintScale)
                : ESM4::tintSkin(skinPixels, tintPixels, sTintScale);
            osg::ref_ptr<osg::Image> result = makeImage(tinted, name);
            if (result != nullptr && named)
            {
                const std::lock_guard lock(sCacheMutex);
                sTintedSkins[name] = result;
            }
            return result;
        }
    }

    osg::ref_ptr<osg::Image> tintSkinImage(const osg::Image& skin, const osg::Image& tint)
    {
        return tintedSkin(skin, tint, false);
    }

    osg::ref_ptr<osg::Image> tintSkinImageByAverage(const osg::Image& skin, const osg::Image& tint)
    {
        return tintedSkin(skin, tint, true);
    }

    namespace
    {
        class TintHair : public osg::NodeVisitor
        {
        public:
            explicit TintHair(const osg::Vec3f& colour)
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
                , mColour(colour)
            {
            }

            void apply(osg::Node& node) override
            {
                tint(node);
                traverse(node);
            }

            void apply(osg::Group& group) override
            {
                tint(group);
                traverse(group);
            }

            std::size_t mChanged = 0;

        private:
            void tint(osg::Node& node)
            {
                osg::StateSet* stateset = node.getStateSet();
                // The node of a shape is the one with the picture; a mesh itself is that of every character
                if (stateset == nullptr || stateset->getTextureAttributeList().empty() || node.asDrawable() != nullptr)
                    return;
                const SceneUtil::Material* material
                    = dynamic_cast<const SceneUtil::Material*>(stateset->getAttribute(osg::StateAttribute::MATERIAL));
                osg::ref_ptr<SceneUtil::Material> own = material != nullptr
                    ? new SceneUtil::Material(*material, osg::CopyOp::SHALLOW_COPY)
                    : new SceneUtil::Material;
                own->setVertexColorMode(SceneUtil::VertexColorModes::None);
                own->setDiffuse(osg::Vec4f(mColour, own->getDiffuse().a()));
                own->setAmbient(osg::Vec4f(mColour, own->getAmbient().a()));

                // The state set is that of every character with the same hair
                osg::ref_ptr<osg::StateSet> copy = new osg::StateSet(*stateset, osg::CopyOp::SHALLOW_COPY);
                copy->setAttributeAndModes(own, osg::StateAttribute::ON);
                node.setStateSet(copy);
                ++mChanged;
            }

            osg::Vec3f mColour;
        };

        class CollectHairShapes : public osg::NodeVisitor
        {
        public:
            CollectHairShapes()
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
            {
            }

            void apply(osg::Group& group) override
            {
                if (Misc::StringUtils::ciEqual(group.getName(), "hat"))
                    mHat.push_back(&group);
                else if (Misc::StringUtils::ciEqual(group.getName(), "nohat"))
                    mNoHat.push_back(&group);
                traverse(group);
            }

            std::vector<osg::Group*> mHat;
            std::vector<osg::Group*> mNoHat;
        };
    }

    std::size_t tintHair(osg::Node& part, const osg::Vec3f& colour)
    {
        TintHair tint(colour * sTintScale);
        part.accept(tint);
        return tint.mChanged;
    }

    std::size_t chooseHairShape(osg::Node& part, bool hat)
    {
        CollectHairShapes collect;
        part.accept(collect);
        if (collect.mHat.empty() || collect.mNoHat.empty())
            return 0;
        std::size_t removed = 0;
        for (osg::Group* shape : hat ? collect.mNoHat : collect.mHat)
            while (shape->getNumParents() > 0)
            {
                shape->getParent(0)->removeChild(shape);
                ++removed;
            }
        return removed;
    }

    std::shared_ptr<const ESM4::FaceMorphs> getFaceMorphs(
        const VFS::Manager& vfs, std::string_view model, std::string_view variant)
    {
        const VFS::Path::Normalized mesh = Misc::ResourceHelpers::correctMeshPath(VFS::Path::Normalized(model));
        std::string morphPath = ESM4::faceMorphPath(mesh.value());
        if (morphPath.empty())
            return nullptr;
        constexpr std::size_t extensionSize = 4; // ".egm"
        if (!variant.empty() && morphPath.size() >= extensionSize)
            morphPath.insert(morphPath.size() - extensionSize, variant);

        const std::lock_guard lock(sCacheMutex);
        const auto cached = sMorphCache.find(morphPath);
        if (cached != sMorphCache.end())
            return cached->second;

        std::shared_ptr<const ESM4::FaceMorphs> result;
        const VFS::Path::Normalized path(morphPath);
        if (vfs.exists(path))
        {
            Files::IStreamPtr stream = vfs.get(path);
            const std::string bytes((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
            ESM4::FaceMorphs morphs;
            std::string error;
            if (ESM4::readFaceMorphs(bytes, morphs, error))
                result = std::make_shared<const ESM4::FaceMorphs>(std::move(morphs));
            else
                Log(Debug::Warning) << "FaceGen: cannot use " << morphPath << ": " << error;
        }
        sMorphCache.emplace(morphPath, result);
        return result;
    }

    const ESM4::FaceTextureIndex& getFaceTextures(const VFS::Manager& vfs)
    {
        const std::lock_guard lock(sCacheMutex);
        if (sTextures == nullptr)
        {
            sTextures = std::make_unique<ESM4::FaceTextureIndex>();
            for (const std::string_view folder : { "textures/characters/facemods", "textures/characters/bodymods" })
                for (const VFS::Path::Normalized& name : vfs.getRecursiveDirectoryIterator(folder))
                    sTextures->add(name.value());
            Log(Debug::Verbose) << "FaceGen: " << sTextures->size() << " textures of single characters";
        }
        return *sTextures;
    }
}

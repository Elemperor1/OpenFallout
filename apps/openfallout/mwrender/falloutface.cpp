#include "falloutface.hpp"

#include <algorithm>
#include <iterator>
#include <map>
#include <mutex>
#include <string>

#include <osg/Geometry>
#include <osg/Texture2D>

#include <components/debug/debuglog.hpp>
#include <components/misc/resourcehelpers.hpp>
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
            explicit ReplaceDiffuseMap(osg::Image* image)
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
                , mImage(image)
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

                    // The state set and the texture are those of every character with the same part
                    osg::ref_ptr<osg::StateSet> own = new osg::StateSet(*stateset, osg::CopyOp::SHALLOW_COPY);
                    osg::ref_ptr<osg::Texture2D> replacement = new osg::Texture2D(*texture, osg::CopyOp::SHALLOW_COPY);
                    replacement->setImage(mImage);
                    replacement->setTextureSize(mImage->s(), mImage->t());
                    own->setTextureAttribute(unit, replacement, osg::StateAttribute::ON);
                    node.setStateSet(own);
                    ++mChanged;
                    return;
                }
            }

            osg::ref_ptr<osg::Image> mImage;
        };

        std::mutex sCacheMutex;
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
        ReplaceDiffuseMap replace(image);
        part.accept(replace);
        return replace.mChanged;
    }

    std::shared_ptr<const ESM4::FaceMorphs> getFaceMorphs(const VFS::Manager& vfs, std::string_view model)
    {
        const VFS::Path::Normalized mesh = Misc::ResourceHelpers::correctMeshPath(VFS::Path::Normalized(model));
        const std::string morphPath = ESM4::faceMorphPath(mesh.value());
        if (morphPath.empty())
            return nullptr;

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

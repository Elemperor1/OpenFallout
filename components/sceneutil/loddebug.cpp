#include "loddebug.hpp"

#include <osg/Geometry>
#include <osg/NodeVisitor>
#include <osg/PolygonMode>
#include <osg/TriangleIndexFunctor>

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "material.hpp"

namespace SceneUtil
{
    namespace
    {
        osg::Vec4f getColor(std::string_view path)
        {
            const std::size_t slash = path.find_last_of('/');
            const std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
            if (path.find("/blocks/") != std::string_view::npos)
                return name.find(".high.") != std::string_view::npos ? osg::Vec4f(0.2f, 1.f, 1.f, 1.f)
                                                                     : osg::Vec4f(1.f, 0.2f, 1.f, 1.f);
            const std::size_t level = name.find("level");
            if (level == std::string_view::npos)
                return osg::Vec4f(1.f, 1.f, 1.f, 1.f);
            switch (std::atoi(std::string(name.substr(level + 5, 3)).c_str()))
            {
                case 4:
                    return osg::Vec4f(1.f, 0.2f, 0.2f, 1.f);
                case 8:
                    return osg::Vec4f(0.2f, 1.f, 0.2f, 1.f);
                case 16:
                    return osg::Vec4f(0.3f, 0.4f, 1.f, 1.f);
                case 32:
                    return osg::Vec4f(1.f, 1.f, 0.2f, 1.f);
                default:
                    return osg::Vec4f(1.f, 1.f, 1.f, 1.f);
            }
        }

        struct KeepTriangles
        {
            const osg::Vec3Array* mVertices = nullptr;
            float mSkirtHeight = 0.f;
            std::vector<unsigned int> mIndices;

            void operator()(unsigned int a, unsigned int b, unsigned int c)
            {
                const std::size_t size = mVertices->size();
                if (a >= size || b >= size || c >= size)
                    return;
                const float za = (*mVertices)[a].z();
                const float zb = (*mVertices)[b].z();
                const float zc = (*mVertices)[c].z();
                if (std::max({ za, zb, zc }) - std::min({ za, zb, zc }) > mSkirtHeight)
                    return;
                mIndices.insert(mIndices.end(), { a, b, c });
            }
        };

        class DebugVisitor : public osg::NodeVisitor
        {
        public:
            DebugVisitor(const LodDebug& debug, float skirtHeight)
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
                , mDebug(debug)
                , mSkirtHeight(skirtHeight)
            {
            }

            void apply(osg::Node& node) override
            {
                if (mDebug.mFlat)
                    if (osg::StateSet* stateset = node.getStateSet())
                    {
                        stateset->getTextureAttributeList().clear();
                        stateset->getTextureModeList().clear();
                    }
                traverse(node);
            }

            void apply(osg::Geometry& geometry) override
            {
                if (mDebug.mNoSkirt)
                    removeSkirt(geometry);
                apply(static_cast<osg::Node&>(geometry));
            }

        private:
            void removeSkirt(osg::Geometry& geometry) const
            {
                const osg::Vec3Array* vertices = dynamic_cast<const osg::Vec3Array*>(geometry.getVertexArray());
                if (vertices == nullptr)
                    return;
                osg::TriangleIndexFunctor<KeepTriangles> keep;
                keep.mVertices = vertices;
                keep.mSkirtHeight = mSkirtHeight;
                geometry.accept(keep);
                geometry.removePrimitiveSet(0, geometry.getNumPrimitiveSets());
                geometry.addPrimitiveSet(new osg::DrawElementsUInt(
                    osg::PrimitiveSet::TRIANGLES, keep.mIndices.begin(), keep.mIndices.end()));
                geometry.dirtyBound();
            }

            const LodDebug& mDebug;
            float mSkirtHeight;
        };
    }

    LodDebug parseLodDebug(std::string_view words, std::vector<std::string>* unknown)
    {
        LodDebug debug;
        std::size_t position = 0;
        while (position < words.size())
        {
            const std::size_t end = words.find_first_of(" ,\t", position);
            const std::string_view word = words.substr(position, end == std::string_view::npos ? end : end - position);
            position = end == std::string_view::npos ? words.size() : end + 1;
            if (word.empty())
                continue;
            if (word == "flat")
                debug.mFlat = true;
            else if (word == "wireframe")
                debug.mWireframe = true;
            else if (word == "no-skirt")
                debug.mNoSkirt = true;
            else if (word == "objects-only")
                debug.mObjectsOnly = true;
            else if (word == "no-high")
                debug.mNoHigh = true;
            else if (word == "no-land")
                debug.mNoLand = true;
            else if (word == "no-records")
                debug.mNoRecords = true;
            else if (unknown != nullptr)
                unknown->emplace_back(word);
        }
        return debug;
    }

    void applyLodDebug(osg::Node& node, std::string_view path, const LodDebug& debug, float skirtHeight)
    {
        if (!debug.changesFiles())
            return;

        DebugVisitor visitor(debug, skirtHeight);
        node.accept(visitor);

        osg::StateSet* stateset = node.getOrCreateStateSet();
        if (debug.mFlat)
        {
            const osg::Vec4f color = getColor(path);
            osg::ref_ptr<Material> material(new Material({ .mDiffuse = color, .mAmbient = color }));
            stateset->setAttributeAndModes(material, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
        }
        if (debug.mWireframe)
            stateset->setAttributeAndModes(
                new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::LINE),
                osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    }
}

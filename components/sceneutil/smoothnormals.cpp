#include "smoothnormals.hpp"

#include <osg/Array>
#include <osg/Geometry>
#include <osg/NodeVisitor>
#include <osg/TriangleIndexFunctor>

#include <algorithm>
#include <vector>

namespace SceneUtil
{
    namespace
    {
        struct AccumulateNormals
        {
            const osg::Vec3Array* mVertices = nullptr;
            float mSkirtHeight = 0.f;
            std::vector<osg::Vec3f> mSums;

            void operator()(unsigned int a, unsigned int b, unsigned int c)
            {
                const std::size_t size = mVertices->size();
                if (a >= size || b >= size || c >= size)
                    return;
                const osg::Vec3f& va = (*mVertices)[a];
                const osg::Vec3f& vb = (*mVertices)[b];
                const osg::Vec3f& vc = (*mVertices)[c];
                if (mSkirtHeight > 0.f
                    && std::max({ va.z(), vb.z(), vc.z() }) - std::min({ va.z(), vb.z(), vc.z() }) > mSkirtHeight)
                    return;
                // The length of the cross product is twice the area of the triangle
                const osg::Vec3f normal = (vb - va) ^ (vc - va);
                mSums[a] += normal;
                mSums[b] += normal;
                mSums[c] += normal;
            }
        };

        class GenerateMissingNormalsVisitor : public osg::NodeVisitor
        {
        public:
            explicit GenerateMissingNormalsVisitor(float skirtHeight)
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
                , mSkirtHeight(skirtHeight)
            {
            }

            void apply(osg::Geometry& geometry) override { generateSmoothNormals(geometry, mSkirtHeight); }

        private:
            float mSkirtHeight;
        };
    }

    bool generateSmoothNormals(osg::Geometry& geometry, float skirtHeight)
    {
        if (geometry.getNormalArray() != nullptr)
            return false;
        const osg::Vec3Array* vertices = dynamic_cast<const osg::Vec3Array*>(geometry.getVertexArray());
        if (vertices == nullptr || vertices->empty())
            return false;

        osg::TriangleIndexFunctor<AccumulateNormals> accumulate;
        accumulate.mVertices = vertices;
        accumulate.mSkirtHeight = skirtHeight;
        accumulate.mSums.assign(vertices->size(), osg::Vec3f());
        geometry.accept(accumulate);

        osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array(vertices->size());
        bool any = false;
        for (std::size_t i = 0; i < normals->size(); ++i)
        {
            osg::Vec3f normal = accumulate.mSums[i];
            if (normal.normalize() > 0.f)
                any = true;
            else
                normal = osg::Vec3f(0.f, 0.f, 1.f);
            (*normals)[i] = normal;
        }
        if (!any)
            return false;

        geometry.setNormalArray(normals, osg::Array::BIND_PER_VERTEX);
        return true;
    }

    void generateMissingNormals(osg::Node& node, float skirtHeight)
    {
        GenerateMissingNormalsVisitor visitor(skirtHeight);
        node.accept(visitor);
    }
}

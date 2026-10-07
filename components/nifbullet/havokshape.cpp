#include "havokshape.hpp"

#include <algorithm>
#include <array>
#include <numeric>
#include <tuple>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <LinearMath/btConvexHullComputer.h>

#include <components/misc/convert.hpp>
#include <components/nif/data.hpp>
#include <components/nif/physics.hpp>
#include <components/resource/bulletshape.hpp>

namespace NifBullet
{
    namespace
    {
        // Havok shapes nest (a MOPP tree around a list around boxes), but not deeply
        constexpr int sMaxShapeDepth = 16;

        // Layers of Fallout 3 and New Vegas (`Fallout3Layer` of nif.xml)
        enum HavokLayer : std::uint8_t
        {
            Layer_Weapon = 5,
            Layer_Projectile = 6,
            Layer_Spell = 7,
            Layer_Biped = 8,
            Layer_Water = 11,
            Layer_Trigger = 12,
            Layer_Trap = 14,
            Layer_NonCollidable = 15,
            Layer_CloudTrap = 16,
            Layer_Portal = 18,
            Layer_DebrisSmall = 19,
            Layer_DebrisLarge = 20,
            Layer_AcousticSpace = 21,
            Layer_ActorZone = 22,
            Layer_ProjectileZone = 23,
            Layer_GasTrap = 24,
            Layer_ShellCasing = 25,
            Layer_DeadBiped = 29,
            Layer_CharController = 30,
            Layer_AvoidBox = 31,
            Layer_CameraSphere = 33,
            Layer_DoorDetection = 34,
            Layer_CameraPick = 35,
            Layer_ItemPick = 36,
            Layer_LineOfSight = 37,
            Layer_PathPick = 38,
            Layer_CustomPick1 = 39,
            Layer_CustomPick2 = 40,
            Layer_SpellExplosion = 41,
            Layer_DroppingPick = 42,
            Layer_Null = 43,
        };

        // The flag of a filter that turns the collision of a body or of a part of it off
        constexpr std::uint8_t sNoCollisionFlag = 0x40;

        btVector3 toBullet(const osg::Vec4f& value, float scale)
        {
            return btVector3(value.x() * scale, value.y() * scale, value.z() * scale);
        }

        // The hull of some points as a triangle mesh: the navigation mesh builder and the copies of collision shapes
        // know boxes and triangle meshes, so a convex hull is made into one, which costs nothing for a static object.
        // Null for points that make no hull.
        std::unique_ptr<btCollisionShape> makeConvexHullShape(const std::vector<btVector3>& points)
        {
            if (points.size() < 4)
                return nullptr;

            btConvexHullComputer hull;
            hull.compute(points[0].m_floats, sizeof(btVector3), static_cast<int>(points.size()), 0.f, 0.f);

            auto mesh = std::make_unique<btTriangleMesh>();
            for (int face = 0; face < hull.faces.size(); ++face)
            {
                // A face is a ring of edges, which a fan of triangles from its first vertex covers
                const btConvexHullComputer::Edge* edge = &hull.edges[hull.faces[face]];
                const int first = edge->getSourceVertex();
                int second = edge->getTargetVertex();
                edge = edge->getNextEdgeOfFace();
                int third = edge->getTargetVertex();
                while (third != first)
                {
                    mesh->addTriangle(hull.vertices[first], hull.vertices[second], hull.vertices[third]);
                    edge = edge->getNextEdgeOfFace();
                    second = third;
                    third = edge->getTargetVertex();
                }
            }

            if (mesh->getNumTriangles() == 0)
                return nullptr;

            auto shape = std::make_unique<Resource::TriangleMeshShape>(mesh.get(), true);
            std::ignore = mesh.release();
            return shape;
        }

        // Points on a sphere, as the corners, the middles of the edges and the middles of the faces of a cube (26
        // directions), which a hull makes into a sphere that is near enough for something that is only to be walked
        // into. A radius that is not above 0 adds nothing.
        void addSpherePoints(std::vector<btVector3>& points, const btVector3& centre, float radius)
        {
            if (!(radius > 0.f))
                return;
            for (int x = -1; x <= 1; ++x)
                for (int y = -1; y <= 1; ++y)
                    for (int z = -1; z <= 1; ++z)
                        if (x != 0 || y != 0 || z != 0)
                            points.push_back(centre + btVector3(x, y, z).normalized() * radius);
        }

        std::unique_ptr<btCollisionShape> makeCapsuleShape(const Nif::bhkCapsuleShape& capsule)
        {
            // The radius of each end is its own; the one of the convex shape is the same in the files that I know of
            const float radius1 = capsule.mRadius1 > 0.f ? capsule.mRadius1 : capsule.mRadius;
            const float radius2 = capsule.mRadius2 > 0.f ? capsule.mRadius2 : capsule.mRadius;

            std::vector<btVector3> points;
            points.reserve(52);
            addSpherePoints(points,
                btVector3(capsule.mPoint1.x(), capsule.mPoint1.y(), capsule.mPoint1.z()) * sHavokScale,
                radius1 * sHavokScale);
            addSpherePoints(points,
                btVector3(capsule.mPoint2.x(), capsule.mPoint2.y(), capsule.mPoint2.z()) * sHavokScale,
                radius2 * sHavokScale);
            return makeConvexHullShape(points);
        }

        std::unique_ptr<btCollisionShape> makeSphereShape(const Nif::bhkConvexShape& sphere)
        {
            std::vector<btVector3> points;
            points.reserve(26);
            addSpherePoints(points, btVector3(0, 0, 0), sphere.mRadius * sHavokScale);
            return makeConvexHullShape(points);
        }

        // The triangle strips of the data of a bhkNiTriStripsShape in one mesh. The vertices of an NiTriStripsData are
        // in game units already (only the packed data of a bhkPackedNiTriStripsShape is in Havok units), so only the
        // scale of the shape is applied. A data that has a filter that stops nothing is left out.
        std::unique_ptr<btCollisionShape> makeStripsShape(const Nif::bhkNiTriStripsShape& shape)
        {
            osg::Vec3f scale(1.f, 1.f, 1.f);
            if (shape.mScale.x() > 0.f && shape.mScale.y() > 0.f && shape.mScale.z() > 0.f)
                scale = osg::Vec3f(shape.mScale.x(), shape.mScale.y(), shape.mScale.z());

            // The filters are for the data one by one; if they are not, all of the data is taken
            const bool hasFilters = shape.mHavokFilters.size() == shape.mData.size();

            auto mesh = std::make_unique<btTriangleMesh>();
            for (std::size_t i = 0; i < shape.mData.size(); ++i)
            {
                if (shape.mData[i].empty() || (hasFilters && !isSolidHavokFilter(shape.mHavokFilters[i])))
                    continue;

                const Nif::NiTriStripsData& data = shape.mData[i].get();
                std::vector<btVector3> vertices;
                vertices.reserve(data.mVertices.size());
                for (const osg::Vec3f& vertex : data.mVertices)
                    vertices.emplace_back(vertex.x() * scale.x(), vertex.y() * scale.y(), vertex.z() * scale.z());

                for (const std::vector<std::uint16_t>& strip : data.mStrips)
                {
                    // Each vertex after the second makes a triangle with the two before it, turned over every other
                    // time. Triangles that have a vertex twice, or a vertex that is not there, are left out.
                    for (std::size_t j = 2; j < strip.size(); ++j)
                    {
                        const std::uint16_t a = strip[j - 2];
                        const std::uint16_t b = strip[j - 1];
                        const std::uint16_t c = strip[j];
                        if (a == b || b == c || a == c || a >= vertices.size() || b >= vertices.size()
                            || c >= vertices.size())
                            continue;
                        if (j % 2 == 0)
                            mesh->addTriangle(vertices[a], vertices[b], vertices[c]);
                        else
                            mesh->addTriangle(vertices[a], vertices[c], vertices[b]);
                    }
                }
            }

            if (mesh->getNumTriangles() == 0)
                return nullptr;

            auto result = std::make_unique<Resource::TriangleMeshShape>(mesh.get(), true);
            std::ignore = mesh.release();
            return result;
        }

        std::unique_ptr<btCollisionShape> makePackedStripsShape(const Nif::bhkPackedNiTriStripsShape& shape)
        {
            if (shape.mData.empty())
                return nullptr;

            const Nif::hkPackedNiTriStripsData& data = shape.mData.get();

            // The shape can scale the vertices of its data too; it is 1 in the files that I know of
            osg::Vec3f scale(1.f, 1.f, 1.f);
            if (shape.mScale.x() > 0.f && shape.mScale.y() > 0.f && shape.mScale.z() > 0.f)
                scale = osg::Vec3f(shape.mScale.x(), shape.mScale.y(), shape.mScale.z());

            std::vector<btVector3> vertices;
            vertices.reserve(data.mVertices.size());
            for (const osg::Vec3f& vertex : data.mVertices)
                vertices.emplace_back(vertex.x() * scale.x() * sHavokScale, vertex.y() * scale.y() * sHavokScale,
                    vertex.z() * scale.z() * sHavokScale);

            // The subshapes take the vertices one after the other and have filters of their own: what is in a layer
            // that is not solid, or has the flag for no collision, stops nothing, though the body does. Subshapes that
            // do not add up to the vertices are not read.
            std::vector<bool> solidVertices(vertices.size(), true);
            const std::size_t verticesInSubshapes
                = std::accumulate(data.mSubshapes.begin(), data.mSubshapes.end(), std::size_t(0),
                    [](std::size_t sum, const Nif::hkSubPartData& part) { return sum + part.mNumVertices; });
            if (!data.mSubshapes.empty() && verticesInSubshapes == vertices.size())
            {
                std::size_t first = 0;
                for (const Nif::hkSubPartData& part : data.mSubshapes)
                {
                    std::fill_n(
                        solidVertices.begin() + first, part.mNumVertices, isSolidHavokFilter(part.mHavokFilter));
                    first += part.mNumVertices;
                }
            }

            auto mesh = std::make_unique<btTriangleMesh>();
            for (const Nif::TriangleData& triangle : data.mTriangles)
            {
                const auto& indices = triangle.mTriangle;
                if (std::ranges::any_of(indices, [&](std::uint16_t index) { return index >= vertices.size(); }))
                    continue;
                // A triangle is in the subshape that its first vertex is in
                if (!solidVertices[indices[0]])
                    continue;
                mesh->addTriangle(vertices[indices[0]], vertices[indices[1]], vertices[indices[2]]);
            }

            if (mesh->getNumTriangles() == 0)
                return nullptr;

            auto result = std::make_unique<Resource::TriangleMeshShape>(mesh.get(), true);
            std::ignore = mesh.release();
            return result;
        }

        std::string convert(
            const Nif::bhkShape& shape, const osg::Matrixf& transform, std::vector<HavokPiece>& pieces, int depth);

        // All the shapes of a list; the first one that is not read ends it
        std::string convertAll(
            const Nif::bhkShapeList& shapes, const osg::Matrixf& transform, std::vector<HavokPiece>& pieces, int depth)
        {
            for (const auto& child : shapes)
            {
                if (child.empty())
                    continue;
                std::string unsupported = convert(child.get(), transform, pieces, depth + 1);
                if (!unsupported.empty())
                    return unsupported;
            }
            return {};
        }

        std::string convert(
            const Nif::bhkShape& shape, const osg::Matrixf& transform, std::vector<HavokPiece>& pieces, int depth)
        {
            if (depth > sMaxShapeDepth)
                return "a shape nested too deeply";

            switch (shape.mRecordType)
            {
                case Nif::RC_bhkMoppBvTreeShape:
                {
                    const auto& mopp = static_cast<const Nif::bhkMoppBvTreeShape&>(shape);
                    if (mopp.mShape.empty())
                        return {};
                    return convert(mopp.mShape.get(), transform, pieces, depth + 1);
                }
                case Nif::RC_bhkListShape:
                    return convertAll(
                        static_cast<const Nif::bhkListShape&>(shape).mSubshapes, transform, pieces, depth);
                case Nif::RC_bhkConvexListShape:
                    // A list of convex shapes of one kind and material; its radius only rounds them
                    return convertAll(
                        static_cast<const Nif::bhkConvexListShape&>(shape).mSubShapes, transform, pieces, depth);
                case Nif::RC_bhkConvexTransformShape:
                {
                    // bhkTransformShape is the same record under another name. The matrix is read as OSG has it (the
                    // translation in its last row) and the translation is in Havok units like the rest.
                    const auto& transformShape = static_cast<const Nif::bhkConvexTransformShape&>(shape);
                    if (transformShape.mShape.empty())
                        return {};
                    osg::Matrixf local = transformShape.mTransform;
                    local.setTrans(local.getTrans() * sHavokScale);
                    return convert(transformShape.mShape.get(), local * transform, pieces, depth + 1);
                }
                case Nif::RC_bhkBoxShape:
                {
                    // The extents of a box are its half extents
                    const auto& box = static_cast<const Nif::bhkBoxShape&>(shape);
                    pieces.push_back(
                        { std::make_unique<btBoxShape>(Misc::Convert::toBullet(box.mExtents * sHavokScale)),
                            transform });
                    return {};
                }
                case Nif::RC_bhkConvexVerticesShape:
                {
                    const auto& convex = static_cast<const Nif::bhkConvexVerticesShape&>(shape);
                    std::vector<btVector3> points;
                    points.reserve(convex.mVertices.size());
                    for (const osg::Vec4f& vertex : convex.mVertices)
                        points.push_back(toBullet(vertex, sHavokScale));
                    if (auto hull = makeConvexHullShape(points))
                        pieces.push_back({ std::move(hull), transform });
                    return {};
                }
                case Nif::RC_bhkPackedNiTriStripsShape:
                {
                    if (auto strips = makePackedStripsShape(static_cast<const Nif::bhkPackedNiTriStripsShape&>(shape)))
                        pieces.push_back({ std::move(strips), transform });
                    return {};
                }
                case Nif::RC_bhkNiTriStripsShape:
                {
                    if (auto strips = makeStripsShape(static_cast<const Nif::bhkNiTriStripsShape&>(shape)))
                        pieces.push_back({ std::move(strips), transform });
                    return {};
                }
                case Nif::RC_bhkCapsuleShape:
                {
                    if (auto capsule = makeCapsuleShape(static_cast<const Nif::bhkCapsuleShape&>(shape)))
                        pieces.push_back({ std::move(capsule), transform });
                    return {};
                }
                case Nif::RC_bhkSphereShape:
                {
                    if (auto sphere = makeSphereShape(static_cast<const Nif::bhkConvexShape&>(shape)))
                        pieces.push_back({ std::move(sphere), transform });
                    return {};
                }
                default:
                    return shape.mRecordName.empty() ? "a shape of an unknown kind" : shape.mRecordName;
            }
        }
    }

    bool isSolidHavokLayer(std::uint8_t layer)
    {
        switch (layer)
        {
            case Layer_Weapon:
            case Layer_Projectile:
            case Layer_Spell:
            case Layer_Biped:
            case Layer_Water:
            case Layer_Trigger:
            case Layer_Trap:
            case Layer_NonCollidable:
            case Layer_CloudTrap:
            case Layer_Portal:
            case Layer_DebrisSmall:
            case Layer_DebrisLarge:
            case Layer_AcousticSpace:
            case Layer_ActorZone:
            case Layer_ProjectileZone:
            case Layer_GasTrap:
            case Layer_ShellCasing:
            case Layer_DeadBiped:
            case Layer_CharController:
            case Layer_AvoidBox:
            case Layer_CameraSphere:
            case Layer_DoorDetection:
            case Layer_CameraPick:
            case Layer_ItemPick:
            case Layer_LineOfSight:
            case Layer_PathPick:
            case Layer_CustomPick1:
            case Layer_CustomPick2:
            case Layer_SpellExplosion:
            case Layer_DroppingPick:
            case Layer_Null:
                return false;
            default:
                return true;
        }
    }

    bool isSolidHavokFilter(const Nif::HavokFilter& filter)
    {
        return (filter.mFlags & sNoCollisionFlag) == 0 && isSolidHavokLayer(filter.mLayer);
    }

    std::string convertHavokShape(
        const Nif::bhkShape& shape, const osg::Matrixf& transform, std::vector<HavokPiece>& pieces)
    {
        return convert(shape, transform, pieces, 0);
    }
}

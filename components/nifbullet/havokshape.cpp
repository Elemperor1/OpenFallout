#include "havokshape.hpp"

#include <algorithm>
#include <array>
#include <tuple>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <LinearMath/btConvexHullComputer.h>

#include <components/misc/convert.hpp>
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
            Layer_CollisionBox = 32,
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
        };

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

            auto mesh = std::make_unique<btTriangleMesh>();
            for (const Nif::TriangleData& triangle : data.mTriangles)
            {
                const auto& indices = triangle.mTriangle;
                if (std::ranges::any_of(indices, [&](std::uint16_t index) { return index >= vertices.size(); }))
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
                {
                    const auto& list = static_cast<const Nif::bhkListShape&>(shape);
                    for (const auto& child : list.mSubshapes)
                    {
                        if (child.empty())
                            continue;
                        std::string unsupported = convert(child.get(), transform, pieces, depth + 1);
                        if (!unsupported.empty())
                            return unsupported;
                    }
                    return {};
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
            case Layer_CollisionBox:
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
                return false;
            default:
                return true;
        }
    }

    std::string convertHavokShape(
        const Nif::bhkShape& shape, const osg::Matrixf& transform, std::vector<HavokPiece>& pieces)
    {
        return convert(shape, transform, pieces, 0);
    }
}

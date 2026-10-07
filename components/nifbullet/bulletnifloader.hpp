#ifndef OPENFALLOUT_COMPONENTS_NIFBULLET_BULLETNIFLOADER_HPP
#define OPENFALLOUT_COMPONENTS_NIFBULLET_BULLETNIFLOADER_HPP

#include <cassert>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <osg/BoundingBox>
#include <osg/Matrixf>
#include <osg/Referenced>
#include <osg/ref_ptr>

#include <BulletCollision/CollisionShapes/btCompoundShape.h>

#include <components/debug/debuglog.hpp>
#include <components/nif/niffile.hpp>
#include <components/resource/bulletshape.hpp>

class btTriangleMesh;
class btCompoundShape;
class btCollisionShape;

namespace Nif
{
    struct NiAVObject;
    struct NiNode;
    struct NiGeometry;
    struct Parent;
}

namespace NifBullet
{
    class HavokSurvey;

    /**
     *Load bulletShape from NIF files.
     */
    class BulletNifLoader
    {
    public:
        /// @param havokCollision make the collision of Fallout 3 and New Vegas meshes from their Havok data (bodies in
        /// boxes, convex hulls and packed triangle strips) instead of from the rendered geometry, where the file has
        /// shapes of those kinds and the result fits the rendered geometry; the rendered geometry stays where it does
        /// not.
        explicit BulletNifLoader(bool havokCollision = false)
            : mHavokCollision(havokCollision)
        {
        }

        void warn(const std::string& msg) { Log(Debug::Warning) << "NIFLoader: Warn: " << msg; }

        [[noreturn]] void fail(const std::string& msg)
        {
            Log(Debug::Error) << "NIFLoader: Fail: " << msg;
            abort();
        }

        /// Counts what is in the Havok data of the files that are loaded and what is made of it (which files get their
        /// Havok collision and why the others do not, what the bodies are, how the size of the shapes compares with
        /// the one of the geometry that is drawn). Null, the default, counts nothing. The survey must outlive the
        /// loads.
        void setHavokSurvey(HavokSurvey* survey) { mHavokSurvey = survey; }

        std::shared_ptr<Resource::BulletShape> load(Nif::FileView file);

    private:
        bool findBoundingBox(const Nif::NiAVObject& node);

        struct HandleNodeArgs
        {
            bool mHasMarkers{ false };
            bool mHasTriMarkers{ false };
            bool mAnimated{ false };
            bool mGenerateCollision{ false };
            bool mAvoid{ false };
            const Nif::NiNode* mCollisionNode{ nullptr };
        };

        void handleRoot(Nif::FileView nif, const Nif::NiAVObject& node, HandleNodeArgs args);
        void handleNode(const Nif::NiAVObject& node, const Nif::Parent* parent, HandleNodeArgs args);
        void handleGeometry(const Nif::NiGeometry& nifNode, const Nif::Parent* parent, HandleNodeArgs args);

        /// Adds a shape to a compound shape, at a transform whose scale is the scale of the shape.
        void addChildShape(
            std::unique_ptr<btCollisionShape> childShape, osg::Matrixf transform, btCompoundShape& compound);

        /// Replaces the collision made from rendered geometry with the one of the Havok bodies of the roots in
        /// mHavokRoots, if that is possible and it fits.
        void applyHavokCollision();
        // What the bodies of a file are: none with a shape, some that stop nothing, or at least one that stops what
        // walks
        enum class HavokBodies
        {
            None,
            NotSolid,
            Solid
        };
        bool collectHavokBodies(const Nif::NiAVObject& node, const Nif::Parent* parent, bool animated,
            btCompoundShape& compound, HavokBodies& bodies, std::string& unsupported);

        void survey(std::string_view section, std::string_view answer) const;

        bool mHavokCollision;
        // Whether a root of the file that is loaded has the flag for collision (bit 1 of its BSXFlags)
        bool mHasCollisionFlag = false;
        HavokSurvey* mHavokSurvey = nullptr;
        std::vector<const Nif::NiAVObject*> mHavokRoots;

        std::unique_ptr<btCompoundShape, Resource::DeleteCollisionShape> mCompoundShape;
        std::unique_ptr<btCompoundShape, Resource::DeleteCollisionShape> mAvoidCompoundShape;

        std::shared_ptr<Resource::BulletShape> mShape;
    };

}

#endif

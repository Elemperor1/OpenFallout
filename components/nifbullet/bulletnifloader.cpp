#include "bulletnifloader.hpp"
#include <memory>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <limits>
#include <sstream>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/files/conversion.hpp>
#include <components/misc/convert.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/nif/extra.hpp>
#include <components/nif/nifstream.hpp>
#include <components/nif/node.hpp>
#include <components/nif/parent.hpp>
#include <components/nif/physics.hpp>

#include "havokshape.hpp"
#include "havoksurvey.hpp"

namespace
{
    // Added to the hash of the file of a shape that is made from its Havok data. Change it when what is made from the
    // Havok data changes, so that tiles of the navigation mesh made from the old shapes are not used.
    constexpr std::string_view sHavokHashSuffix = "havok1";

    constexpr std::uint16_t sCollisionObjectActive = 0x1;

    bool pathFileNameStartsWithX(const std::string& path)
    {
        const std::size_t slashpos = path.find_last_of("/\\");
        const std::size_t letterPos = slashpos == std::string::npos ? 0 : slashpos + 1;
        return letterPos < path.size() && (path[letterPos] == 'x' || path[letterPos] == 'X');
    }

    // Whether a controller of the node moves it
    bool hasMovingController(const Nif::NiAVObject& node)
    {
        for (Nif::NiTimeControllerPtr ctrl = node.mController; !ctrl.empty(); ctrl = ctrl->mNext)
        {
            if (!ctrl->isActive())
                continue;
            switch (ctrl->mRecordType)
            {
                case Nif::RC_NiKeyframeController:
                case Nif::RC_NiPathController:
                case Nif::RC_NiRollController:
                    return true;
                default:
                    continue;
            }
        }
        return false;
    }

    osg::Matrixf getWorldTransform(const Nif::NiAVObject& node, const Nif::Parent* parent)
    {
        osg::Matrixf transform = node.mTransform.toMatrix();
        for (; parent != nullptr; parent = parent->mParent)
            transform *= parent->mNiNode.mTransform.toMatrix();
        return transform;
    }

    // The Havok data is made to fit the model it belongs to, so it has about its size and place. A collision that does
    // not (in the file, or because of what this loader does not know about it) is more likely wrong than right, and
    // an invisible wall, or a wall that is not there, is what a person walking sees of it.
    struct HavokFit
    {
        // The longest extent of the Havok shape over the one of the geometry that is drawn
        float mSizeRatio;
        // The distance of the centres of the two, in longest extents of the geometry that is drawn
        float mOffset;

        bool fits() const { return mSizeRatio <= 4.f && mSizeRatio >= 0.25f && mOffset <= 0.5f; }
    };

    HavokFit measureHavokFit(const btCompoundShape& havok, const btCompoundShape& rendered)
    {
        btVector3 havokMin, havokMax, renderedMin, renderedMax;
        havok.getAabb(btTransform::getIdentity(), havokMin, havokMax);
        rendered.getAabb(btTransform::getIdentity(), renderedMin, renderedMax);

        const btVector3 havokExtents = havokMax - havokMin;
        const btVector3 renderedExtents = renderedMax - renderedMin;
        const btScalar havokSize = havokExtents[havokExtents.maxAxis()];
        const btScalar renderedSize = renderedExtents[renderedExtents.maxAxis()];

        const btVector3 havokCenter = (havokMin + havokMax) * 0.5f;
        const btVector3 renderedCenter = (renderedMin + renderedMax) * 0.5f;
        // Nothing drawn that has a size is nothing that this fits
        if (!(renderedSize > 0.f))
            return { std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() };
        return { static_cast<float>(havokSize / renderedSize),
            static_cast<float>((havokCenter - renderedCenter).length() / renderedSize) };
    }

    std::string_view motionTypeName(Nif::HkMotionType type)
    {
        switch (type)
        {
            case Nif::HkMotionType::Motion_Dynamic:
                return "dynamic";
            case Nif::HkMotionType::Motion_SphereInertia:
                return "sphere inertia";
            case Nif::HkMotionType::Motion_SphereStabilized:
                return "sphere stabilized";
            case Nif::HkMotionType::Motion_BoxInertia:
                return "box inertia";
            case Nif::HkMotionType::Motion_BoxStabilized:
                return "box stabilized";
            case Nif::HkMotionType::Motion_Keyframed:
                return "keyframed";
            case Nif::HkMotionType::Motion_Fixed:
                return "fixed";
            case Nif::HkMotionType::Motion_ThinBox:
                return "thin box";
            case Nif::HkMotionType::Motion_Character:
                return "character";
            default:
                return "invalid";
        }
    }

    std::string_view responseTypeName(Nif::HkResponseType type)
    {
        switch (type)
        {
            case Nif::HkResponseType::Response_SimpleContact:
                return "simple contact";
            case Nif::HkResponseType::Response_Reporting:
                return "reporting";
            case Nif::HkResponseType::Response_None:
                return "none";
            default:
                return "invalid";
        }
    }

    // The files that do not get their Havok collision say so, but only so many times: a run on real data would
    // otherwise have a line for each of thousands of meshes
    void logHavokNotUsed(const std::string& file, std::string_view reason)
    {
        static std::atomic_int count = 0;
        constexpr int limit = 40;
        const int n = count.fetch_add(1);
        if (n < limit)
            Log(Debug::Info) << "Havok collision of " << file << " is not used: " << reason;
        else if (n == limit)
            Log(Debug::Info) << "Havok collision: no more files that do not get it are logged at this level";
        else
            Log(Debug::Verbose) << "Havok collision of " << file << " is not used: " << reason;
    }

}

namespace NifBullet
{

    std::shared_ptr<Resource::BulletShape> BulletNifLoader::load(Nif::FileView nif)
    {
        mShape = std::make_shared<Resource::BulletShape>();

        mCompoundShape.reset();
        mAvoidCompoundShape.reset();
        mHavokRoots.clear();
        mHasCollisionFlag = false;

        mShape->mFileHash = nif.getHash();

        const size_t numRoots = nif.numRoots();
        std::vector<const Nif::NiAVObject*> roots;
        for (size_t i = 0; i < numRoots; ++i)
        {
            const Nif::Record* r = nif.getRoot(i);
            if (!r)
                continue;
            const Nif::NiAVObject* node = dynamic_cast<const Nif::NiAVObject*>(r);
            if (node)
                roots.emplace_back(node);
        }
        mShape->mFileName = nif.getFilename();
        if (roots.empty())
        {
            warn("Found no root nodes in NIF file " + mShape->mFileName.value());
            return mShape;
        }

        for (const Nif::NiAVObject* node : roots)
            if (findBoundingBox(*node))
                break;

        HandleNodeArgs args;

        // files with the name convention xmodel.nif usually have keyframes stored in a separate file xmodel.kf (see
        // Animation::addAnimSource). assume all nodes in the file will be animated
        // TODO: investigate whether this should and could be optimized.
        args.mAnimated = pathFileNameStartsWithX(mShape->mFileName);

        for (const Nif::NiAVObject* node : roots)
            handleRoot(nif, *node, args);

        // Once for the file, whatever its roots are
        if (nif.getBethVersion() == Nif::NIFFile::BethVersion::BETHVER_FO3)
            survey("Files of Fallout 3 and New Vegas", mHasCollisionFlag ? "collision flag set" : "no collision flag");

        if (!mHavokRoots.empty())
            applyHavokCollision();

        if (mCompoundShape)
            mShape->mCollisionShape = std::move(mCompoundShape);

        if (mAvoidCompoundShape)
            mShape->mAvoidCollisionShape = std::move(mAvoidCompoundShape);

        return mShape;
    }

    // Find a bounding box in the node hierarchy to use for actor collision
    bool BulletNifLoader::findBoundingBox(const Nif::NiAVObject& node)
    {
        if (Misc::StringUtils::ciEqual(node.mName, "Bounding Box"))
        {
            if (node.mBounds.mType == Nif::BoundingVolume::Type::BOX_BV
                && std::ranges::all_of(node.mBounds.mBox.mExtents._v, [](float extent) { return extent > 0.f; }))
            {
                mShape->mCollisionBox.mExtents = node.mBounds.mBox.mExtents;
                mShape->mCollisionBox.mCenter = node.mBounds.mBox.mCenter;
            }
            else
            {
                warn("Invalid Bounding Box node bounds in file " + mShape->mFileName.value());
            }
            return true;
        }

        if (auto ninode = dynamic_cast<const Nif::NiNode*>(&node))
            for (const auto& child : ninode->mChildren)
                if (!child.empty() && findBoundingBox(child.get()))
                    return true;

        return false;
    }

    void BulletNifLoader::handleRoot(Nif::FileView nif, const Nif::NiAVObject& node, HandleNodeArgs args)
    {
        // Gamebryo/Bethbryo meshes
        if (nif.getVersion() >= Nif::NIFStream::generateVersion(10, 0, 1, 0))
        {
            // Handle BSXFlags
            const Nif::NiIntegerExtraData* bsxFlags = nullptr;
            for (const auto& e : node.getExtraList())
            {
                if (!e.empty() && e->mRecordType == Nif::RC_BSXFlags)
                {
                    bsxFlags = static_cast<const Nif::NiIntegerExtraData*>(e.getPtr());
                    break;
                }
            }

            // Collision flag
            if (!bsxFlags || !(bsxFlags->mData & 2))
                return;
            mHasCollisionFlag = true;

            // Editor marker flag
            if (bsxFlags->mData & 32)
                args.mHasMarkers = true;

            // The rendered geometry is the collision, unless the Havok data replaces it (applyHavokCollision), which
            // is read for Fallout 3 and New Vegas only: other games have other layers, scales and shapes
            if (mHavokCollision && nif.getBethVersion() == Nif::NIFFile::BethVersion::BETHVER_FO3)
                mHavokRoots.push_back(&node);
            args.mGenerateCollision = true;
        }
        // Pre-Gamebryo meshes
        else
        {
            bool recursiveRcn = false;
            // Check for extra data
            for (const auto& e : node.getExtraList())
            {
                if (!e.empty() && e->mRecordType == Nif::RC_NiStringExtraData)
                {
                    // String markers may contain important information
                    // affecting the entire subtree of this node
                    auto sd = static_cast<const Nif::NiStringExtraData*>(e.getPtr());

                    // Editor marker flag
                    if (sd->mData == "MRK")
                        args.mHasTriMarkers = true;
                    else if (Misc::StringUtils::ciStartsWith(sd->mData, "NC"))
                    {
                        // NC prefix is case-insensitive but the second C in NCC flag needs be uppercase.

                        // Collide only with camera.
                        if (sd->mData.length() > 2 && sd->mData[2] == 'C')
                            mShape->mVisualCollisionType = Resource::VisualCollisionType::Camera;
                        // No collision.
                        else
                            mShape->mVisualCollisionType = Resource::VisualCollisionType::Default;
                    }
                    else if (sd->mData == "RCN")
                        recursiveRcn = true;
                }
            }

            const Nif::NiNode* ninode = dynamic_cast<const Nif::NiNode*>(&node);
            if (ninode)
                args.mCollisionNode = ninode->findRootCollisionNode(recursiveRcn);
            if (!args.mCollisionNode)
                args.mGenerateCollision = true;
            else if (args.mCollisionNode->mChildren.empty())
            {
                // FIXME: BulletNifLoader should never have to provide rendered geometry for camera collision
                args.mGenerateCollision = true;
                mShape->mVisualCollisionType = Resource::VisualCollisionType::Camera;
            }
        }

        handleNode(node, nullptr, args);
    }

    void BulletNifLoader::handleNode(const Nif::NiAVObject& node, const Nif::Parent* parent, HandleNodeArgs args)
    {
        // TODO: allow on-the fly collision switching via toggling this flag
        if (node.mRecordType == Nif::RC_NiCollisionSwitch && !node.collisionActive())
            return;

        if (!args.mAnimated && hasMovingController(node))
            args.mAnimated = true;

        if (node.mRecordType == Nif::RC_RootCollisionNode)
        {
            // Encountered our RootCollisionNode inside an autogenerated mesh.
            // We treat empty RootCollisionNodes as NCC flag (set collisionType to `Camera`)
            // and generate the camera collision shape based on rendered geometry.
            if (args.mCollisionNode == &node && args.mGenerateCollision
                && mShape->mVisualCollisionType == Resource::VisualCollisionType::Camera)
                return;

            // Standard handling
            if (!args.mCollisionNode)
            {
                Log(Debug::Info) << "BulletNifLoader: Unexpected RootCollisionNode in " << mShape->mFileName
                                 << ". Treating as visible geometry.";
            }
            else if (args.mCollisionNode != &node)
            {
                Log(Debug::Info) << "BulletNifLoader: Extra RootCollisionNode in " << mShape->mFileName
                                 << ". Treating as visible geometry.";
            }
            else
            {
                args.mGenerateCollision = true;
            }
        }

        // Don't collide with AvoidNode shapes
        if (node.mRecordType == Nif::RC_AvoidNode)
            args.mAvoid = true;

        if (args.mGenerateCollision)
        {
            auto geometry = dynamic_cast<const Nif::NiGeometry*>(&node);
            if (geometry)
                handleGeometry(*geometry, parent, args);
        }

        // For NiNodes, loop through children
        if (const Nif::NiNode* ninode = dynamic_cast<const Nif::NiNode*>(&node))
        {
            const Nif::Parent currentParent{ *ninode, parent };
            for (const auto& child : ninode->mChildren)
            {
                if (!child.empty())
                {
                    assert(std::find(child->mParents.begin(), child->mParents.end(), ninode) != child->mParents.end());
                    handleNode(child.get(), &currentParent, args);
                }
                // For NiSwitchNodes and NiFltAnimationNodes, only use the first child
                // TODO: must synchronize with the rendering scene graph somehow
                // Doing this for NiLODNodes is unsafe (the first level might not be the closest)
                if (node.mRecordType == Nif::RC_NiSwitchNode || node.mRecordType == Nif::RC_NiFltAnimationNode)
                    break;
            }
        }
    }

    void BulletNifLoader::handleGeometry(
        const Nif::NiGeometry& niGeometry, const Nif::Parent* nodeParent, HandleNodeArgs args)
    {
        // This flag comes from BSXFlags
        if (args.mHasMarkers && Misc::StringUtils::ciStartsWith(niGeometry.mName, "EditorMarker"))
            return;

        // This flag comes from Morrowind
        if (args.mHasTriMarkers && Misc::StringUtils::ciStartsWith(niGeometry.mName, "Tri EditorMarker"))
            return;

        if (!niGeometry.mSkin.empty())
            args.mAnimated = false;

        std::unique_ptr<btCollisionShape> childShape = niGeometry.getCollisionShape();
        if (childShape == nullptr)
            return;

        const osg::Matrixf transform = getWorldTransform(niGeometry, nodeParent);

        if (!args.mAvoid)
        {
            if (!mCompoundShape)
                mCompoundShape.reset(new btCompoundShape);

            if (args.mAnimated)
                mShape->mAnimatedShapes.emplace(niGeometry.mRecordIndex, mCompoundShape->getNumChildShapes());
            addChildShape(std::move(childShape), transform, *mCompoundShape);
        }
        else
        {
            if (!mAvoidCompoundShape)
                mAvoidCompoundShape.reset(new btCompoundShape);
            addChildShape(std::move(childShape), transform, *mAvoidCompoundShape);
        }
    }

    void BulletNifLoader::addChildShape(
        std::unique_ptr<btCollisionShape> childShape, osg::Matrixf transform, btCompoundShape& compound)
    {
        if (childShape->getShapeType() == TRIANGLE_MESH_SHAPE_PROXYTYPE)
        {
            auto scaledShape = std::make_unique<Resource::ScaledTriangleMeshShape>(
                static_cast<btBvhTriangleMeshShape*>(childShape.get()), Misc::Convert::toBullet(transform.getScale()));
            std::ignore = childShape.release();

            childShape = std::move(scaledShape);
        }
        else
        {
            childShape->setLocalScaling(Misc::Convert::toBullet(transform.getScale()));
        }

        transform.orthoNormalize(transform);

        btTransform trans;
        trans.setOrigin(Misc::Convert::toBullet(transform.getTrans()));
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                trans.getBasis()[i][j] = transform(j, i);

        compound.addChildShape(trans, childShape.get());

        std::ignore = childShape.release();
    }

    void BulletNifLoader::survey(std::string_view section, std::string_view answer) const
    {
        if (mHavokSurvey != nullptr)
            mHavokSurvey->add(section, answer, mShape->mFileName.value());
    }

    void BulletNifLoader::applyHavokCollision()
    {
        std::unique_ptr<btCompoundShape, Resource::DeleteCollisionShape> havok(new btCompoundShape);
        HavokBodies bodies = HavokBodies::None;
        std::string unsupported;
        // The same files as for rendered geometry (see load) are taken for animated as a whole
        const bool animated = pathFileNameStartsWithX(mShape->mFileName);
        bool supported = true;
        for (const Nif::NiAVObject* root : mHavokRoots)
        {
            // A survey goes on after a body that is not read, so that it counts the bodies that come after it too
            if (!collectHavokBodies(*root, nullptr, animated, *havok, bodies, unsupported))
            {
                supported = false;
                if (mHavokSurvey == nullptr)
                    break;
            }
        }
        if (!supported)
        {
            logHavokNotUsed(mShape->mFileName.value(), "it has " + unsupported);
            survey("Havok collision of the files", "not used, it has " + unsupported);
            return;
        }

        if (havok->getNumChildShapes() == 0)
        {
            // No body in the file is solid (or there is none, or what is solid has no pieces): this is not what a
            // Havok file looks like, and the rendered geometry is as good a guess as any
            std::string reason = "it has no body with a shape of a kind that is read";
            if (bodies == HavokBodies::NotSolid)
                reason = "none of its bodies is solid";
            else if (bodies == HavokBodies::Solid)
                reason = "its solid bodies have no pieces (an empty or degenerate shape)";
            logHavokNotUsed(mShape->mFileName.value(), reason);
            survey("Havok collision of the files", "not used, " + reason);
            return;
        }

        // A model that is only collision, a wall that is not drawn, has no geometry to compare with
        if (mCompoundShape != nullptr)
        {
            const HavokFit fit = measureHavokFit(*havok, *mCompoundShape);
            survey("Size of the Havok shape over the size of the geometry that is drawn",
                havokSizeRatioAnswer(fit.mSizeRatio));
            survey("Distance of the centres, in sizes of the geometry that is drawn", havokOffsetAnswer(fit.mOffset));
            if (!fit.fits())
            {
                logHavokNotUsed(mShape->mFileName.value(), "its size and place are not those of its rendered geometry");
                survey("Havok collision of the files", "not used, its size and place are not those of the geometry");
                return;
            }
            survey("Havok collision of the files", "used, with geometry that is drawn");
        }
        else
        {
            survey("Havok collision of the files", "used, without geometry that is drawn");
        }

        Log(Debug::Verbose) << "Havok collision of " << mShape->mFileName << ": " << havok->getNumChildShapes()
                            << " shapes";
        mCompoundShape = std::move(havok);
        mShape->mAnimatedShapes.clear();
        // The hash names the shape in the navigation mesh database, which keeps tiles that were made for another shape
        // of the same file (the rendered geometry, from before or with the setting off) apart from these.
        mShape->mFileHash += sHavokHashSuffix;
    }

    bool BulletNifLoader::collectHavokBodies(const Nif::NiAVObject& node, const Nif::Parent* parent, bool animated,
        btCompoundShape& compound, HavokBodies& bodies, std::string& unsupported)
    {
        if (node.mRecordType == Nif::RC_NiCollisionSwitch && !node.collisionActive())
            return true;

        // What is for avoiding is not an obstacle
        if (node.mRecordType == Nif::RC_AvoidNode)
            return true;

        bool supported = true;
        animated = animated || hasMovingController(node);

        if (!node.mCollision.empty())
        {
            const auto* object = dynamic_cast<const Nif::bhkCollisionObject*>(node.mCollision.getPtr());
            const Nif::bhkRigidBody* body = nullptr;
            if (object == nullptr)
            {
                survey("Collision objects", node.mCollision->mRecordName + ", not a bhkCollisionObject");
            }
            else
            {
                survey("Collision objects",
                    object->mRecordName + ", flags " + havokNumberAnswer(object->mFlags)
                        + ((object->mFlags & sCollisionObjectActive) != 0 ? " (active)" : " (not active)"));
                // A collision object that is not active (bit 0 of its flags) has its collision turned off
                if ((object->mFlags & sCollisionObjectActive) != 0)
                {
                    if (object->mBody.empty())
                        survey("Bodies of the active collision objects", "none");
                    else
                    {
                        survey("Bodies of the active collision objects", object->mBody->mRecordName);
                        body = dynamic_cast<const Nif::bhkRigidBody*>(object->mBody.getPtr());
                    }
                }
            }

            // Phantoms are volumes that detect what is in them, and make no obstacle
            if (body != nullptr && body->mShape.empty())
                survey("Rigid bodies", "without a shape");
            if (body != nullptr && !body->mShape.empty())
            {
                bodies = std::max(bodies, HavokBodies::NotSolid);
                // A body that only reports what touches it, or does nothing when it is touched, stops nothing
                const bool stopsWhatTouchesIt = body->mInfo.mResponseType != Nif::HkResponseType::Response_Reporting
                    && body->mInfo.mResponseType != Nif::HkResponseType::Response_None;
                if (mHavokSurvey != nullptr)
                {
                    survey("Rigid bodies", body->mRecordName);
                    survey("Rigid bodies by layer",
                        havokNumberAnswer(body->mHavokFilter.mLayer)
                            + (isSolidHavokLayer(body->mHavokFilter.mLayer) ? " (solid)" : " (not solid)"));
                    survey("Rigid bodies by flags of the filter",
                        havokNumberAnswer(body->mHavokFilter.mFlags)
                            + ((body->mHavokFilter.mFlags & 0x40) != 0 ? " (no collision)" : ""));
                    survey("Rigid bodies by response", responseTypeName(body->mInfo.mResponseType));
                    survey("Rigid bodies by motion type", motionTypeName(body->mInfo.mMotionType));
                    survey("Rigid bodies by shape", body->mShape->mRecordName);
                    survey("Rigid bodies that stop what walks",
                        isSolidHavokFilter(body->mHavokFilter) && stopsWhatTouchesIt ? "yes" : "no");
                }
                if (isSolidHavokFilter(body->mHavokFilter) && stopsWhatTouchesIt)
                {
                    bodies = HavokBodies::Solid;
                    // What is not read is not used, and a survey goes on to count the bodies that come after it
                    std::string failure;
                    // The shape moves with the node, and where the node is when it does is not what this knows
                    if (animated)
                    {
                        failure = "a body that moves";
                    }
                    else
                    {
                        // Only a bhkRigidBodyT has the transform (the translation and rotation of a bhkRigidBody are
                        // not used). A Havok quaternion turns a vector as an OSG one, and a body is placed by it and
                        // then by its translation, which is in Havok units
                        osg::Matrixf bodyTransform;
                        if (body->mRecordType == Nif::RC_bhkRigidBodyT)
                        {
                            const osg::Vec4f& translation = body->mInfo.mTranslation;
                            bodyTransform = osg::Matrixf::rotate(body->mInfo.mRotation)
                                * osg::Matrixf::translate(
                                    osg::Vec3f(translation.x(), translation.y(), translation.z()) * sHavokScale);
                        }

                        std::vector<HavokPiece> pieces;
                        failure = convertHavokShape(body->mShape.get(), bodyTransform, pieces);
                        if (failure.empty())
                        {
                            const osg::Matrixf nodeTransform = getWorldTransform(node, parent);
                            for (HavokPiece& piece : pieces)
                                addChildShape(std::move(piece.mShape), piece.mTransform * nodeTransform, compound);
                        }
                    }

                    if (!failure.empty())
                    {
                        if (unsupported.empty())
                            unsupported = std::move(failure);
                        if (mHavokSurvey == nullptr)
                            return false;
                        supported = false;
                    }
                }
            }
        }

        if (const Nif::NiNode* ninode = dynamic_cast<const Nif::NiNode*>(&node))
        {
            const Nif::Parent currentParent{ *ninode, parent };
            for (const auto& child : ninode->mChildren)
            {
                if (!child.empty()
                    && !collectHavokBodies(child.get(), &currentParent, animated, compound, bodies, unsupported))
                {
                    if (mHavokSurvey == nullptr)
                        return false;
                    supported = false;
                }
                // The same children as the rendered geometry has: the first one of a switch
                if (node.mRecordType == Nif::RC_NiSwitchNode || node.mRecordType == Nif::RC_NiFltAnimationNode)
                    break;
            }
        }

        return supported;
    }

} // namespace NifBullet

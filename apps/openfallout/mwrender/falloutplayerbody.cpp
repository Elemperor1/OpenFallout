#include "falloutplayerbody.hpp"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <sstream>
#include <utility>

#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Transform>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadnpc.hpp>

#include "../mwbase/environment.hpp"
#include "../mwclass/esm4npc.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/manualref.hpp"

#include "esm4npcanimation.hpp"
#include "npcanimation.hpp"
#include "vismask.hpp"

namespace OFRender
{
    namespace
    {
        /// Where `node` stands in the skeleton of the arms: its transform from the node that the arms hang from down
        /// (the transforms above that one, which put the arms where the camera is, are left out)
        osg::Matrix poseInArms(const osg::Node& node, const osg::Node& armsRoot)
        {
            osg::Matrix result;
            const osg::NodePathList paths = node.getParentalNodePaths();
            for (const osg::NodePath& path : paths)
            {
                bool below = false;
                for (const osg::Node* step : path)
                {
                    if (below)
                    {
                        if (const osg::Transform* transform = step->asTransform())
                            transform->computeLocalToWorldMatrix(result, nullptr);
                    }
                    else if (step == &armsRoot)
                        below = true;
                }
                if (below)
                    break;
            }
            return result;
        }

        std::string describe(const osg::Vec3d& v)
        {
            std::ostringstream out;
            out.precision(1);
            out << std::fixed << "(" << v.x() << ", " << v.y() << ", " << v.z() << ")";
            return out.str();
        }

        /// "x y z yaw pitch roll" of the environment variable: a place (units) and a turn (degrees) in the frame of the
        /// camera, where X is to the right, Y ahead and Z up
        void armsOffsetFromEnvironment(osg::Vec3f& offset, osg::Quat& turn)
        {
            offset = osg::Vec3f();
            turn = osg::Quat();
            const char* text = std::getenv("OPENFALLOUT_FIRSTPERSON_ARMS");
            if (text == nullptr)
                return;
            std::istringstream in(text);
            double values[6] = { 0, 0, 0, 0, 0, 0 };
            for (double& value : values)
                if (!(in >> value))
                    break;
            offset = osg::Vec3f(values[0], values[1], values[2]);
            const double toRadians = osg::PI / 180.0;
            turn = osg::Quat(values[5] * toRadians, osg::Vec3d(0, 1, 0)) * osg::Quat(values[4] * toRadians, osg::Vec3d(1, 0, 0))
                * osg::Quat(values[3] * toRadians, osg::Vec3d(0, 0, 1));
            Log(Debug::Info) << "Player arms: moved from the camera by OPENFALLOUT_FIRSTPERSON_ARMS (" << text << ")";
        }
    }

    FalloutPlayerBody::FalloutPlayerBody(osg::ref_ptr<osg::Group> parent, osg::ref_ptr<osg::Group> sceneRoot,
        Resource::ResourceSystem* resourceSystem, float firstPersonFieldOfView, RenderingManager* renderingManager)
        : mParent(std::move(parent))
        , mSceneRoot(std::move(sceneRoot))
        , mResourceSystem(resourceSystem)
        , mFirstPersonFieldOfView(firstPersonFieldOfView)
        , mRenderingManager(renderingManager)
    {
        armsOffsetFromEnvironment(mArmsOffset, mArmsTurn);
    }

    FalloutPlayerBody::~FalloutPlayerBody()
    {
        clear();
    }

    void FalloutPlayerBody::dropBody()
    {
        if (mAnimation != nullptr)
            mAnimation->removeFromScene();
        mAnimation = nullptr;
        if (mRoot != nullptr && mParent != nullptr)
            mParent->removeChild(mRoot);
        mRoot = nullptr;
        mBodyHeld = Held();
    }

    void FalloutPlayerBody::dropArms()
    {
        if (mArms != nullptr)
            mArms->removeFromScene();
        mArms = nullptr;
        if (mArmsRoot != nullptr && mSceneRoot != nullptr)
            mSceneRoot->removeChild(mArmsRoot);
        mArmsRoot = nullptr;
        mArmsHeld = Held();
        mArmsFrames = 0;
    }

    void FalloutPlayerBody::clear()
    {
        // The animations first: they need the ref, which needs the record
        dropBody();
        dropArms();
        mRef.reset();
        mRecord.reset();
        mPose = 0;
        mBodyFailed = false;
        mArmsFailed = false;
    }

    bool FalloutPlayerBody::set(const ESM::RefId& templateId, const ESM4::Npc& record,
        const std::vector<const ESM4::Armor*>& armor, const OFWorld::Ptr& player, std::uint64_t revision)
    {
        clear();
        mRevision = revision;
        mPlayer = player;
        const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();
        if (store == nullptr || mParent == nullptr)
            return false;
        try
        {
            mRecord = std::make_unique<ESM4::Npc>(record);
            mRef = std::make_unique<OFWorld::ManualRef>(*store, templateId);
            const OFWorld::Ptr& ptr = mRef->getPtr();
            OFClass::ESM4Npc::setLook(ptr, *mRecord);
            OFClass::ESM4Npc::setWornArmor(ptr, armor);
        }
        catch (const std::exception& e)
        {
            Log(Debug::Error) << "Player body: " << e.what();
            clear();
            return false;
        }
        // The body is made even when it is not drawn (it keeps moving for the sounds of its steps); the arms only when
        // they show
        buildBody();
        if (mArmsVisible)
            buildArms();
        return mAnimation != nullptr || mArms != nullptr;
    }

    void FalloutPlayerBody::buildBody()
    {
        if (mAnimation != nullptr || mRef == nullptr || mParent == nullptr || mBodyFailed)
            return;
        try
        {
            const OFWorld::Ptr& ptr = mRef->getPtr();
            mRoot = new osg::Group;
            mRoot->setName("Player Body");
            mRoot->setNodeMask(mVisible ? ~0u : static_cast<unsigned>(Mask_UpdateVisitor));
            mParent->addChild(mRoot);
            mAnimation = new ESM4NpcAnimation(ptr, mRoot, mResourceSystem);
            // The character of the data stands still; the body walks as the player does
            mAnimation->setMotionSource(mPlayer);
            if (mPose != 0)
                mAnimation->setPose(mPose);
        }
        catch (const std::exception& e)
        {
            Log(Debug::Error) << "Player body: " << e.what();
            dropBody();
            mBodyFailed = true;
            return;
        }
        Log(Debug::Info) << "Player body: made " << (mVisible ? "for the third person camera" : "(not drawn: the camera is in the first person)");
        syncWeapon();
    }

    void FalloutPlayerBody::buildArms()
    {
        if (mArms != nullptr || mRef == nullptr || mSceneRoot == nullptr || mArmsFailed)
            return;
        try
        {
            const OFWorld::Ptr& ptr = mRef->getPtr();
            // The node that follows the camera: drawn in its own render bin after the world, with the field of view of the
            // first person, as the arms of the other games of the engine are
            mArmsRoot = new osg::MatrixTransform;
            mArmsRoot->setName("Player Arms");
            mArmsRoot->setNodeMask(mArmsVisible ? static_cast<unsigned>(Mask_FirstPerson) : 0u);
            setFirstPersonRenderBin(*mArmsRoot);
            addFirstPersonFieldOfView(*mArmsRoot, mFirstPersonFieldOfView, mRenderingManager);
            mSceneRoot->addChild(mArmsRoot);
            mArms = new ESM4NpcAnimation(ptr, mArmsRoot, mResourceSystem, true);
            // Where the eye is in the skeleton of the arms: the node the games put the camera on
            mArmsEye = osg::Vec3f();
            if (const osg::Node* camera = mArms->getNode("Camera1st"))
                mArmsEye = poseInArms(*camera, *mArmsRoot).getTrans();
            else
                Log(Debug::Warning) << "Player arms: the skeleton of the first person has no Camera1st node";
        }
        catch (const std::exception& e)
        {
            Log(Debug::Error) << "Player arms: " << e.what();
            dropArms();
            mArmsFailed = true;
            return;
        }
        Log(Debug::Info) << "Player arms: made for the first person camera, eye at " << describe(mArmsEye);
        syncWeapon();
    }

    void FalloutPlayerBody::setVisible(bool visible)
    {
        mVisible = visible;
        buildBody();
        // A body that is not drawn still goes on moving, for the sounds of its steps (Mask_UpdateVisitor is the mask of
        // the traversal that updates it, which the cull and the picking do not have)
        if (mRoot != nullptr)
            mRoot->setNodeMask(visible ? ~0u : static_cast<unsigned>(Mask_UpdateVisitor));
    }

    void FalloutPlayerBody::setArmsVisible(bool visible)
    {
        mArmsVisible = visible;
        FalloutActorAnimation::setFirstPersonArmsShown(visible);
        if (visible)
            buildArms();
        if (mArmsRoot != nullptr)
            mArmsRoot->setNodeMask(visible ? static_cast<unsigned>(Mask_FirstPerson) : 0u);
    }

    void FalloutPlayerBody::placeArms(const osg::Vec3d& cameraPosition, const osg::Quat& cameraOrientation)
    {
        if (mArmsRoot == nullptr || !mArmsVisible)
            return;
        // The skeleton of the first person looks along the same axes as the camera does (ahead along Y, up along Z), with
        // its Camera1st node as the eye: it is turned as the camera is and put so that the eye is at the camera
        const osg::Quat turn = cameraOrientation * mArmsTurn;
        const osg::Vec3d place = cameraPosition + cameraOrientation * osg::Vec3d(mArmsOffset) - turn * osg::Vec3d(mArmsEye);
        mArmsRoot->setMatrix(osg::Matrix::rotate(turn) * osg::Matrix::translate(place));
        ++mArmsFrames;
        // Two looks in the log (a pose that has settled, and a later one) for the first few times the arms are made
        static unsigned logged = 0;
        if ((mArmsFrames == 90 || mArmsFrames == 600) && logged < 8)
        {
            ++logged;
            logArms();
        }
    }

    void FalloutPlayerBody::logArms()
    {
        if (mArms == nullptr || mArmsRoot == nullptr)
            return;
        std::ostringstream out;
        out << "Player arms: after " << mArmsFrames << " frames, in the skeleton (eye " << describe(mArmsEye) << ")";
        for (const char* name : { "Camera1st", "Bip01 R Hand", "Bip01 L Hand", "Weapon" })
        {
            const osg::Node* node = mArms->getNode(name);
            if (node == nullptr)
            {
                out << ", " << name << " none";
                continue;
            }
            const osg::Vec3d at = poseInArms(*node, *mArmsRoot).getTrans();
            out << ", " << name << " " << describe(at) << " (from the eye " << describe(at - osg::Vec3d(mArmsEye)) << ")";
        }
        out << ", weapon " << (mArmsHeld.mDrawn ? "out" : "away") << ", node mask " << mArmsRoot->getNodeMask();
        Log(Debug::Info) << out.str();
    }

    void FalloutPlayerBody::die()
    {
        if (mAnimation != nullptr)
            mAnimation->die(false);
    }

    bool FalloutPlayerBody::isDown() const
    {
        return mAnimation != nullptr && mAnimation->isDown();
    }

    void FalloutPlayerBody::syncWeapon(ESM4NpcAnimation& animation, Held& held, bool drawn, std::uint32_t weapon,
        const std::string& model, int animationType)
    {
        if (drawn == held.mDrawn && (!drawn || weapon == held.mWeapon))
            return;
        held.mDrawn = drawn;
        held.mWeapon = weapon;
        if (drawn)
            animation.drawWeapon(model, animationType);
        else
            animation.holsterWeapon();
    }

    void FalloutPlayerBody::syncWeapon()
    {
        if (mAnimation != nullptr)
            syncWeapon(*mAnimation, mBodyHeld, mDrawn, mWeapon, mWeaponModel, mWeaponType);
        if (mArms != nullptr)
            syncWeapon(*mArms, mArmsHeld, mDrawn, mWeapon, mWeaponModel, mWeaponType);
    }

    void FalloutPlayerBody::setWeapon(bool drawn, std::uint32_t weapon, const std::string& model, int animationType)
    {
        mDrawn = drawn;
        mWeapon = weapon;
        mWeaponModel = model;
        mWeaponType = animationType;
        syncWeapon();
    }

    void FalloutPlayerBody::attack()
    {
        if (mAnimation != nullptr)
            mAnimation->attack();
        if (mArms != nullptr && mArmsVisible)
            mArms->attack();
    }

    void FalloutPlayerBody::setPose(int entryKind)
    {
        // Asked for every frame, as it is for the characters of the world: the animation keeps what it has, and asks
        // again for a pose that it could not start yet
        mPose = entryKind;
        if (mAnimation != nullptr)
            mAnimation->setPose(entryKind);
    }
}

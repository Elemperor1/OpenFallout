#include "weaponanimation.hpp"

#include <osg/MatrixTransform>

#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"

#include "../mwmechanics/combat.hpp"
#include "../mwmechanics/weapontype.hpp"

#include "animation.hpp"
#include "rotatecontroller.hpp"

namespace OFRender
{

    float WeaponAnimationTime::getValue(osg::NodeVisitor*)
    {
        if (mWeaponGroup.empty())
            return 0;

        float current = mAnimation->getCurrentTime(mWeaponGroup);
        if (current == -1)
            return 0;
        return current - mStartTime;
    }

    void WeaponAnimationTime::setGroup(const std::string& group, bool relativeTime)
    {
        mWeaponGroup = group;
        mRelativeTime = relativeTime;

        if (mRelativeTime)
            mStartTime = mAnimation->getStartTime(mWeaponGroup);
        else
            mStartTime = 0;
    }

    void WeaponAnimationTime::updateStartTime()
    {
        setGroup(mWeaponGroup, mRelativeTime);
    }

    WeaponAnimation::WeaponAnimation()
        : mPitchFactor(0)
    {
    }

    WeaponAnimation::~WeaponAnimation() {}

    void WeaponAnimation::attachArrow(const OFWorld::Ptr& actor)
    {
        const OFWorld::InventoryStore& inv = actor.getClass().getInventoryStore(actor);
        OFWorld::ConstContainerStoreIterator weaponSlot = inv.getSlot(OFWorld::InventoryStore::Slot_CarriedRight);
        if (weaponSlot == inv.end())
            return;
        if (weaponSlot->getType() != ESM::Weapon::sRecordId)
            return;

        const ESM::RefId type = weaponSlot->get<ESM::Weapon>()->mBase->mData.mType;
        ESM::WeaponType::Class weapclass = OFMechanics::getWeaponType(type)->mWeaponClass;
        if (weapclass == ESM::WeaponType::Thrown)
        {
            const auto& soundid = weaponSlot->getClass().getUpSoundId(*weaponSlot);
            if (!soundid.empty())
            {
                OFBase::SoundManager* sndMgr = OFBase::Environment::get().getSoundManager();
                sndMgr->playSound3D(actor, soundid, 1.0f, 1.0f);
            }
            showWeapon(true);
        }
        else if (weapclass == ESM::WeaponType::Ranged)
        {
            osg::Group* parent = getArrowBone();
            if (!parent)
                return;

            OFWorld::ConstContainerStoreIterator ammo = inv.getSlot(OFWorld::InventoryStore::Slot_Ammunition);
            if (ammo == inv.end())
                return;
            VFS::Path::Normalized model(ammo->getClass().getCorrectedModel(*ammo));

            osg::ref_ptr<osg::Node> arrow = getResourceSystem()->getSceneManager()->getInstance(model, parent);

            mAmmunition = std::make_unique<PartHolder>(arrow, model);
        }
    }

    void WeaponAnimation::detachArrow(OFWorld::Ptr actor)
    {
        mAmmunition.reset();
    }

    void WeaponAnimation::releaseArrow(OFWorld::Ptr actor, float attackStrength, float attackWindUp)
    {
        OFWorld::InventoryStore& inv = actor.getClass().getInventoryStore(actor);
        OFWorld::ContainerStoreIterator weapon = inv.getSlot(OFWorld::InventoryStore::Slot_CarriedRight);
        if (weapon == inv.end())
            return;
        if (weapon->getType() != ESM::Weapon::sRecordId)
            return;

        // The orientation of the launched projectile. Always the same as the actor orientation, even if the ArrowBone's
        // orientation dictates otherwise.
        osg::Quat orient = osg::Quat(actor.getRefData().getPosition().rot[0], osg::Vec3f(-1, 0, 0))
            * osg::Quat(actor.getRefData().getPosition().rot[2], osg::Vec3f(0, 0, -1));

        const OFWorld::Store<ESM::GameSetting>& gmst
            = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

        OFMechanics::applyFatigueLoss(actor, *weapon, attackStrength);

        if (OFMechanics::getWeaponType(weapon->get<ESM::Weapon>()->mBase->mData.mType)->mWeaponClass
            == ESM::WeaponType::Thrown)
        {
            // Thrown weapons get detached now
            osg::Node* weaponNode = getWeaponNode();
            if (!weaponNode)
                return;
            osg::NodePathList nodepaths = weaponNode->getParentalNodePaths();
            if (nodepaths.empty())
                return;
            osg::Vec3f launchPos = osg::computeLocalToWorld(nodepaths[0]).getTrans();

            float fThrownWeaponMinSpeed = gmst.find("fThrownWeaponMinSpeed")->mValue.getFloat();
            float fThrownWeaponMaxSpeed = gmst.find("fThrownWeaponMaxSpeed")->mValue.getFloat();
            float speed = fThrownWeaponMinSpeed + (fThrownWeaponMaxSpeed - fThrownWeaponMinSpeed) * attackStrength;

            OFWorld::Ptr weaponPtr = *weapon;
            OFBase::Environment::get().getWorld()->launchProjectile(
                actor, weaponPtr, launchPos, orient, weaponPtr, speed, attackStrength, attackWindUp);

            showWeapon(false);

            inv.remove(*weapon, 1);
        }
        else
        {
            // With bows and crossbows only the used arrow/bolt gets detached
            OFWorld::ContainerStoreIterator ammo = inv.getSlot(OFWorld::InventoryStore::Slot_Ammunition);
            if (ammo == inv.end())
                return;

            if (!mAmmunition)
                return;

            osg::ref_ptr<osg::Node> ammoNode = mAmmunition->getNode();
            osg::NodePathList nodepaths = ammoNode->getParentalNodePaths();
            if (nodepaths.empty())
                return;
            osg::Vec3f launchPos = osg::computeLocalToWorld(nodepaths[0]).getTrans();

            float fProjectileMinSpeed = gmst.find("fProjectileMinSpeed")->mValue.getFloat();
            float fProjectileMaxSpeed = gmst.find("fProjectileMaxSpeed")->mValue.getFloat();
            float speed = fProjectileMinSpeed + (fProjectileMaxSpeed - fProjectileMinSpeed) * attackStrength;

            OFWorld::Ptr weaponPtr = *weapon;
            OFWorld::Ptr ammoPtr = *ammo;
            OFBase::Environment::get().getWorld()->launchProjectile(
                actor, ammoPtr, launchPos, orient, weaponPtr, speed, attackStrength, attackWindUp);

            inv.remove(ammoPtr, 1);
            mAmmunition.reset();
        }
    }

    void WeaponAnimation::addControllers(const Animation::NodeMap& nodes,
        std::vector<std::pair<osg::ref_ptr<osg::Node>, osg::ref_ptr<osg::Callback>>>& map, osg::Node* objectRoot)
    {
        for (int i = 0; i < 2; ++i)
        {
            mSpineControllers[i] = nullptr;

            Animation::NodeMap::const_iterator found = nodes.find(i == 0 ? "bip01 spine1" : "bip01 spine2");
            if (found != nodes.end())
            {
                osg::Node* node = found->second;
                mSpineControllers[i] = new RotateController(objectRoot);
                node->addUpdateCallback(mSpineControllers[i]);
                map.emplace_back(node, mSpineControllers[i]);
            }
        }
    }

    void WeaponAnimation::deleteControllers()
    {
        for (int i = 0; i < 2; ++i)
            mSpineControllers[i] = nullptr;
    }

    void WeaponAnimation::configureControllers(float characterPitchRadians)
    {
        if (mPitchFactor == 0.f || characterPitchRadians == 0.f)
        {
            setControllerEnabled(false);
            return;
        }

        float pitch = characterPitchRadians * mPitchFactor;
        osg::Quat rotate(pitch / 2, osg::Vec3f(-1, 0, 0));
        setControllerRotate(rotate);
        setControllerEnabled(true);
    }

    void WeaponAnimation::setControllerRotate(const osg::Quat& rotate)
    {
        for (int i = 0; i < 2; ++i)
            if (mSpineControllers[i])
                mSpineControllers[i]->setRotate(rotate);
    }

    void WeaponAnimation::setControllerEnabled(bool enabled)
    {
        for (int i = 0; i < 2; ++i)
            if (mSpineControllers[i])
                mSpineControllers[i]->setEnabled(enabled);
    }

}

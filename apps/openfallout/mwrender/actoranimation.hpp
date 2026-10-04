#ifndef GAME_RENDER_ACTORANIMATION_H
#define GAME_RENDER_ACTORANIMATION_H

#include <map>

#include <osg/ref_ptr>

#include <components/vfs/pathutil.hpp>

#include "../mwworld/containerstore.hpp"

#include "animation.hpp"

namespace osg
{
    class Node;
}

namespace OFWorld
{
    class ConstPtr;
}

namespace SceneUtil
{
    class LightSource;
    class LightListCallback;
}

namespace OFRender
{

    class ActorAnimation : public Animation, public OFWorld::ContainerStoreListener
    {
    public:
        ActorAnimation(
            const OFWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem);
        virtual ~ActorAnimation();

        void itemAdded(const OFWorld::ConstPtr& item, int count) override;
        void itemRemoved(const OFWorld::ConstPtr& item, int count) override;
        virtual bool isArrowAttached() const { return false; }
        bool useShieldAnimations() const override;
        bool updateCarriedLeftVisible(ESM::RefId weaptype) const override;

        void removeFromScene() override;

    protected:
        osg::Group* getBoneByName(std::string_view boneName) const;
        void updateHolsteredWeapon(bool showHolsteredWeapons);
        void updateHolsteredShield(bool showCarriedLeft);
        void updateQuiver();
        VFS::Path::Normalized getShieldMesh(const OFWorld::ConstPtr& shield, bool female) const;
        virtual std::string getSheathedShieldMesh(const OFWorld::ConstPtr& shield) const;
        std::string_view getHolsteredWeaponBoneName(const OFWorld::ConstPtr& weapon);

        PartHolderPtr attachMesh(
            VFS::Path::NormalizedView model, std::string_view bonename, const osg::Vec4f* glowColor = nullptr);

        osg::ref_ptr<osg::Node> attach(
            VFS::Path::NormalizedView model, std::string_view bonename, std::string_view bonefilter, bool isLight);

        PartHolderPtr mScabbard;
        PartHolderPtr mHolsteredShield;

    private:
        void addHiddenItemLight(const OFWorld::ConstPtr& item, const ESM::Light* esmLight);
        void removeHiddenItemLight(const OFWorld::ConstPtr& item);
        void resetControllers(osg::Node* node);
        void removeFromSceneImpl();

        typedef std::map<OFWorld::ConstPtr, osg::ref_ptr<SceneUtil::LightSource>> ItemLightMap;
        ItemLightMap mItemLights;
    };

}

#endif

#ifndef OPENFALLOUT_APPS_OPENFALLOUT_MWRENDER_FALLOUTPLAYERBODY_H
#define OPENFALLOUT_APPS_OPENFALLOUT_MWRENDER_FALLOUTPLAYERBODY_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <osg/Matrix>
#include <osg/Quat>
#include <osg/ref_ptr>

#include <components/esm/refid.hpp>

#include "../mwworld/ptr.hpp"

namespace osg
{
    class Group;
    class MatrixTransform;
}

namespace ESM4
{
    struct Armor;
    struct Npc;
}

namespace OFWorld
{
    class ManualRef;
}

namespace Resource
{
    class ResourceSystem;
}

namespace OFRender
{
    class ESM4NpcAnimation;
    class RenderingManager;

    /// The body of the player in Fallout 3 and New Vegas, which shows when the camera is not in the player's eyes. The
    /// player of the engine is a record of the Morrowind kind with no model of Fallout, so what the games draw there
    /// (the skeleton, body, head, hair and armor of the player's race and sex) is a character of Fallout made from the
    /// look the player has picked and the armor it wears, standing at the player's place, turned as the player is and
    /// walking and running with the player's speed. It is made when it is first wanted and again when the look or the
    /// armor changes.
    ///
    /// The same character is also the arms that the player sees through their own eyes: the skeleton of the first
    /// person with the hands on it, which hangs in front of the camera and holds the weapon the player has drawn. Both are
    /// made when they are first wanted (the body is not made for a player who only ever looks out of their own eyes).
    class FalloutPlayerBody
    {
    public:
        /// `parent` is the node of the player in the scene: the body stands in it, so it goes where the player goes.
        /// `sceneRoot` is the root of the scene, where the arms hang (they go where the camera goes, not where the player
        /// stands), drawn with the field of view of the first person `firstPersonFieldOfView` (degrees).
        FalloutPlayerBody(osg::ref_ptr<osg::Group> parent, osg::ref_ptr<osg::Group> sceneRoot,
            Resource::ResourceSystem* resourceSystem, float firstPersonFieldOfView, RenderingManager* renderingManager);
        ~FalloutPlayerBody();

        FalloutPlayerBody(const FalloutPlayerBody&) = delete;
        FalloutPlayerBody& operator=(const FalloutPlayerBody&) = delete;

        /// Makes the body again from `record` (copied; the character that the skeleton and body come from is
        /// `templateId`), wearing `armor` (which must outlive it), moved by the speed of `player`. `revision` is the
        /// number the caller gave to this look (see `revision`). False when it cannot be made, and then nothing shows.
        bool set(const ESM::RefId& templateId, const ESM4::Npc& record, const std::vector<const ESM4::Armor*>& armor,
            const OFWorld::Ptr& player, std::uint64_t revision);

        /// The number of the look that the body was made from
        std::uint64_t revision() const { return mRevision; }
        bool isBuilt() const { return mAnimation != nullptr; }
        /// Whether the arms of the first person are made
        bool hasArms() const { return mArms != nullptr; }

        /// Shows the body or hides it (it is hidden in the player's eyes, where the player would look out of its head)
        void setVisible(bool visible);
        bool isVisible() const { return mVisible; }

        /// Shows the arms of the first person or hides them: they show when the player looks out of their own eyes with a
        /// weapon in the hands. They are made the first time they are wanted.
        void setArmsVisible(bool visible);
        bool isArmsVisible() const { return mArmsVisible; }

        /// Puts the arms where the camera is, said every frame while they show: the point of their skeleton where the
        /// games have the eye is the place of the camera, and they turn with it. The environment variable
        /// `OPENFALLOUT_FIRSTPERSON_ARMS` ("x y z yaw pitch roll": units and degrees, in the frame of the camera) moves
        /// them from there, to find where they should be.
        void placeArms(const osg::Vec3d& cameraPosition, const osg::Quat& cameraOrientation);

        /// The body sits, lies or leans in furniture whose entry is of this kind (0: it does not); said every frame
        void setPose(int entryKind);

        /// The player has died: the body falls as a character of the world does, and lies there until it is made again
        void die();
        /// Whether the body has fallen
        bool isDown() const;

        /// The weapon in the hands, said every frame: with `drawn` the weapon `weapon` (a form id; its model and the
        /// animation type of its record, -1 for bare hands) is out in the hand and held ready, as it is for the
        /// characters of the world; without it the hands are empty and down. Only a change does anything.
        void setWeapon(bool drawn, std::uint32_t weapon, const std::string& model, int animationType);
        /// One shot or blow with it (the arms of the first person do it too, when they are made)
        void attack();

        /// Takes the body out of the scene
        void clear();

    private:
        /// What one animation (the body, or the arms) holds: whether a weapon is out and which
        struct Held
        {
            bool mDrawn = false;
            std::uint32_t mWeapon = 0;
        };

        /// Makes the body, or the arms, from the record that `set` was given (nothing when they are made already, when
        /// there is no record, or when they could not be made from it)
        void buildBody();
        void buildArms();
        /// Takes the body, or the arms, out of the scene
        void dropBody();
        void dropArms();
        /// Puts the weapon that is wanted in the hands of the body and the arms, where they do not hold it yet
        void syncWeapon();
        static void syncWeapon(ESM4NpcAnimation& animation, Held& held, bool drawn, std::uint32_t weapon,
            const std::string& model, int animationType);
        /// Says in the log where the camera and hands are in the skeleton of the first person's arms
        void logArms();

        osg::ref_ptr<osg::Group> mParent;
        osg::ref_ptr<osg::Group> mSceneRoot;
        Resource::ResourceSystem* mResourceSystem;
        float mFirstPersonFieldOfView;
        RenderingManager* mRenderingManager;
        osg::ref_ptr<osg::Group> mRoot;
        // In the order they are made, for they are taken down the other way: the animation needs the ref, which needs
        // the record the look is in
        std::unique_ptr<ESM4::Npc> mRecord;
        std::unique_ptr<OFWorld::ManualRef> mRef;
        osg::ref_ptr<ESM4NpcAnimation> mAnimation;
        OFWorld::Ptr mPlayer;
        std::uint64_t mRevision = 0;
        bool mVisible = false;
        int mPose = 0;
        /// What the hands are to hold: whether it is out, which weapon (the key of the change), its model and animation
        /// type; and what the body and the arms hold now (they are made at different times)
        bool mDrawn = false;
        std::uint32_t mWeapon = 0;
        std::string mWeaponModel;
        int mWeaponType = -1;
        Held mBodyHeld;
        Held mArmsHeld;
        /// Set when the body or the arms could not be made from the record, so that they are not tried every frame
        bool mBodyFailed = false;
        bool mArmsFailed = false;

        /// The arms of the first person: the node in the scene that follows the camera, the animation on it, whether they
        /// show, where the eye is in their skeleton, and how many frames they were placed (for the log)
        osg::ref_ptr<osg::MatrixTransform> mArmsRoot;
        osg::ref_ptr<ESM4NpcAnimation> mArms;
        bool mArmsVisible = false;
        osg::Vec3f mArmsEye;
        unsigned mArmsFrames = 0;
        /// How the arms are moved from the camera (see placeArms): a place and a turn, in the frame of the camera
        osg::Vec3f mArmsOffset;
        osg::Quat mArmsTurn;
    };
}

#endif

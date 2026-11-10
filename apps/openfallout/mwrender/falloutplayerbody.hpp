#ifndef OPENFALLOUT_APPS_OPENFALLOUT_MWRENDER_FALLOUTPLAYERBODY_H
#define OPENFALLOUT_APPS_OPENFALLOUT_MWRENDER_FALLOUTPLAYERBODY_H

#include <cstdint>
#include <memory>
#include <vector>

#include <osg/ref_ptr>

#include <components/esm/refid.hpp>

#include "../mwworld/ptr.hpp"

namespace osg
{
    class Group;
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

    /// The body of the player in Fallout 3 and New Vegas, which shows when the camera is not in the player's eyes. The
    /// player of the engine is a record of the Morrowind kind with no model of Fallout, so what the games draw there
    /// (the skeleton, body, head, hair and armor of the player's race and sex) is a character of Fallout made from the
    /// look the player has picked and the armor it wears, standing at the player's place, turned as the player is and
    /// walking and running with the player's speed. It is made when it is first wanted and again when the look or the
    /// armor changes.
    class FalloutPlayerBody
    {
    public:
        /// `parent` is the node of the player in the scene: the body stands in it, so it goes where the player goes.
        FalloutPlayerBody(osg::ref_ptr<osg::Group> parent, Resource::ResourceSystem* resourceSystem);
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

        /// Shows the body or hides it (it is hidden in the player's eyes, where the player would look out of its head)
        void setVisible(bool visible);
        bool isVisible() const { return mVisible; }

        /// The body sits, lies or leans in furniture whose entry is of this kind (0: it does not); said every frame
        void setPose(int entryKind);

        /// The player has died: the body falls as a character of the world does, and lies there until it is made again
        void die();
        /// Whether the body has fallen
        bool isDown() const;

        /// Takes the body out of the scene
        void clear();

    private:
        osg::ref_ptr<osg::Group> mParent;
        Resource::ResourceSystem* mResourceSystem;
        osg::ref_ptr<osg::Group> mRoot;
        // In the order they are made, for they are taken down the other way: the animation needs the ref, which needs
        // the record the look is in
        std::unique_ptr<ESM4::Npc> mRecord;
        std::unique_ptr<OFWorld::ManualRef> mRef;
        osg::ref_ptr<ESM4NpcAnimation> mAnimation;
        std::uint64_t mRevision = 0;
        bool mVisible = false;
        int mPose = 0;
    };
}

#endif

#ifndef OPENFALLOUT_APPS_OPENFALLOUT_MWRENDER_FACEPREVIEW_H
#define OPENFALLOUT_APPS_OPENFALLOUT_MWRENDER_FACEPREVIEW_H

#include <memory>
#include <string>

#include <osg/Vec3f>
#include <osg/ref_ptr>

#include <components/esm/refid.hpp>

namespace osg
{
    class Group;
    class NodeCallback;
}

namespace ESM4
{
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
    class MenuScene;

    /// The head that the screen of the sex, race and face shows (RaceSexMenu): a character of Fallout in a scene of the
    /// menus, made from a record that says how it looks and made again whenever the look changes. The games turn the
    /// player's own model under their camera; the player of this port has no model, so the head is a character that is
    /// made for the screen, from a character of the data that has the sex and race and the look written over it.
    class FacePreview
    {
    public:
        /// `name` is the model's name in the scene. `templateId` is the character the skeleton and body come from.
        FacePreview(MenuScene& scene, std::string name, Resource::ResourceSystem* resourceSystem);
        ~FacePreview();

        FacePreview(const FacePreview&) = delete;
        FacePreview& operator=(const FacePreview&) = delete;

        /// Makes the character again from this record (the record is copied). False when it cannot be made.
        bool set(const ESM::RefId& templateId, const ESM4::Npc& record);

        /// The camera looks at the head from `distance` units, turned `yaw` radians about the vertical (0 looks at the
        /// face of a character that stands facing +Y)
        void view(float yaw, float distance);

        /// The head's place in the scene (where the bone of the head is, and else the place a head would be)
        osg::Vec3f headPosition() const;

        /// Moves the animation of the character on
        void advance(float seconds);

    private:
        void clear();

        MenuScene& mScene;
        std::string mName;
        Resource::ResourceSystem* mResourceSystem;
        osg::ref_ptr<osg::Group> mRoot;
        osg::ref_ptr<osg::NodeCallback> mUpdate;
        // In the order they are made, for they are taken down the other way: the animation needs the ref, which needs
        // the record the look is in
        std::unique_ptr<ESM4::Npc> mRecord;
        std::unique_ptr<OFWorld::ManualRef> mRef;
        osg::ref_ptr<ESM4NpcAnimation> mAnimation;
    };
}

#endif

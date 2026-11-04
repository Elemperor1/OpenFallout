#include "facepreview.hpp"

#include <cmath>

#include <osg/NodeCallback>
#include <osg/NodeVisitor>
#include <osg/Group>
#include <osg/MatrixTransform>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/sceneutil/visitor.hpp>

#include "../mwbase/environment.hpp"
#include "../mwclass/esm4npc.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/manualref.hpp"

#include "esm4npcanimation.hpp"
#include "menuscene.hpp"

namespace OFRender
{
    namespace
    {
        /// Where a head is when the skeleton has no bone for it (units above the feet of a character of 1.0 height)
        constexpr float defaultHeadHeight = 150.f;

        /// Runs the animation of the character with the time of the frames of the scene
        class AdvanceCallback : public osg::NodeCallback
        {
        public:
            explicit AdvanceCallback(FacePreview& preview)
                : mPreview(preview)
            {
            }

            void operator()(osg::Node* node, osg::NodeVisitor* visitor) override
            {
                if (const osg::FrameStamp* stamp = visitor->getFrameStamp())
                {
                    const double now = stamp->getSimulationTime();
                    if (mLast >= 0.0)
                        mPreview.advance(static_cast<float>(std::max(0.0, std::min(now - mLast, 0.25))));
                    mLast = now;
                }
                traverse(node, visitor);
            }

        private:
            FacePreview& mPreview;
            double mLast = -1.0;
        };
    }

    FacePreview::FacePreview(MenuScene& scene, std::string name, Resource::ResourceSystem* resourceSystem)
        : mScene(scene)
        , mName(std::move(name))
        , mResourceSystem(resourceSystem)
    {
    }

    FacePreview::~FacePreview()
    {
        clear();
    }

    void FacePreview::clear()
    {
        if (mRoot != nullptr && mUpdate != nullptr)
            mRoot->removeUpdateCallback(mUpdate);
        mUpdate = nullptr;
        if (mAnimation != nullptr)
            mAnimation->removeFromScene();
        mAnimation = nullptr;
        mScene.removeModel(mName);
        mRoot = nullptr;
        mRef.reset();
        mRecord.reset();
    }

    bool FacePreview::set(const ESM::RefId& templateId, const ESM4::Npc& record)
    {
        clear();
        const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();
        if (store == nullptr)
            return false;
        try
        {
            mRecord = std::make_unique<ESM4::Npc>(record);
            mRef = std::make_unique<OFWorld::ManualRef>(*store, templateId);
            const OFWorld::Ptr& ptr = mRef->getPtr();
            OFClass::ESM4Npc::setLook(ptr, *mRecord);
            mRoot = new osg::Group;
            mAnimation = new ESM4NpcAnimation(ptr, mRoot, mResourceSystem);
            mUpdate = new AdvanceCallback(*this);
            mRoot->addUpdateCallback(mUpdate);
        }
        catch (const std::exception& e)
        {
            Log(Debug::Error) << "Face preview: " << e.what();
            clear();
            return false;
        }
        return mScene.addNode(mName, mRoot);
    }

    void FacePreview::advance(float seconds)
    {
        if (mAnimation != nullptr)
            mAnimation->advance(seconds);
    }

    osg::Vec3f FacePreview::headPosition() const
    {
        if (mAnimation != nullptr)
        {
            if (const osg::Node* head = mAnimation->getNode("Bip01 Head"))
            {
                const osg::NodePathList paths = head->getParentalNodePaths();
                if (!paths.empty())
                {
                    // In the place of the character (its root is at the origin of the scene, so the head's place is the
                    // head's place in the world of the scene)
                    const osg::Matrixd toWorld = osg::computeLocalToWorld(paths.front());
                    const osg::Vec3d at = toWorld.getTrans();
                    // The bone is the base of the skull; the camera looks at the middle of the head with its hair, which is
                    // about this far above it (units of the game), so that the head is in the middle of its window
                    constexpr float headCentreAboveBone = 8.f;
                    if (at.length2() > 1.0)
                        return osg::Vec3f(at) + osg::Vec3f(0.f, 0.f, headCentreAboveBone);
                }
            }
        }
        return osg::Vec3f(0.f, 0.f, defaultHeadHeight);
    }

    void FacePreview::view(float yaw, float distance)
    {
        const osg::Vec3f head = headPosition();
        // The camera is on the turned axis through the head and looks at it, level, with +Z up in the picture
        const osg::Vec3f away(std::sin(yaw), std::cos(yaw), 0.f);
        mScene.setCamera(head + away * distance, -away, osg::Vec3f(0.f, 0.f, 1.f));
    }
}

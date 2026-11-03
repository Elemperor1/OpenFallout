#include "menuscene.hpp"

#include <algorithm>
#include <cmath>

#include <osg/BlendFunc>
#include <osg/Camera>
#include <osg/FrontFace>
#include <osg/MatrixTransform>
#include <osg/Texture2D>

#include <components/debug/debuglog.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/controller.hpp>
#include <components/sceneutil/depth.hpp>
#include <components/sceneutil/fog.hpp>
#include <components/sceneutil/keyframe.hpp>
#include <components/sceneutil/lightmanager.hpp>
#include <components/sceneutil/material.hpp>
#include <components/sceneutil/nodecallback.hpp>
#include <components/sceneutil/rtt.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/sceneutil/userdata.hpp>
#include <components/sceneutil/visitor.hpp>
#include <components/settings/values.hpp>
#include <components/stereo/multiview.hpp>

#include "premultipliedblend.hpp"
#include "util.hpp"
#include "vismask.hpp"

namespace OFRender
{
    namespace
    {
        /// The time that the controllers of a model read: the scene sets it, the clock of the game does not run it
        class SetTime : public SceneUtil::ControllerSource
        {
        public:
            float getValue(osg::NodeVisitor* /*nv*/) override { return mTime; }

            float mTime = 0.f;
        };

        /// The update traversal does not enter an RTTNode, but the controllers of the models are run by it
        class UpdateSubgraph : public SceneUtil::NodeCallback<UpdateSubgraph>
        {
        public:
            explicit UpdateSubgraph(osg::Node* subgraph)
                : mSubgraph(subgraph)
            {
            }

            void operator()(osg::Node* node, osg::NodeVisitor* nv)
            {
                mSubgraph->accept(*nv);
                traverse(node, nv);
            }

        private:
            osg::ref_ptr<osg::Node> mSubgraph;
        };

        const SceneUtil::EmbeddedSequences* findEmbeddedSequences(osg::Node& node)
        {
            if (const SceneUtil::EmbeddedSequences* found = SceneUtil::findUserData<SceneUtil::EmbeddedSequences>(node))
                return found;
            if (osg::Group* group = node.asGroup())
                for (unsigned int i = 0; i < group->getNumChildren(); ++i)
                    if (const SceneUtil::EmbeddedSequences* found = findEmbeddedSequences(*group->getChild(i)))
                        return found;
            return nullptr;
        }

        /// The first group of a holder (a holder made of a sequence has the one group, named after it) with the times
        /// of its start and stop keys
        bool sequenceOf(const SceneUtil::KeyframeHolder& holder, MenuScene::Sequence& out)
        {
            const auto& groups = holder.mTextKeys.getGroups();
            if (groups.empty())
                return false;
            out.mName = *groups.begin();
            const auto start = holder.mTextKeys.findGroupStart(out.mName);
            if (start == holder.mTextKeys.end())
                return false;
            out.mStart = start->first;
            out.mStop = start->first;
            const std::string stopKey = out.mName + ": stop";
            for (auto it = holder.mTextKeys.begin(); it != holder.mTextKeys.end(); ++it)
                if (it->second == stopKey)
                    out.mStop = it->first;
            return true;
        }
    }

    osg::Matrixf placement(const osg::Vec3f& translation, const osg::Vec3f& eulerDegrees, float scale)
    {
        const osg::Quat x(osg::DegreesToRadians(eulerDegrees.x()), osg::Vec3f(1, 0, 0));
        const osg::Quat y(osg::DegreesToRadians(eulerDegrees.y()), osg::Vec3f(0, 1, 0));
        const osg::Quat z(osg::DegreesToRadians(eulerDegrees.z()), osg::Vec3f(0, 0, 1));
        // The quaternion that turns by x, then y, then z (OSG's product applies its right factor first)
        const osg::Quat rotation = x * y * z;
        return osg::Matrixf::scale(scale, scale, scale) * osg::Matrixf::rotate(rotation)
            * osg::Matrixf::translate(translation);
    }

    osg::Matrixf placementFromRows(const osg::Vec3f& translation, const std::array<float, 9>& rows, float scale)
    {
        // The rows turn a column vector; OSG multiplies a row vector, with the transposed matrix
        const osg::Matrixf rotation(rows[0], rows[3], rows[6], 0.f, rows[1], rows[4], rows[7], 0.f, rows[2], rows[5],
            rows[8], 0.f, 0.f, 0.f, 0.f, 1.f);
        return osg::Matrixf::scale(scale, scale, scale) * rotation * osg::Matrixf::translate(translation);
    }

    osg::Matrixf upsideDown(const osg::Matrixf& projection)
    {
        // OSG multiplies a row vector: the second column makes the y of a point on the screen
        osg::Matrixf result = projection;
        for (int row = 0; row < 4; ++row)
            result(row, 1) = -result(row, 1);
        return result;
    }

    class MenuSceneRTTNode : public SceneUtil::RTTNode
    {
    public:
        MenuSceneRTTNode(uint32_t sizeX, uint32_t sizeY)
            : RTTNode(sizeX, sizeY, Settings::video().mAntialiasing, false, 0,
                  StereoAwareness::Unaware_MultiViewShaders, shouldAddMSAAIntermediateTarget())
        {
            setColorBufferInternalFormat(GL_RGBA);
            setDepthBufferInternalFormat(GL_DEPTH24_STENCIL8);
            updateProjection();
            osg::StateSet* stateset = mGroup->getOrCreateStateSet();
            stateset->addUniform(mProjectionUniform);
            // The projection is upside down (see upsideDown): the faces that look at the camera are the ones whose
            // corners go round the other way
            stateset->setAttributeAndModes(new osg::FrontFace(osg::FrontFace::CLOCKWISE), osg::StateAttribute::ON);
        }

        void setDefaults(osg::Camera* camera) override
        {
            camera->setName("MenuScene");
            camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
            camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT, osg::Camera::PIXEL_BUFFER_RTT);
            camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            camera->setViewport(0, 0, width(), height());
            camera->setRenderOrder(osg::Camera::PRE_RENDER);
            camera->setCullMask(~(Mask_UpdateVisitor));
            camera->setComputeNearFarMode(osg::Camera::DO_NOT_COMPUTE_NEAR_FAR);
            SceneUtil::setCameraClearDepth(camera);

            camera->setNodeMask(Mask_RenderToTexture);
            camera->addChild(mGroup);
        }

        void apply(osg::Camera* camera) override
        {
            camera->setClearColor(mClearColour);
            camera->setProjectionMatrix(mProjection);
            camera->setViewMatrix(mView);

            if (shouldDoTextureArray())
                Stereo::setMultiviewMatrices(mGroup->getOrCreateStateSet(), { mProjection, mProjection });
        }

        void setFrustum(float left, float right, float bottom, float top, float nearPlane, float farPlane)
        {
            mNear = nearPlane;
            mFar = farPlane;
            // The picture is centred on the camera's axis in all of the game's menu scenes that matter; an off-centre
            // frustum is made centred by its height and width
            const float frustumHeight = top - bottom;
            const float widthOnNear = right - left;
            mFovYDegrees = osg::RadiansToDegrees(2.f * std::atan(0.5f * frustumHeight / nearPlane));
            mAspectRatio = widthOnNear / frustumHeight;
            updateProjection();
        }

        void setView(const osg::Matrixf& view) { mView = view; }
        void setClearColour(const osg::Vec4f& colour) { mClearColour = colour; }

        float nearPlane() const { return mNear; }
        float farPlane() const { return mFar; }

        osg::ref_ptr<osg::Group> mGroup = new osg::Group;

    private:
        void updateProjection()
        {
            if (SceneUtil::AutoDepth::isReversed())
                mProjection = static_cast<osg::Matrixf>(
                    SceneUtil::getReversedZProjectionMatrixAsPerspective(mFovYDegrees, mAspectRatio, mNear, mFar));
            else
                mProjection = osg::Matrixf::perspective(mFovYDegrees, mAspectRatio, mNear, mFar);
            mProjection = upsideDown(mProjection);
            mProjectionUniform->set(mProjection);
        }

        float mFovYDegrees = 45.f;
        float mAspectRatio = 1.f;
        float mNear = 1.f;
        float mFar = 10000.f;
        osg::Matrixf mProjection;
        osg::Matrixf mView = osg::Matrixf::identity();
        osg::Vec4f mClearColour{ 0.f, 0.f, 0.f, 0.f };
        osg::ref_ptr<osg::Uniform> mProjectionUniform = new osg::Uniform("projectionMatrix", osg::Matrixf());
    };

    struct MenuScene::Model
    {
        osg::ref_ptr<osg::MatrixTransform> mRoot;
        osg::ref_ptr<osg::Node> mInstance;
        std::vector<Sequence> mSequences;
        std::vector<osg::ref_ptr<const SceneUtil::KeyframeHolder>> mHolders;
        SceneUtil::NodeMap mNodes;
        std::shared_ptr<SetTime> mTime = std::make_shared<SetTime>();
        // The controllers that are on the nodes now, and the sequence they belong to
        std::vector<std::pair<osg::ref_ptr<osg::Node>, osg::ref_ptr<osg::Callback>>> mActive;
        std::string mActiveSequence;
    };

    MenuScene::MenuScene(osg::Group* parent, Resource::ResourceSystem* resourceSystem, int width, int height)
        : mParent(parent)
        , mResourceSystem(resourceSystem)
        , mWidth(std::max(width, 1))
        , mHeight(std::max(height, 1))
    {
        mTextureStateSet = new osg::StateSet;
        mTextureStateSet->setAttribute(new osg::BlendFunc(osg::BlendFunc::ONE, osg::BlendFunc::ONE_MINUS_SRC_ALPHA));

        mRTTNode = new MenuSceneRTTNode(mWidth, mHeight);
        mRTTNode->setNodeMask(Mask_RenderToTexture);

        osg::ref_ptr<SceneUtil::LightManager> lightManager = new SceneUtil::LightManager(
            SceneUtil::LightSettings{
                .mClusteredLighting = Settings::shaders().mClusteredLighting,
                .mMaxLights = Settings::shaders().mMaxLights,
                .mMaximumLightDistance = Settings::shaders().mMaximumLightDistance,
                .mLightFadeStart = Settings::shaders().mLightFadeStart,
                .mLightRadiusMultiplier = Settings::shaders().mLightRadiusMultiplier,
                .mClusteredGridSize = { 1, 1, 1 },
                .mClusteredWorkGroupSize = 1,
            },
            resourceSystem);
        osg::ref_ptr<osg::StateSet> stateset = lightManager->getOrCreateStateSet();
        stateset->setDefine("FORCE_OPAQUE", "1", osg::StateAttribute::ON);
        stateset->setMode(GL_NORMALIZE, osg::StateAttribute::ON);
        stateset->setMode(GL_CULL_FACE, osg::StateAttribute::ON);
        osg::ref_ptr<SceneUtil::Material> defaultMat(new SceneUtil::Material);
        defaultMat->updateStateSet(stateset);
        stateset->setAttribute(defaultMat);

        SceneUtil::ShadowManager::instance().disableShadowsForStateSet(*stateset);
        SceneUtil::disableFog(*stateset, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

        // The loose uniforms that the shaders of the world depend on (the same as the character preview sets)
        stateset->addUniform(new osg::Uniform("far", 10000000.0f));
        stateset->addUniform(new osg::Uniform("near", 1.0f));
        stateset->addUniform(new osg::Uniform("skyBlendingStart", 8000000.0f));
        stateset->addUniform(
            new osg::Uniform("screenRes", osg::Vec2f{ static_cast<float>(mWidth), static_cast<float>(mHeight) }));
        stateset->addUniform(new osg::Uniform("alpha", 1.f));
        stateset->addUniform(new osg::Uniform("actorFade", 1.f));

        osg::ref_ptr<osg::Texture2D> dummyTexture = new osg::Texture2D();
        dummyTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        dummyTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        dummyTexture->setInternalFormat(GL_DEPTH_COMPONENT);
        dummyTexture->setTextureSize(1, 1);
        dummyTexture->setShadowComparison(true);
        dummyTexture->setShadowCompareFunc(osg::Texture::ShadowCompareFunc::ALWAYS);
        stateset->setTextureAttribute(7, dummyTexture, osg::StateAttribute::ON);

        mLight = new SceneUtil::Light;
        mLight->setSpecular(osg::Vec4(0, 0, 0, 0));
        mLight->setConstantAttenuation(1.f);
        mLight->setLinearAttenuation(0.f);
        mLight->setQuadraticAttenuation(0.f);
        lightManager->setSunlight(mLight);
        setLight(osg::Vec3f(0.5f, 0.5f, 0.5f), osg::Vec3f(0.5f, 0.5f, 0.5f), osg::Vec3f(0.f, 0.f, -1.f));

        mModels = new osg::MatrixTransform;
        lightManager->addChild(mModels);
        mRTTNode->mGroup->addChild(lightManager);

        mRTTNode->addUpdateCallback(new UpdateSubgraph(mRTTNode->mGroup));
        mParent->addChild(mRTTNode);
    }

    MenuScene::~MenuScene()
    {
        stop();
    }

    void MenuScene::stop()
    {
        if (mStopped)
            return;
        mStopped = true;
        mParent->removeChild(mRTTNode);
    }

    osg::ref_ptr<osg::Texture2D> MenuScene::getTexture()
    {
        return static_cast<osg::Texture2D*>(mRTTNode->getColorTexture(nullptr));
    }

    void MenuScene::setFrustum(float left, float right, float bottom, float top, float nearPlane, float farPlane)
    {
        // (a frustum that is not a box with the top above the bottom and the right of the left has no picture)
        if (nearPlane <= 0.f || farPlane <= nearPlane || top <= bottom || right <= left)
            return;
        mRTTNode->setFrustum(left, right, bottom, top, nearPlane, farPlane);
    }

    void MenuScene::setCamera(const osg::Vec3f& eye, const osg::Vec3f& forward, const osg::Vec3f& up)
    {
        if (forward.length2() == 0.f)
            return;
        mRTTNode->setView(osg::Matrixf::lookAt(eye, eye + forward, up));
    }

    void MenuScene::setLight(const osg::Vec3f& ambient, const osg::Vec3f& diffuse, const osg::Vec3f& direction)
    {
        osg::Vec3f toLight = -direction;
        if (toLight.length2() > 0.f)
            toLight.normalize();
        mLight->setPosition(osg::Vec4(toLight, 0.0));
        mLight->setDiffuse(osg::Vec4(diffuse, 1));
        mLight->setAmbient(osg::Vec4(ambient, 1));
    }

    void MenuScene::setClearColour(const osg::Vec4f& colour)
    {
        mRTTNode->setClearColour(colour);
    }

    MenuScene::Model* MenuScene::find(const std::string& name)
    {
        const auto it = mModelsByName.find(name);
        return it == mModelsByName.end() ? nullptr : it->second.get();
    }

    const MenuScene::Model* MenuScene::find(const std::string& name) const
    {
        const auto it = mModelsByName.find(name);
        return it == mModelsByName.end() ? nullptr : it->second.get();
    }

    bool MenuScene::hasModel(const std::string& name) const
    {
        return find(name) != nullptr;
    }

    bool MenuScene::addModel(const std::string& name, VFS::Path::NormalizedView path)
    {
        removeModel(name);

        osg::ref_ptr<osg::Node> instance;
        try
        {
            instance = mResourceSystem->getSceneManager()->getInstance(path);
        }
        catch (const std::exception& e)
        {
            Log(Debug::Error) << "Menu scene: cannot load " << path << ": " << e.what();
            return false;
        }
        if (instance == nullptr)
            return false;

        auto model = std::make_unique<Model>();
        model->mRoot = new osg::MatrixTransform;
        model->mRoot->setName(name);
        // The point lights that a script adds (addLight) reach a model through the light list that this callback makes
        // for its subgraph, as the objects of the world have it; without it only the sunlight (ambient and directional)
        // lights the model (the lock was black in the first run, Mac round 12AK)
        model->mRoot->addCullCallback(new SceneUtil::LightListCallback);
        model->mRoot->addChild(instance);
        model->mInstance = instance;

        SceneUtil::NodeMapVisitor visitor(model->mNodes);
        instance->accept(visitor);
        {
            // What a script can name (the scene file's nodes that the optimizer kept), for the log of a run
            std::string names;
            size_t count = 0;
            for (const auto& entry : model->mNodes)
            {
                if (++count > 60)
                    break;
                names += " " + entry.first;
            }
            Log(Debug::Info) << "Menu scene: " << name << " has the nodes" << names;
        }

        if (const SceneUtil::EmbeddedSequences* sequences = findEmbeddedSequences(*instance))
        {
            for (const osg::ref_ptr<const SceneUtil::KeyframeHolder>& holder : *sequences)
            {
                Sequence sequence;
                if (!sequenceOf(*holder, sequence))
                    continue;
                model->mSequences.push_back(sequence);
                model->mHolders.push_back(holder);
            }
        }

        // The shader of the models works with premultiplied alpha, as the preview of the character does
        SetUpBlendVisitor blend;
        instance->accept(blend);

        mModels->addChild(model->mRoot);
        mModelsByName[name] = std::move(model);
        return true;
    }

    void MenuScene::removeModel(const std::string& name)
    {
        const auto it = mModelsByName.find(name);
        if (it == mModelsByName.end())
            return;
        mModels->removeChild(it->second->mRoot);
        mModelsByName.erase(it);
    }

    void MenuScene::setTransform(const std::string& name, const osg::Matrixf& transform)
    {
        if (Model* model = find(name))
            model->mRoot->setMatrix(transform);
    }

    void MenuScene::setSceneTransform(const osg::Matrixf& transform)
    {
        mModels->setMatrix(transform);
    }

    bool MenuScene::addLight(const std::string& name, const std::string& node, const osg::Vec3f& offset,
        const osg::Vec3f& diffuse, float radius)
    {
        Model* model = find(name);
        if (model == nullptr)
            return false;

        osg::ref_ptr<SceneUtil::Light> light = new SceneUtil::Light;
        light->setDiffuse(osg::Vec4f(diffuse, 1.f));
        light->setAmbient(osg::Vec4f(0.f, 0.f, 0.f, 1.f));
        light->setSpecular(osg::Vec4f(diffuse, 1.f));
        light->setConstantAttenuation(1.f);
        light->setLinearAttenuation(0.f);
        light->setQuadraticAttenuation(0.f);
        osg::ref_ptr<SceneUtil::LightSource> source = new SceneUtil::LightSource;
        source->setRadius(radius);
        source->setLight(light);

        osg::ref_ptr<osg::MatrixTransform> place = new osg::MatrixTransform(osg::Matrixf::translate(offset));
        place->addChild(source);

        osg::Group* parent = model->mRoot;
        // An empty name is the root of the model: the map has the unnamed transforms of the file under that name too
        const auto found = node.empty() ? model->mNodes.end() : model->mNodes.find(Misc::StringUtils::lowerCase(node));
        if (found != model->mNodes.end())
            parent = found->second;
        Log(Debug::Info) << "Menu scene: light of the model " << name << " under "
                         << (found != model->mNodes.end() ? node : std::string("its root (no node '") + node + "')")
                         << " at " << offset.x() << " " << offset.y() << " " << offset.z();
        parent->addChild(place);
        return true;
    }

    void MenuScene::setVisible(const std::string& name, bool visible)
    {
        if (Model* model = find(name))
            model->mRoot->setNodeMask(visible ? ~0u : 0u);
    }

    std::vector<MenuScene::Sequence> MenuScene::sequences(const std::string& name) const
    {
        if (const Model* model = find(name))
            return model->mSequences;
        return {};
    }

    bool MenuScene::setSequenceTime(const std::string& name, const std::string& sequence, float time)
    {
        Model* model = find(name);
        if (model == nullptr)
            return false;

        const std::string wanted = Misc::StringUtils::lowerCase(sequence);
        size_t index = model->mSequences.size();
        for (size_t i = 0; i < model->mSequences.size(); ++i)
            if (Misc::StringUtils::lowerCase(model->mSequences[i].mName) == wanted)
                index = i;
        if (index == model->mSequences.size())
            return false;

        if (model->mActiveSequence != wanted)
        {
            // The nodes of the sequence that was on get the controllers of this one; a node that only the old one
            // moved keeps the place it was left at
            for (auto& [node, callback] : model->mActive)
                node->removeUpdateCallback(callback);
            model->mActive.clear();
            model->mActiveSequence = wanted;

            for (const auto& [nodeName, controller] : model->mHolders[index]->mKeyframeControllers)
            {
                const auto found = model->mNodes.find(Misc::StringUtils::lowerCase(nodeName));
                if (found == model->mNodes.end())
                    continue;
                osg::ref_ptr<SceneUtil::KeyframeController> cloned
                    = osg::clone(controller.get(), osg::CopyOp::SHALLOW_COPY);
                cloned->setSource(model->mTime);
                osg::Callback* callback = cloned->getAsCallback();
                found->second->addUpdateCallback(callback);
                model->mActive.emplace_back(found->second, callback);
            }
        }

        const Sequence& range = model->mSequences[index];
        model->mTime->mTime = range.mStart + std::clamp(time, 0.f, range.mStop - range.mStart);
        return true;
    }
}

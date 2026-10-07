#include "water.hpp"

#include <cmath>
#include <limits>
#include <sstream>

#include <osg/ClipNode>
#include <osg/Depth>
#include <osg/Fog>
#include <osg/FrontFace>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/PositionAttitudeTransform>
#include <osg/ViewportIndexed>

#include <osgUtil/CullVisitor>
#include <osgUtil/IncrementalCompileOperation>

#include <components/resource/imagemanager.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>

#include <components/sceneutil/depth.hpp>
#include <components/sceneutil/fog.hpp>
#include <components/sceneutil/material.hpp>
#include <components/sceneutil/rtt.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/sceneutil/waterutil.hpp>

#include <components/misc/constants.hpp>
#include <components/stereo/stereomanager.hpp>

#include <components/nifosg/controller.hpp>

#include <components/shader/shadermanager.hpp>

#include <components/esm/util.hpp>
#include <components/esm3/loadcell.hpp>

#include <components/fallback/fallback.hpp>

#include <components/settings/values.hpp>

#include "../mwworld/cellstore.hpp"

#include "renderbin.hpp"
#include "ripples.hpp"
#include "ripplesimulation.hpp"
#include "util.hpp"
#include "vismask.hpp"

namespace OFRender
{

    // --------------------------------------------------------------------------------------------------------------------------------

    /// @brief Allows to cull and clip meshes that are below a plane. Useful for reflection camera effects.
    /// Also handles flipping of the plane when the eye point goes below it.
    /// To use, simply create the scene as subgraph of this node, then do setPlane(const osg::Plane& plane);
    class ClipCullNode : public osg::Group
    {
        class PlaneCullCallback : public SceneUtil::NodeCallback<PlaneCullCallback, osg::Node*, osgUtil::CullVisitor*>
        {
        public:
            /// @param cullPlane The culling plane (in world space).
            PlaneCullCallback(const osg::Plane* cullPlane)
                : mCullPlane(cullPlane)
            {
            }

            void operator()(osg::Node* node, osgUtil::CullVisitor* cv)
            {
                osg::Polytope::PlaneList origPlaneList
                    = cv->getProjectionCullingStack().back().getFrustum().getPlaneList();

                osg::Plane plane = *mCullPlane;
                plane.transform(*cv->getCurrentRenderStage()->getInitialViewMatrix());

                osg::Vec3d eyePoint = cv->getEyePoint();
                if (mCullPlane->intersect(osg::BoundingSphere(osg::Vec3d(0, 0, eyePoint.z()), 0)) > 0)
                    plane.flip();

                cv->getProjectionCullingStack().back().getFrustum().add(plane);

                traverse(node, cv);

                // undo
                cv->getProjectionCullingStack().back().getFrustum().set(origPlaneList);
            }

        private:
            const osg::Plane* mCullPlane;
        };

        class FlipCallback : public SceneUtil::NodeCallback<FlipCallback, osg::Node*, osgUtil::CullVisitor*>
        {
        public:
            FlipCallback(const osg::Plane* cullPlane)
                : mCullPlane(cullPlane)
            {
            }

            void operator()(osg::Node* node, osgUtil::CullVisitor* cv)
            {
                osg::Vec3d eyePoint = cv->getEyePoint();

                osg::RefMatrix* modelViewMatrix = new osg::RefMatrix(*cv->getModelViewMatrix());

                // apply the height of the plane
                // we can't apply this height in the addClipPlane() since the "flip the below graph" function would
                // otherwise flip the height as well
                modelViewMatrix->preMultTranslate(mCullPlane->getNormal() * ((*mCullPlane)[3] * -1));

                // flip the below graph if the eye point is above the plane
                if (mCullPlane->intersect(osg::BoundingSphere(osg::Vec3d(0, 0, eyePoint.z()), 0)) > 0)
                {
                    modelViewMatrix->preMultScale(osg::Vec3(1, 1, -1));
                }

                // move the plane back along its normal a little bit to prevent bleeding at the water shore
                const float fov = Settings::camera().mFieldOfView;
                constexpr double clipFudgeMin = 2.5; // minimum offset of clip plane
                constexpr double clipFudgeScale = -15000.0;
                double clipFudge
                    = std::abs(std::abs((*mCullPlane)[3]) - eyePoint.z()) * fov / clipFudgeScale - clipFudgeMin;
                modelViewMatrix->preMultTranslate(mCullPlane->getNormal() * clipFudge);

                cv->pushModelViewMatrix(modelViewMatrix, osg::Transform::RELATIVE_RF);
                traverse(node, cv);
                cv->popModelViewMatrix();
            }

        private:
            const osg::Plane* mCullPlane;
        };

    public:
        ClipCullNode()
        {
            addCullCallback(new PlaneCullCallback(&mPlane));

            mClipNodeTransform = new osg::Group;
            mClipNodeTransform->addCullCallback(new FlipCallback(&mPlane));
            osg::Group::addChild(mClipNodeTransform);

            mClipNode = new osg::ClipNode;

            mClipNodeTransform->addChild(mClipNode);
        }

        void setPlane(const osg::Plane& plane)
        {
            if (plane == mPlane)
                return;
            mPlane = plane;

            mClipNode->getClipPlaneList().clear();
            mClipNode->addClipPlane(
                new osg::ClipPlane(0, osg::Plane(mPlane.getNormal(), 0))); // mPlane.d() applied in FlipCallback
            mClipNode->setStateSetModes(*getOrCreateStateSet(), osg::StateAttribute::ON);
            mClipNode->setCullingActive(false);
        }

    private:
        osg::ref_ptr<osg::Group> mClipNodeTransform;
        osg::ref_ptr<osg::ClipNode> mClipNode;

        osg::Plane mPlane;
    };

    /// This callback on the Camera has the effect of a RELATIVE_RF_INHERIT_VIEWPOINT transform mode (which does not
    /// exist in OSG). We want to keep the View Point of the parent camera so we will not have to recreate LODs.
    class InheritViewPointCallback
        : public SceneUtil::NodeCallback<InheritViewPointCallback, osg::Node*, osgUtil::CullVisitor*>
    {
    public:
        InheritViewPointCallback() {}

        void operator()(osg::Node* node, osgUtil::CullVisitor* cv)
        {
            osg::ref_ptr<osg::RefMatrix> modelViewMatrix = new osg::RefMatrix(*cv->getModelViewMatrix());
            cv->popModelViewMatrix();
            cv->pushModelViewMatrix(modelViewMatrix, osg::Transform::ABSOLUTE_RF_INHERIT_VIEWPOINT);
            traverse(node, cv);
        }
    };

    /// Moves water mesh away from the camera slightly if the camera gets too close on the Z axis.
    /// The offset works around graphics artifacts that occurred with the GL_DEPTH_CLAMP when the camera gets extremely
    /// close to the mesh (seen on NVIDIA at least). Must be added as a Cull callback.
    class FudgeCallback : public SceneUtil::NodeCallback<FudgeCallback, osg::Node*, osgUtil::CullVisitor*>
    {
    public:
        void operator()(osg::Node* node, osgUtil::CullVisitor* cv)
        {
            const float fudge = 0.2f;
            if (std::abs(cv->getEyeLocal().z()) < fudge)
            {
                float diff = fudge - cv->getEyeLocal().z();
                osg::RefMatrix* modelViewMatrix = new osg::RefMatrix(*cv->getModelViewMatrix());

                if (cv->getEyeLocal().z() >= 0)
                    modelViewMatrix->preMultTranslate(osg::Vec3f(0, 0, -diff));
                else
                    modelViewMatrix->preMultTranslate(osg::Vec3f(0, 0, diff));

                cv->pushModelViewMatrix(modelViewMatrix, osg::Transform::RELATIVE_RF);
                traverse(node, cv);
                cv->popModelViewMatrix();
            }
            else
                traverse(node, cv);
        }
    };

    class RainSettingsUpdater : public SceneUtil::StateSetUpdater
    {
    public:
        RainSettingsUpdater() = default;

        void setRainIntensity(float rainIntensity) { mRainIntensity = rainIntensity; }

    protected:
        void setDefaults(osg::StateSet* stateset) override
        {
            osg::ref_ptr<osg::Uniform> rainIntensityUniform = new osg::Uniform("rainIntensity", 0.0f);
            stateset->addUniform(rainIntensityUniform.get());
        }

        void apply(osg::StateSet* stateset, osg::NodeVisitor* /*nv*/) override
        {
            osg::ref_ptr<osg::Uniform> rainIntensityUniform = stateset->getUniform("rainIntensity");
            if (rainIntensityUniform != nullptr)
                rainIntensityUniform->set(mRainIntensity);
        }

    private:
        float mRainIntensity{ 0.f };
    };

    class Reflection : public SceneUtil::RTTNode
    {
    public:
        Reflection(uint32_t rttSize, bool isInterior)
            : RTTNode(rttSize, rttSize, 0, false, 0, StereoAwareness::Aware, shouldAddMSAAIntermediateTarget())
        {
            setInterior(isInterior);
            setDepthBufferInternalFormat(GL_DEPTH32F_STENCIL8);
            mClipCullNode = new ClipCullNode;
        }

        void setDefaults(osg::Camera* camera) override
        {
            camera->setReferenceFrame(osg::Camera::RELATIVE_RF);
            camera->setSmallFeatureCullingPixelSize(Settings::water().mSmallFeatureCullingPixelSize);
            camera->setName("ReflectionCamera");
            camera->addCullCallback(new InheritViewPointCallback);

            // Inform the shader that we're in a reflection
            camera->getOrCreateStateSet()->addUniform(new osg::Uniform("isReflection", true));

            // XXX: should really flip the FrontFace on each renderable instead of forcing clockwise.
            osg::ref_ptr<osg::FrontFace> frontFace(new osg::FrontFace);
            frontFace->setMode(osg::FrontFace::CLOCKWISE);
            camera->getOrCreateStateSet()->setAttributeAndModes(frontFace, osg::StateAttribute::ON);

            camera->addChild(mClipCullNode);
            camera->setNodeMask(Mask_RenderToTexture);

            SceneUtil::ShadowManager::instance().disableShadowsForStateSet(*camera->getOrCreateStateSet());
        }

        void apply(osg::Camera* camera) override
        {
            camera->setViewMatrix(mViewMatrix);
            camera->setCullMask(mNodeMask);
        }

        void setInterior(bool isInterior)
        {
            mInterior = isInterior;
            mNodeMask = calcNodeMask();
        }

        void setWaterLevel(float waterLevel)
        {
            mViewMatrix = osg::Matrix::scale(1, 1, -1) * osg::Matrix::translate(0, 0, 2 * waterLevel);
            mClipCullNode->setPlane(osg::Plane(osg::Vec3d(0, 0, 1), osg::Vec3d(0, 0, waterLevel)));
        }

        void setScene(osg::Node* scene)
        {
            if (mScene)
                mClipCullNode->removeChild(mScene);
            mScene = scene;
            mClipCullNode->addChild(scene);
        }

        void showWorld(bool show)
        {
            if (show)
                mNodeMask = calcNodeMask();
            else
                mNodeMask = calcNodeMask() & ~sToggleWorldMask;
        }

    private:
        unsigned int calcNodeMask()
        {
            int reflectionDetail = Settings::water().mReflectionDetail;
            reflectionDetail = std::clamp(reflectionDetail, mInterior ? 2 : 0, 5);
            unsigned int extraMask = 0;
            if (reflectionDetail >= 1)
                extraMask |= Mask_Terrain;
            if (reflectionDetail >= 2)
                extraMask |= Mask_Static;
            if (reflectionDetail >= 3)
                extraMask |= Mask_Effect | Mask_ParticleSystem | Mask_Object;
            if (reflectionDetail >= 4)
                extraMask |= Mask_Player | Mask_Actor;
            if (reflectionDetail >= 5)
                extraMask |= Mask_Groundcover;
            return Mask_Scene | Mask_Sky | Mask_Lighting | extraMask;
        }

        osg::ref_ptr<ClipCullNode> mClipCullNode;
        osg::ref_ptr<osg::Node> mScene;
        osg::Node::NodeMask mNodeMask;
        osg::Matrix mViewMatrix{ osg::Matrix::identity() };
        bool mInterior;
    };

    /// DepthClampCallback enables GL_DEPTH_CLAMP for the current draw, if supported.
    class DepthClampCallback : public osg::Drawable::DrawCallback
    {
    public:
        void drawImplementation(osg::RenderInfo& renderInfo, const osg::Drawable* drawable) const override
        {
            static bool supported = osg::isGLExtensionOrVersionSupported(
                renderInfo.getState()->getContextID(), "GL_ARB_depth_clamp", 3.3f);
            if (!supported)
            {
                drawable->drawImplementation(renderInfo);
                return;
            }

            glEnable(GL_DEPTH_CLAMP);

            drawable->drawImplementation(renderInfo);

            // restore default
            glDisable(GL_DEPTH_CLAMP);
        }
    };

    Water::Water(osg::Group* parent, osg::Group* sceneRoot, Resource::ResourceSystem* resourceSystem,
        osgUtil::IncrementalCompileOperation* ico)
        : mRainSettingsUpdater(nullptr)
        , mParent(parent)
        , mSceneRoot(sceneRoot)
        , mResourceSystem(resourceSystem)
        , mEnabled(true)
        , mToggled(true)
        , mTop(0)
        , mInterior(false)
        , mShowWorld(true)
        , mTileMode(false)
        , mViewLevel(std::numeric_limits<float>::lowest())
        , mCullCallback(nullptr)
        , mShaderWaterStateSetUpdater(nullptr)
    {
        mSimulation = std::make_unique<RippleSimulation>(mSceneRoot, resourceSystem);

        mWaterGeom = SceneUtil::createWaterGeometry(Constants::CellSizeInUnits * 150, 40, 900);
        mWaterGeom->setDrawCallback(new DepthClampCallback);
        mWaterGeom->setNodeMask(Mask_Water);
        mWaterGeom->setDataVariance(osg::Object::STATIC);
        mWaterGeom->setName("Water Geometry");

        mWaterNode = new osg::PositionAttitudeTransform;
        mWaterNode->setName("Water Root");
        mWaterNode->addChild(mWaterGeom);
        mWaterNode->addCullCallback(new FudgeCallback);

        // simple water fallback for the local map
        mSimpleWaterGeom = osg::clone(mWaterGeom.get(), osg::CopyOp::DEEP_COPY_NODES);
        createSimpleWaterStateSet(mSimpleWaterGeom, Fallback::Map::getFloat("Water_Map_Alpha"));
        mSimpleWaterGeom->setNodeMask(Mask_SimpleWater);
        mSimpleWaterGeom->setName("Simple Water Geometry");
        mWaterNode->addChild(mSimpleWaterGeom);

        // The square of a cell of a worldspace of Fallout, which all the tiles share
        mTileGeom = SceneUtil::createWaterGeometry(static_cast<float>(Constants::ESM4CellSizeInUnits), 1, 1);
        mTileGeom->setDrawCallback(new DepthClampCallback);
        mTileGeom->setNodeMask(Mask_Water);
        mTileGeom->setDataVariance(osg::Object::STATIC);
        mTileGeom->setName("Water Tile Geometry");
        // simple water fallback for the local map, as for the plane of the world
        mTileSimpleGeom = osg::clone(mTileGeom.get(), osg::CopyOp::DEEP_COPY_NODES);
        createSimpleWaterStateSet(mTileSimpleGeom, Fallback::Map::getFloat("Water_Map_Alpha"));
        mTileSimpleGeom->setNodeMask(Mask_SimpleWater);
        mTileSimpleGeom->setName("Water Tile Simple Geometry");
        mTileGroup = new osg::Group;
        mTileGroup->setName("Water Tiles");
        mWaterNode->addChild(mTileGroup);

        mSceneRoot->addChild(mWaterNode);

        setHeight(mTop);

        updateWaterMaterial();

        if (ico)
            ico->add(mWaterNode);
    }

    void Water::setCullCallback(osg::Callback* callback)
    {
        if (mCullCallback)
        {
            mWaterNode->removeCullCallback(mCullCallback);
            if (mReflection)
                mReflection->removeCullCallback(mCullCallback);
        }

        mCullCallback = callback;

        if (callback)
        {
            mWaterNode->addCullCallback(callback);
            if (mReflection)
                mReflection->addCullCallback(callback);
        }
    }

    void Water::updateWaterMaterial()
    {
        if (mShaderWaterStateSetUpdater)
        {
            mWaterNode->removeCullCallback(mShaderWaterStateSetUpdater);
            mShaderWaterStateSetUpdater = nullptr;
        }
        if (mReflection)
        {
            mParent->removeChild(mReflection);
            mReflection = nullptr;
        }
        if (mRipples)
        {
            mParent->removeChild(mRipples);
            mRipples = nullptr;
            mSimulation->setRipples(nullptr);
        }

        mWaterNode->setStateSet(nullptr);
        mWaterGeom->setStateSet(nullptr);
        mWaterGeom->setUpdateCallback(nullptr);
        mTileGeom->setStateSet(nullptr);
        mTileGeom->setUpdateCallback(nullptr);

        if (Settings::water().mShader)
        {
            const unsigned int rttSize = Settings::water().mRttSize;

            mReflection = new Reflection(rttSize, mInterior);
            mReflection->setWaterLevel(mTop);
            mReflection->setScene(mSceneRoot);
            if (mCullCallback)
                mReflection->addCullCallback(mCullCallback);
            mParent->addChild(mReflection);

            mRipples = new Ripples(mResourceSystem);
            mSimulation->setRipples(mRipples);
            mParent->addChild(mRipples);

            showWorld(mShowWorld);

            createShaderWaterStateSet(mWaterNode);
        }
        else
        {
            createSimpleWaterStateSet(mWaterGeom, Fallback::Map::getFloat("Water_World_Alpha"));
            createSimpleWaterStateSet(mTileGeom, Fallback::Map::getFloat("Water_World_Alpha"));
        }

        mResourceSystem->getSceneManager()->setUpNormalsRTForStateSet(mWaterGeom->getOrCreateStateSet(), true);
        mResourceSystem->getSceneManager()->setUpNormalsRTForStateSet(mTileGeom->getOrCreateStateSet(), true);

        // The way a look is given depends on whether the water has its shader
        applyPlaneLook();
        for (auto& [cell, tile] : mTiles)
            applyTileLook(tile);

        updateVisible();
    }

    osg::Vec3d Water::getPosition() const
    {
        return mWaterNode->getPosition();
    }

    osg::Drawable* Water::getDrawable() const
    {
        return mTileMode ? mTileGeom.get() : mWaterGeom.get();
    }

    void Water::createSimpleWaterStateSet(osg::Node* node, float alpha)
    {
        osg::ref_ptr<osg::StateSet> stateset
            = SceneUtil::createSimpleWaterStateSet(alpha, OFRender::RenderBin_DepthSorted);

        node->setStateSet(stateset);
        node->setUpdateCallback(nullptr);
        mRainSettingsUpdater = nullptr;

        // Add animated textures
        std::vector<osg::ref_ptr<osg::Texture2D>> textures;
        const int frameCount = std::clamp(Fallback::Map::getInt("Water_SurfaceFrameCount"), 0, 320);
        std::string_view texture = Fallback::Map::getString("Water_SurfaceTexture");
        for (int i = 0; i < frameCount; ++i)
        {
            std::ostringstream texname;
            texname << "textures/water/" << texture << std::setw(2) << std::setfill('0') << i << ".dds";
            const VFS::Path::Normalized path(texname.str());
            osg::ref_ptr<osg::Texture2D> tex(new osg::Texture2D(mResourceSystem->getImageManager()->getImage(path)));
            tex->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
            tex->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
            mResourceSystem->getSceneManager()->applyFilterSettings(tex);
            textures.push_back(tex);
        }

        if (!textures.empty())
        {
            float fps = Fallback::Map::getFloat("Water_SurfaceFPS");

            osg::ref_ptr<NifOsg::FlipController> controller(new NifOsg::FlipController(0, 1.f / fps, textures));
            controller->setSource(std::make_shared<SceneUtil::FrameTimeSource>());
            node->setUpdateCallback(controller);

            stateset->setTextureAttribute(0, textures[0], osg::StateAttribute::ON);
        }

        // use a shader to render the simple water, ensuring that fog is applied per pixel as required, and that the
        // colour of its material is used when it has no texture (the water of Fallout).
        // this could be removed if a more detailed water mesh, using some sort of paging solution, is implemented.
        Resource::SceneManager* sceneManager = mResourceSystem->getSceneManager();
        sceneManager->recreateShaders(node);
    }

    namespace
    {
        // The uniforms of the shader of the water that say how the water looks
        void addLookUniforms(osg::StateSet* stateset, const WaterLook& look)
        {
            stateset->addUniform(new osg::Uniform("waterShallowColour", look.mShallowColour));
            stateset->addUniform(new osg::Uniform("waterDeepColour", look.mDeepColour));
            stateset->addUniform(new osg::Uniform("waterLook", osg::Vec2f(look.mOpacity, look.mReflectivity)));
        }

        // The material of the water that is drawn without the shader: white at the alpha of the water of Morrowind
        // for the standard look (what SceneUtil::createSimpleWaterStateSet makes), else the colour and the opacity
        // of the look. The ambient colour has the colour as well, or the water would be lit with white by the light
        // of the cell.
        osg::ref_ptr<SceneUtil::Material> makeSimpleMaterial(const WaterLook& look)
        {
            osg::ref_ptr<SceneUtil::Material> material(new SceneUtil::Material);
            material->setEmission(osg::Vec4f(0.f, 0.f, 0.f, 1.f));
            if (look == WaterLook::standard())
            {
                material->setDiffuse(osg::Vec4f(1.f, 1.f, 1.f, Fallback::Map::getFloat("Water_World_Alpha")));
                material->setAmbient(osg::Vec4f(1.f, 1.f, 1.f, 1.f));
            }
            else
            {
                const osg::Vec3f colour = look.simpleColour();
                material->setDiffuse(osg::Vec4f(colour, look.mOpacity));
                material->setAmbient(osg::Vec4f(colour, 1.f));
            }
            material->setVertexColorMode(SceneUtil::VertexColorModes::None);
            return material;
        }
    }

    class ShaderWaterStateSetUpdater : public SceneUtil::StateSetUpdater
    {
    public:
        ShaderWaterStateSetUpdater(Water* water, Resource::ResourceSystem* resourceSystem, Reflection* reflection,
            Ripples* ripples, osg::ref_ptr<osg::Program> program, osg::ref_ptr<osg::Texture2D> normalMap)
            : mWater(water)
            , mReflection(reflection)
            , mRipples(ripples)
            , mProgram(std::move(program))
            , mNormalMap(std::move(normalMap))
            , mResourceSystem(resourceSystem)
            , mOpaqueDepthTextureUnit(resourceSystem->getSceneManager()->getShaderManager().reserveGlobalTextureUnits(
                  Shader::ShaderManager::Slot::OpaqueDepthTexture))
            , mOpaqueColorTextureUnit(resourceSystem->getSceneManager()->getShaderManager().reserveGlobalTextureUnits(
                  Shader::ShaderManager::Slot::OpaqueColorTexture))
        {
        }

        void setDefaults(osg::StateSet* stateset) override
        {
            stateset->addUniform(new osg::Uniform("normalMap", 0));
            stateset->setTextureAttribute(0, mNormalMap, osg::StateAttribute::ON);
            stateset->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
            stateset->setAttributeAndModes(mProgram, osg::StateAttribute::ON);

            stateset->addUniform(new osg::Uniform("reflectionMap", 1));
            stateset->addUniform(new osg::Uniform("opaqueColorTex", mOpaqueColorTextureUnit));
            stateset->addUniform(new osg::Uniform("opaqueDepthTex", mOpaqueDepthTextureUnit));
            stateset->setMode(GL_BLEND, osg::StateAttribute::ON);
            stateset->addUniform(new osg::Uniform("waterSurface", true));
            stateset->setRenderBinDetails(OFRender::RenderBin_DepthSorted, "DepthSortedBin");
            osg::ref_ptr<osg::Depth> depth = new SceneUtil::AutoDepth;
            depth->setWriteMask(false);
            stateset->setAttributeAndModes(depth, osg::StateAttribute::ON);

            if (mRipples)
            {
                stateset->addUniform(new osg::Uniform("rippleMap", 4));
            }
            stateset->addUniform(new osg::Uniform("nodePosition", osg::Vec3f(mWater->getPosition())));
            addLookUniforms(stateset, mWater->getLook());
        }

        void apply(osg::StateSet* stateset, osg::NodeVisitor* nv) override
        {
            osgUtil::CullVisitor* cv = static_cast<osgUtil::CullVisitor*>(nv);
            stateset->setTextureAttribute(1, mReflection->getColorTexture(cv), osg::StateAttribute::ON);
            stateset->setTextureAttribute(mOpaqueColorTextureUnit,
                mResourceSystem->getSceneManager()->getOpaqueColorTex(cv->getTraversalNumber()),
                osg::StateAttribute::ON);
            stateset->setTextureAttribute(mOpaqueDepthTextureUnit,
                mResourceSystem->getSceneManager()->getOpaqueDepthTex(cv->getTraversalNumber()),
                osg::StateAttribute::ON);

            if (mRipples)
            {
                stateset->setTextureAttribute(4, mRipples->getColorTexture(), osg::StateAttribute::ON);
            }
            stateset->getUniform("nodePosition")->set(osg::Vec3f(mWater->getPosition()));
            // The water of one plane has one look, which is changed with the cell. Tiles have their own.
            const WaterLook& look = mWater->getLook();
            stateset->getUniform("waterShallowColour")->set(look.mShallowColour);
            stateset->getUniform("waterDeepColour")->set(look.mDeepColour);
            stateset->getUniform("waterLook")->set(osg::Vec2f(look.mOpacity, look.mReflectivity));
        }

    private:
        Water* mWater;
        Reflection* mReflection;
        Ripples* mRipples;
        osg::ref_ptr<osg::Program> mProgram;
        osg::ref_ptr<osg::Texture2D> mNormalMap;
        Resource::ResourceSystem* mResourceSystem;
        int mOpaqueDepthTextureUnit;
        int mOpaqueColorTextureUnit;
    };

    void Water::createShaderWaterStateSet(osg::Node* node)
    {
        // use a define map to conditionally compile the shader
        std::map<std::string, std::string> defineMap;
        const int rippleDetail = Settings::water().mRainRippleDetail;
        defineMap["waterRefraction"] = std::string(Settings::water().mRefraction ? "1" : "0");
        defineMap["rainRippleDetail"] = std::to_string(rippleDetail);
        defineMap["rippleMapWorldScale"] = std::to_string(RipplesSurface::sWorldScaleFactor);
        defineMap["rippleMapSize"] = std::to_string(RipplesSurface::sRTTSize) + ".0";
        defineMap["sunlightScattering"] = Settings::water().mSunlightScattering ? "1" : "0";
        defineMap["wobblyShores"] = Settings::water().mWobblyShores ? "1" : "0";

        Stereo::shaderStereoDefines(defineMap);

        Shader::ShaderManager& shaderMgr = mResourceSystem->getSceneManager()->getShaderManager();
        osg::ref_ptr<osg::Program> program = shaderMgr.getProgram("water", defineMap);

        constexpr VFS::Path::NormalizedView waterImage("textures/omw/water_nm.png");
        osg::ref_ptr<osg::Texture2D> normalMap(
            new osg::Texture2D(mResourceSystem->getImageManager()->getImage(waterImage)));
        normalMap->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
        normalMap->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
        mResourceSystem->getSceneManager()->applyFilterSettings(normalMap);

        mRainSettingsUpdater = new RainSettingsUpdater();
        node->setUpdateCallback(mRainSettingsUpdater);

        mShaderWaterStateSetUpdater = new ShaderWaterStateSetUpdater(
            this, mResourceSystem, mReflection, mRipples, std::move(program), std::move(normalMap));
        node->addCullCallback(mShaderWaterStateSetUpdater);
    }

    void Water::processChangedSettings(const Settings::CategorySettingVector& settings)
    {
        updateWaterMaterial();
    }

    Water::~Water()
    {
        mParent->removeChild(mWaterNode);

        if (mReflection)
        {
            mParent->removeChild(mReflection);
            mReflection = nullptr;
        }
        if (mRipples)
        {
            mParent->removeChild(mRipples);
            mRipples = nullptr;
            mSimulation->setRipples(nullptr);
        }
    }

    void Water::listAssetsToPreload(std::vector<VFS::Path::Normalized>& textures)
    {
        const int frameCount = std::clamp(Fallback::Map::getInt("Water_SurfaceFrameCount"), 0, 320);
        std::string_view texture = Fallback::Map::getString("Water_SurfaceTexture");
        for (int i = 0; i < frameCount; ++i)
        {
            std::ostringstream texname;
            texname << "textures/water/" << texture << std::setw(2) << std::setfill('0') << i << ".dds";
            textures.emplace_back(texname.str());
        }
    }

    void Water::setEnabled(bool enabled)
    {
        mEnabled = enabled;
        updateVisible();
    }

    void Water::changeCell(const OFWorld::CellStore* store)
    {
        bool isInterior = !store->getCell()->isExterior();
        bool wasInterior = mInterior;
        const bool tileMode = !isInterior && ESM::isEsm4Ext(store->getCell()->getWorldSpace());
        if (tileMode)
        {
            // The tiles are where their cells are
            mWaterNode->setPosition(osg::Vec3f());
            mInterior = false;
        }
        else if (!isInterior)
        {
            mWaterNode->setPosition(
                getSceneNodeCoordinates(store->getCell()->getGridX(), store->getCell()->getGridY()));
            mInterior = false;
        }
        else
        {
            mWaterNode->setPosition(osg::Vec3f(0, 0, mTop));
            mInterior = true;
        }
        if (mInterior != wasInterior && mReflection)
            mReflection->setInterior(mInterior);

        if (tileMode != mTileMode)
        {
            mTileMode = tileMode;
            if (tileMode)
                setCullCallback(nullptr);
            updateVisible();
        }
    }

    std::optional<float> Water::getTileHeightAt(float x, float y) const
    {
        const float size = static_cast<float>(Constants::ESM4CellSizeInUnits);
        const auto found
            = mTiles.find({ static_cast<int>(std::floor(x / size)), static_cast<int>(std::floor(y / size)) });
        if (found == mTiles.end())
            return std::nullopt;
        return found->second.mHeight;
    }

    void Water::applyTileLook(Tile& tile)
    {
        addLookUniforms(tile.mNode->getOrCreateStateSet(), tile.mLook);

        // Without the shader the surface has the material of the tile instead of the shared one, which the material of
        // the node of the surface overrides
        if (mReflection || tile.mLook == WaterLook::standard())
            tile.mSurface->setStateSet(nullptr);
        else
            makeSimpleMaterial(tile.mLook)
                ->setStateSet(
                    tile.mSurface->getOrCreateStateSet(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    }

    void Water::applyPlaneLook()
    {
        if (mReflection || mWaterGeom->getStateSet() == nullptr)
            return;
        makeSimpleMaterial(mLook)->setStateSet(mWaterGeom->getStateSet());
    }

    void Water::setLook(const WaterLook& look)
    {
        if (look == mLook)
            return;
        mLook = look;
        applyPlaneLook();
    }

    void Water::addTile(int gridX, int gridY, float height, const WaterLook& look)
    {
        removeTile(gridX, gridY);

        const float size = static_cast<float>(Constants::ESM4CellSizeInUnits);
        const osg::Vec3f centre((gridX + 0.5f) * size, (gridY + 0.5f) * size, height);

        osg::ref_ptr<osg::PositionAttitudeTransform> node(new osg::PositionAttitudeTransform);
        node->setName("Water Tile");
        node->setPosition(centre);
        osg::ref_ptr<osg::Group> surface(new osg::Group);
        surface->setName("Water Tile Surface");
        surface->addChild(mTileGeom);
        node->addChild(surface);
        node->addChild(mTileSimpleGeom);
        node->addCullCallback(new FudgeCallback);
        // The shader finds the place of a point of the water from the position of the node of its tile
        node->getOrCreateStateSet()->addUniform(new osg::Uniform("nodePosition", centre));
        mTileGroup->addChild(node);

        Tile& tile = mTiles[{ gridX, gridY }] = Tile{ node, surface, height, look };
        applyTileLook(tile);
        updateVisible();
    }

    bool Water::removeTile(int gridX, int gridY)
    {
        const auto found = mTiles.find({ gridX, gridY });
        if (found == mTiles.end())
            return false;
        mTileGroup->removeChild(found->second.mNode);
        mTiles.erase(found);
        updateVisible();
        return true;
    }

    void Water::setViewPoint(const osg::Vec3f& position)
    {
        if (!mTileMode)
            return;

        const std::optional<float> inside = getTileHeightAt(position.x(), position.y());
        mViewLevel = inside.value_or(std::numeric_limits<float>::lowest());

        // The reflection is of one height: the one of the water the camera is over, or else of the nearest water
        float level = mTop;
        if (inside)
            level = *inside;
        else if (!mTiles.empty())
        {
            const float size = static_cast<float>(Constants::ESM4CellSizeInUnits);
            float nearest = std::numeric_limits<float>::max();
            for (const auto& [cell, tile] : mTiles)
            {
                const float dx = (cell.first + 0.5f) * size - position.x();
                const float dy = (cell.second + 0.5f) * size - position.y();
                const float distance = dx * dx + dy * dy;
                if (distance < nearest)
                {
                    nearest = distance;
                    level = tile.mHeight;
                }
            }
        }

        if (level != mTop)
        {
            mTop = level;
            mSimulation->setWaterHeight(level);
            if (mReflection)
                mReflection->setWaterLevel(level);
        }
    }

    void Water::setHeight(const float height)
    {
        mTop = height;

        mSimulation->setWaterHeight(height);

        osg::Vec3f pos = mWaterNode->getPosition();
        pos.z() = height;
        mWaterNode->setPosition(pos);

        if (mReflection)
            mReflection->setWaterLevel(mTop);
    }

    void Water::setRainIntensity(float rainIntensity)
    {
        if (mRainSettingsUpdater)
            mRainSettingsUpdater->setRainIntensity(rainIntensity);
    }

    void Water::update(float dt, bool paused)
    {
        if (!paused)
        {
            mSimulation->update(dt);
        }

        if (mRipples)
        {
            mRipples->setPaused(paused);
        }
    }

    void Water::updateVisible()
    {
        bool visible = hasWater() && mToggled;
        mWaterNode->setNodeMask(visible ? ~0u : 0u);
        // The plane of the whole world is for the water that has one height, the tiles for the water of Fallout
        mWaterGeom->setNodeMask(mTileMode ? 0u : Mask_Water);
        mSimpleWaterGeom->setNodeMask(mTileMode ? 0u : Mask_SimpleWater);
        mTileGroup->setNodeMask(mTileMode ? ~0u : 0u);
        if (mReflection)
            mReflection->setNodeMask(visible ? Mask_RenderToTexture : 0u);
        if (mRipples)
            mRipples->setNodeMask(visible ? Mask_RenderToTexture : 0u);
    }

    bool Water::toggle()
    {
        mToggled = !mToggled;
        updateVisible();
        return mToggled;
    }

    bool Water::isUnderwater(const osg::Vec3f& pos) const
    {
        if (mTileMode)
        {
            const std::optional<float> height = getTileHeightAt(pos.x(), pos.y());
            return height && pos.z() < *height && mToggled;
        }
        return pos.z() < mTop && mToggled && mEnabled;
    }

    osg::Vec3f Water::getSceneNodeCoordinates(int gridX, int gridY)
    {
        return osg::Vec3f(static_cast<float>(gridX * Constants::CellSizeInUnits + (Constants::CellSizeInUnits / 2)),
            static_cast<float>(gridY * Constants::CellSizeInUnits + (Constants::CellSizeInUnits / 2)), mTop);
    }

    void Water::addEmitter(const OFWorld::Ptr& ptr, float scale, float force)
    {
        mSimulation->addEmitter(ptr, scale, force);
    }

    void Water::removeEmitter(const OFWorld::Ptr& ptr)
    {
        mSimulation->removeEmitter(ptr);
    }

    void Water::updateEmitterPtr(const OFWorld::Ptr& old, const OFWorld::Ptr& ptr)
    {
        mSimulation->updateEmitterPtr(old, ptr);
    }

    void Water::emitRipple(const osg::Vec3f& pos)
    {
        mSimulation->emitRipple(pos);
    }

    void Water::removeCell(const OFWorld::CellStore* store)
    {
        mSimulation->removeCell(store);
    }

    void Water::clearRipples()
    {
        mSimulation->clear();
    }

    void Water::showWorld(bool show)
    {
        if (mReflection)
            mReflection->showWorld(show);
        mShowWorld = show;
    }

}

#ifndef OPENFALLOUT_COMPONENTS_MYGUIPLATFORM_MYGUIRENDERMANAGER_H
#define OPENFALLOUT_COMPONENTS_MYGUIPLATFORM_MYGUIRENDERMANAGER_H

#include <MyGUI_RenderManager.h>

#include <osg/ref_ptr>

namespace Resource
{
    class ImageManager;
}

namespace Shader
{
    class ShaderManager;
}

namespace osgViewer
{
    class Viewer;
}

namespace osg
{
    class Group;
    class Camera;
    class RenderInfo;
    class StateSet;
    class Texture2D;
}

namespace MyGUIPlatform
{

    class Drawable;
    class OSGTexture;

    class RenderManager : public MyGUI::RenderManager, public MyGUI::IRenderTarget
    {
        osg::ref_ptr<osgViewer::Viewer> mViewer;
        osg::ref_ptr<osg::Group> mSceneRoot;
        osg::ref_ptr<Drawable> mDrawable;
        Resource::ImageManager* mImageManager;

        MyGUI::IntSize mViewSize;
        bool mUpdate;
        MyGUI::VertexColourType mVertexFormat;
        MyGUI::RenderTargetInfo mInfo;

        std::map<std::string, OSGTexture> mTextures;

        bool mIsInitialise;

        osg::ref_ptr<osg::Camera> mGuiRoot;

        float mInvScalingFactor;

        osg::StateSet* mInjectState;

    public:
        RenderManager(osgViewer::Viewer* viewer, osg::Group* sceneroot, Resource::ImageManager* imageManager,
            float scalingFactor);
        virtual ~RenderManager();

        void initialise();
        void shutdown();

        void enableShaders(Shader::ShaderManager& shaderManager);

        static RenderManager& getInstance() { return *getInstancePtr(); }
        static RenderManager* getInstancePtr()
        {
            return static_cast<RenderManager*>(MyGUI::RenderManager::getInstancePtr());
        }

        bool checkTexture(MyGUI::ITexture* texture) override;

        /** @see RenderManager::getViewSize */
        const MyGUI::IntSize& getViewSize() const override { return mViewSize; }

        /** @see RenderManager::getVertexFormat */
        MyGUI::VertexColourType getVertexFormat() const override { return mVertexFormat; }

        /** @see RenderManager::isFormatSupported */
        bool isFormatSupported(MyGUI::PixelFormat format, MyGUI::TextureUsage usage) override;

        /** @see RenderManager::createVertexBuffer */
        MyGUI::IVertexBuffer* createVertexBuffer() override;
        /** @see RenderManager::destroyVertexBuffer */
        void destroyVertexBuffer(MyGUI::IVertexBuffer* buffer) override;

        /** @see RenderManager::createTexture */
        MyGUI::ITexture* createTexture(const std::string& name) override;
        /** @see RenderManager::destroyTexture */
        void destroyTexture(MyGUI::ITexture* texture) override;
        /** @see RenderManager::getTexture */
        MyGUI::ITexture* getTexture(const std::string& name) override;

        /// Makes a texture that is drawn by the engine (and not loaded from a file) known by `name`, so that a widget
        /// can show it by that name. A name that is taken is given to the new texture; the entry stays for the whole
        /// run (a widget may still point at it), so a screen that shows a texture of the same name again finds it.
        MyGUI::ITexture* registerTexture(const std::string& name, osg::Texture2D* texture, osg::StateSet* injectState);

        // Called by the update traversal
        void update();

        // Called by the cull traversal
        /** @see IRenderTarget::begin */
        void begin() override;
        /** @see IRenderTarget::end */
        void end() override;
        /** @see IRenderTarget::doRender */
        void doRender(MyGUI::IVertexBuffer* buffer, MyGUI::ITexture* texture, size_t count) override;

        /** specify a StateSet to inject for rendering. The StateSet will be used by future doRender calls until you
         * reset it to nullptr again. */
        void setInjectState(osg::StateSet* stateSet);

        /** @see IRenderTarget::getInfo */
        const MyGUI::RenderTargetInfo& getInfo() const override { return mInfo; }

        void setViewSize(int width, int height) override;

        /// Interface units to drawable pixels; takes effect at the next setViewSize
        void setScalingFactor(float scalingFactor);

        void registerShader(const std::string& shaderName, const std::string& vertexProgramFile,
            const std::string& fragmentProgramFile) override;

        /*internal:*/

        void collectDrawCalls();
    };

}

#endif

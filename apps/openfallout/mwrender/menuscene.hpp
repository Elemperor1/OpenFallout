#ifndef OPENFALLOUT_APPS_OPENFALLOUT_MWRENDER_MENUSCENE_H
#define OPENFALLOUT_APPS_OPENFALLOUT_MWRENDER_MENUSCENE_H

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <osg/Matrixf>
#include <osg/Vec3f>
#include <osg/Vec4f>
#include <osg/ref_ptr>

#include <components/vfs/pathutil.hpp>

namespace osg
{
    class Group;
    class MatrixTransform;
    class Node;
    class StateSet;
    class Texture2D;
}

namespace Resource
{
    class ResourceSystem;
}

namespace SceneUtil
{
    class Light;
}

namespace OFRender
{
    class MenuSceneRTTNode;

    /// The matrix of a place in a scene from the numbers of the game's files: a translation, a rotation as the angles
    /// in degrees of the file (R = Rz(z) * Ry(y) * Rx(x) on a column vector: about X first, then Y, then Z, all about
    /// the axes of the parent) and one scale. A point `p` of the model is at `p * placement(...)` (OSG order).
    osg::Matrixf placement(const osg::Vec3f& translation, const osg::Vec3f& eulerDegrees, float scale = 1.f);

    /// The same from a rotation as the nine numbers of a 3 by 3 matrix, row by row, that turns a column vector (the way
    /// the game's code writes a rotation: the first row gives the x of the turned point)
    osg::Matrixf placementFromRows(const osg::Vec3f& translation, const std::array<float, 9>& rows, float scale = 1.f);

    /// The projection with its picture turned upside down: a point above the axis of the camera lands below the middle
    /// of the picture. A texture that was drawn into has its first row at the bottom, and a widget reads the first row
    /// of a texture as its top, so a scene is drawn upside down to stand upright on the screen (a mirrored projection
    /// turns the order of the corners of a face round: the scene draws the faces that look the other way round as its
    /// front).
    osg::Matrixf upsideDown(const osg::Matrixf& projection);

    /// A few models of the game drawn to a texture that a menu shows: the lock and pick of the lockpicking screen,
    /// the model behind the pause menu, the item of the Pip-Boy. The game draws these inside its menus with a camera
    /// and lights of the menu's own; here the scene has the same parts (a frustum, a place and direction for the
    /// camera, one ambient and one directional light, models with a transform and, for a model that has sequences, the
    /// time of one of them) and the picture is a texture that a menu names.
    ///
    /// All members are for the main thread. The scene is drawn again on every frame while it exists.
    class MenuScene
    {
    public:
        /// Where in the scene graph the scene is drawn from (the root of the viewer) and how big its texture is.
        MenuScene(osg::Group* parent, Resource::ResourceSystem* resourceSystem, int width, int height);
        ~MenuScene();

        MenuScene(const MenuScene&) = delete;
        MenuScene& operator=(const MenuScene&) = delete;

        int width() const { return mWidth; }
        int height() const { return mHeight; }

        osg::ref_ptr<osg::Texture2D> getTexture();
        /// The state a widget that shows the texture needs (the texture has premultiplied alpha)
        osg::StateSet* getTextureStateSet() { return mTextureStateSet; }

        /// The planes of the frustum on the near plane, as the game's cameras store them (units of the scene)
        void setFrustum(float left, float right, float bottom, float top, float nearPlane, float farPlane);
        /// The camera stands at `eye` and looks along `forward`; `up` is the direction that is up in the picture
        void setCamera(const osg::Vec3f& eye, const osg::Vec3f& forward, const osg::Vec3f& up);
        /// `direction` is the way the light travels (from the lamp to the models)
        void setLight(const osg::Vec3f& ambient, const osg::Vec3f& diffuse, const osg::Vec3f& direction);
        /// The colour and alpha that the texture is cleared to before the models are drawn
        void setClearColour(const osg::Vec4f& colour);

        /// Loads the model under `name` (a name that is taken is replaced). False when the model cannot be loaded.
        /// `keepShapes` keeps the optimizer from merging the shapes of the file or folding its nodes into one another, so
        /// that a script can name each of them (the file must not have been loaded before).
        bool addModel(const std::string& name, VFS::Path::NormalizedView path, bool keepShapes = false);
        void removeModel(const std::string& name);
        bool hasModel(const std::string& name) const;

        /// The model's place in the scene: translation, rotation and scale as one matrix
        void setTransform(const std::string& name, const osg::Matrixf& transform);
        /// The place of all the models together (the game puts its scene under a node of its own, turned so that the
        /// x of a model is to the right in the picture, its z up and its y away from the camera)
        void setSceneTransform(const osg::Matrixf& transform);
        /// A point light that is a child of the node `node` of the model (the root of the model when the name is
        /// empty or not found) at `offset` from it, with no fall-off, as the lights of the game's files are
        bool addLight(const std::string& name, const std::string& node, const osg::Vec3f& offset,
            const osg::Vec3f& diffuse, float radius);
        void setVisible(const std::string& name, bool visible);

        /// Puts the picture of the file `path` on the shape `shape` of the model (the node of the file that holds the
        /// shape, by its name) in place of the diffuse picture it has: the faces of the reels of the slot machine are
        /// given the pictures that the casino's record names. Only that shape changes (the picture of the other
        /// shapes that shared its state stays). False when the model or the shape is not there or has no picture.
        bool setTexture(const std::string& name, const std::string& shape, VFS::Path::NormalizedView path);

        /// Shows or hides the shape of the model (the node of the file, by its name; the shapes under it go with it).
        /// False when the model or the shape is not there.
        bool setShapeVisible(const std::string& name, const std::string& shape, bool visible);

        /// The shape of the picture under the point (`x` across and `y` down, from 0 to 1 over the whole texture): the
        /// first of `shapes` (names of nodes of the models) that the ray from the camera through the point meets, nearest
        /// first, in the form it was given; an empty text when it meets none. The shapes that are hidden are not met.
        std::string pick(float x, float y, const std::vector<std::string>& shapes) const;

        struct Sequence
        {
            std::string mName;
            float mStart = 0.f;
            float mStop = 0.f;
        };
        /// The sequences that the file of the model holds (the NiControllerSequences of its manager), with the times
        /// between which each runs
        std::vector<Sequence> sequences(const std::string& name) const;
        /// Puts the model at `time` (seconds from the start of the sequence, held at its ends) of one of its sequences.
        /// False when the model or the sequence is not there.
        bool setSequenceTime(const std::string& name, const std::string& sequence, float time);

        /// Takes the scene out of the graph: the texture keeps its last picture
        void stop();

    private:
        struct Model;

        Model* find(const std::string& name);
        const Model* find(const std::string& name) const;

        osg::ref_ptr<osg::Group> mParent;
        Resource::ResourceSystem* mResourceSystem;
        int mWidth;
        int mHeight;
        osg::ref_ptr<osg::StateSet> mTextureStateSet;
        osg::ref_ptr<MenuSceneRTTNode> mRTTNode;
        osg::ref_ptr<SceneUtil::Light> mLight;
        osg::ref_ptr<osg::MatrixTransform> mModels;
        std::map<std::string, std::unique_ptr<Model>> mModelsByName;
        bool mStopped = false;
    };
}

#endif

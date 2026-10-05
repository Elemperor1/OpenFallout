#ifndef GAME_MWCLASS_ACTIVATOR_H
#define GAME_MWCLASS_ACTIVATOR_H

#include "../mwworld/registeredclass.hpp"

namespace OFClass
{
    class Activator final : public OFWorld::RegisteredClass<Activator>
    {
        friend OFWorld::RegisteredClass<Activator>;

        Activator();

        OFWorld::Ptr copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const override;

        static int getSndGenTypeFromName(std::string_view name);

    public:
        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override;
        ///< Add reference into a cell for rendering

        void insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override;

        void insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override;

        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;
        ///< \return name or ID; can return an empty string.

        bool hasToolTip(const OFWorld::ConstPtr& ptr) const override;
        ///< @return true if this object has a tooltip when focused (default implementation: true)

        OFGui::ToolTipInfo getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const override;
        ///< @return the content of the tool tip to be displayed. raises exception if the object has no tooltip.

        ESM::RefId getScript(const OFWorld::ConstPtr& ptr) const override;
        ///< Return name of the script attached to ptr

        std::string_view getWerewolfRefusalSoundId() const override { return "WolfActivator"; }

        std::unique_ptr<OFWorld::Action> activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const override;
        ///< Generate action for activation

        VFS::Path::NormalizedView getModel(const OFWorld::ConstPtr& ptr) const override;

        bool useAnim() const override;
        ///< Whether or not to use animated variant of model (default false)

        bool isActivator() const override;

        ESM::RefId getSoundIdFromSndGen(const OFWorld::Ptr& ptr, std::string_view name) const override;
    };
}

#endif

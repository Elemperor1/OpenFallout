#ifndef GAME_MWCLASS_ESM4CREATURE_H
#define GAME_MWCLASS_ESM4CREATURE_H

#include <components/esm4/creaturemodel.hpp>
#include <components/esm4/loadcrea.hpp>

#include "../mwgui/tooltips.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/registeredclass.hpp"

#include "esm4base.hpp"

namespace OFClass
{
    class ESM4CreatureCustomData;

    /// The half extents of the solid body of a creature in game units, from the box of its record: a cylinder as wide
    /// as the smaller side of the box and as tall as the box, kept within what the data has meant. A creature without a
    /// box is as big as a person.
    osg::Vec3f creatureBodyHalfExtents(const ESM4::Creature& creature);

    /// A creature of Fallout 3 or New Vegas (CREA). What it looks like and is called comes from its record and, for
    /// the many that take them from a template, the records of its templates (`ESM4::creatureModel`). The skeleton is
    /// its model; the models that hang on it and its animations are found by the rendering.
    class ESM4Creature final : public OFWorld::RegisteredClass<ESM4Creature, ESM4Base<ESM4::Creature>>
    {
        friend OFWorld::RegisteredClass<ESM4Creature, ESM4Base<ESM4::Creature>>;
        ESM4Creature()
            : OFWorld::RegisteredClass<ESM4Creature, ESM4Base<ESM4::Creature>>(ESM4::Creature::sRecordId)
        {
        }

    public:
        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override;

        void insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override;

        bool hasToolTip(const OFWorld::ConstPtr& ptr) const override { return !getName(ptr).empty(); }
        OFGui::ToolTipInfo getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const override
        {
            return ESM4Impl::getToolTipInfo(getName(ptr), count);
        }

        VFS::Path::NormalizedView getModel(const OFWorld::ConstPtr& ptr) const override;
        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;

        /// The skeleton, body models and animation files of the creature, found through its templates
        static const ESM4::CreatureModel& getCreatureModel(const OFWorld::Ptr& ptr);

    private:
        static ESM4CreatureCustomData& getCustomData(const OFWorld::ConstPtr& ptr);
    };
}

#endif // GAME_MWCLASS_ESM4CREATURE_H

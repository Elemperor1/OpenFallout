#ifndef GAME_MWCLASS_CREATURELEVLIST_H
#define GAME_MWCLASS_CREATURELEVLIST_H

#include "../mwworld/registeredclass.hpp"

namespace OFClass
{
    class CreatureLevList : public OFWorld::RegisteredClass<CreatureLevList>
    {
        friend OFWorld::RegisteredClass<CreatureLevList>;

        CreatureLevList();

        void ensureCustomData(const OFWorld::Ptr& ptr) const;

    public:
        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;
        ///< \return name or ID; can return an empty string.

        bool hasToolTip(const OFWorld::ConstPtr& ptr) const override;
        ///< @return true if this object has a tooltip when focused (default implementation: true)

        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override;
        ///< Add reference into a cell for rendering

        void readAdditionalState(const OFWorld::Ptr& ptr, const ESM::ObjectState& state) const override;
        ///< Read additional state from \a state into \a ptr.

        void writeAdditionalState(const OFWorld::ConstPtr& ptr, ESM::ObjectState& state) const override;
        ///< Write additional state from \a ptr into \a state.

        void respawn(const OFWorld::Ptr& ptr) const override;

        OFWorld::Ptr copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const override;

        void adjustPosition(const OFWorld::Ptr& ptr, bool force) const override;
    };
}

#endif

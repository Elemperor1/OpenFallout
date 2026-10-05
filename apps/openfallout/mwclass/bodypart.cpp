#include "bodypart.hpp"

#include <components/esm3/loadbody.hpp>

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "../mwworld/cellstore.hpp"

#include "classmodel.hpp"

namespace OFClass
{
    BodyPart::BodyPart()
        : OFWorld::RegisteredClass<BodyPart>(ESM::BodyPart::sRecordId)
    {
    }

    OFWorld::Ptr BodyPart::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::BodyPart>* ref = ptr.get<ESM::BodyPart>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }

    void BodyPart::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
        }
    }

    std::string_view BodyPart::getName(const OFWorld::ConstPtr& ptr) const
    {
        return {};
    }

    bool BodyPart::hasToolTip(const OFWorld::ConstPtr& ptr) const
    {
        return false;
    }

    VFS::Path::NormalizedView BodyPart::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::BodyPart>(ptr);
    }

}

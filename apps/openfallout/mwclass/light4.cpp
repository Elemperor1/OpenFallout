#include "light4.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwworld/ptr.hpp"

#include <components/esm4/loadligh.hpp>

namespace OFClass
{
    ESM4Light::ESM4Light()
        : OFWorld::RegisteredClass<ESM4Light, ESM4Base<ESM4::Light>>(ESM4::Light::sRecordId)
    {
    }

    void ESM4Light ::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        OFWorld::LiveCellRef<ESM4::Light>* ref = ptr.get<ESM4::Light>();

        // Insert even if model is empty, so that the light is added
        renderingInterface.getObjects().insertModel(ptr, model, !(ref->mBase->mData.flags & ESM4::Light::OffDefault));
    }
}

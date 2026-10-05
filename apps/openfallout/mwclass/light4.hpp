#ifndef OPENW_MWCLASS_LIGHT4
#define OPENW_MWCLASS_LIGHT4

#include "../mwworld/registeredclass.hpp"

#include "esm4base.hpp"

namespace OFClass
{
    class ESM4Light : public OFWorld::RegisteredClass<ESM4Light, ESM4Base<ESM4::Light>>
    {
        friend OFWorld::RegisteredClass<ESM4Light, ESM4Base<ESM4::Light>>;

        ESM4Light();

    public:
        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override;
        ///< Add reference into a cell for rendering
    };
}
#endif

#ifndef OPENFALLOUT_COMPONENTS_NIF_PARENT_HPP
#define OPENFALLOUT_COMPONENTS_NIF_PARENT_HPP

namespace Nif
{
    struct NiNode;

    struct Parent
    {
        const NiNode& mNiNode;
        const Parent* mParent;
    };
}

#endif

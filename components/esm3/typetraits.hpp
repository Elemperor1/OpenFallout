#ifndef OPENFALLOUT_COMPONENTS_ESM3_TYPETRAITS
#define OPENFALLOUT_COMPONENTS_ESM3_TYPETRAITS

namespace ESM
{
    template <class T>
    concept HasIndex = requires
    {
        T::mIndex;
    };

    template <class T>
    concept HasStringId = requires
    {
        T::mStringId;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM3_TYPETRAITS

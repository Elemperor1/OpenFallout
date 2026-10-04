#ifndef OPENFALLOUT_COMPONENTS_ESM_TYPETRAITS
#define OPENFALLOUT_COMPONENTS_ESM_TYPETRAITS

namespace ESM
{
    template <class T>
    concept HasId = requires
    {
        T::mId;
    };

    template <class T>
    concept HasModel = requires
    {
        T::mModel;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM_TYPETRAITS

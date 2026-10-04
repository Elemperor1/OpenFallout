#ifndef OPENFALLOUT_COMPONENTS_SETTINGS_SANITIZER_H
#define OPENFALLOUT_COMPONENTS_SETTINGS_SANITIZER_H

namespace Settings
{
    template <class T>
    struct Sanitizer
    {
        virtual ~Sanitizer() = default;

        virtual T apply(const T& value) const = 0;
    };
}

#endif

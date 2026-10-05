#ifndef OPENFALLOUT_COMPONENTS_ESM_FOURCC_H
#define OPENFALLOUT_COMPONENTS_ESM_FOURCC_H

namespace ESM
{
    inline constexpr unsigned int fourCC(const char (&name)[5])
    {
        return static_cast<unsigned char>(name[0]) | (static_cast<unsigned char>(name[1]) << 8)
            | (static_cast<unsigned char>(name[2]) << 16) | (static_cast<unsigned char>(name[3]) << 24);
    }
}

#endif // OPENFALLOUT_COMPONENTS_ESM_FOURCC_H

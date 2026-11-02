#ifndef OPENFALLOUT_COMPONENTS_FALLOUTMENU_ATLAS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTMENU_ATLAS_H

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace FalloutMenu
{
    /// A texture atlas of the games: a `.tai` file lists which part of one big texture each small texture is cut from
    class Atlas
    {
    public:
        struct Entry
        {
            /// The file of the big texture, as the list gives it (`InterfaceShared0.dds`)
            std::string mTexture;
            /// The part of it, from 0 to 1: the corner and the size
            float mU = 0;
            float mV = 0;
            float mWidth = 0;
            float mHeight = 0;
        };

        static Atlas parse(std::string_view text);

        /// The part for a texture of the original name (a path is cut down to its last part, the case does not matter)
        const Entry* find(std::string_view name) const;
        std::size_t size() const { return mEntries.size(); }

    private:
        std::unordered_map<std::string, Entry> mEntries;
    };
}

#endif

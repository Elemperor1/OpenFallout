#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HOTKEYS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HOTKEYS_H

#include <array>
#include <cstdint>
#include <mutex>

#include "saveaccess.hpp"
#include "savestate.hpp"
#include "script.hpp"

namespace FalloutScript
{
    /// The items that the player has put on the number keys: the eight slots of the hot key wheel of the games
    /// (FalloutNV.exe, class HotKeyWheel 0x701610). The second slot is not one of them: the wheel shows the ammunition
    /// swap there and the key of it changes the ammunition of the weapon in hand. It can be used from any thread.
    class HotKeys
    {
        template <class T>
        friend struct SaveAccess;

    public:
        static constexpr int slotCount = 8;
        static constexpr int ammoSwapSlot = 1;

        /// Whether the slot takes an item (a slot that exists and is not the ammunition swap)
        static bool assignable(int slot);

        /// The item in the slot (counted from 0), 0 for a slot that is empty or does not exist
        FormId item(int slot) const;
        /// Puts the item in the slot. An item has one slot: it leaves the one it was in. False for a slot that takes
        /// none or for no item.
        bool assign(int slot, FormId item);
        /// Empties the slot
        void clear(int slot);
        /// Takes the item out of the slot that holds it
        void clearItem(FormId item);
        /// The slot that holds the item, -1 when there is none
        int slotOf(FormId item) const;
        /// Empties all the slots (a new game or one that is loaded)
        void reset();

    private:
        mutable std::mutex mMutex;
        std::array<FormId, slotCount> mSlots{};
    };

    constexpr std::uint32_t hotKeysSectionVersion = 1;
    void writeHotKeys(HotKeys& keys, SaveWriter& out);
    void readHotKeys(HotKeys& keys, SaveReader& in, std::uint32_t version);
}

#endif

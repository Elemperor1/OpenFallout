#include "hotkeys.hpp"

#include <algorithm>

namespace FalloutScript
{
    bool HotKeys::assignable(int slot)
    {
        return slot >= 0 && slot < slotCount && slot != ammoSwapSlot;
    }

    FormId HotKeys::item(int slot) const
    {
        if (slot < 0 || slot >= slotCount)
            return 0;
        const std::lock_guard lock(mMutex);
        return mSlots[static_cast<std::size_t>(slot)];
    }

    bool HotKeys::assign(int slot, FormId item)
    {
        if (!assignable(slot) || item == 0)
            return false;
        const std::lock_guard lock(mMutex);
        for (FormId& other : mSlots)
            if (other == item)
                other = 0;
        mSlots[static_cast<std::size_t>(slot)] = item;
        return true;
    }

    void HotKeys::clear(int slot)
    {
        if (slot < 0 || slot >= slotCount)
            return;
        const std::lock_guard lock(mMutex);
        mSlots[static_cast<std::size_t>(slot)] = 0;
    }

    void HotKeys::clearItem(FormId item)
    {
        if (item == 0)
            return;
        const std::lock_guard lock(mMutex);
        for (FormId& other : mSlots)
            if (other == item)
                other = 0;
    }

    int HotKeys::slotOf(FormId item) const
    {
        if (item == 0)
            return -1;
        const std::lock_guard lock(mMutex);
        const auto found = std::find(mSlots.begin(), mSlots.end(), item);
        return found == mSlots.end() ? -1 : static_cast<int>(found - mSlots.begin());
    }

    void HotKeys::reset()
    {
        const std::lock_guard lock(mMutex);
        mSlots.fill(0);
    }

    template <>
    struct SaveAccess<HotKeys>
    {
        static void write(HotKeys& keys, SaveWriter& out)
        {
            const std::lock_guard lock(keys.mMutex);
            out.count(keys.mSlots.size());
            for (const FormId item : keys.mSlots)
                out.form(item);
        }

        static void read(HotKeys& keys, SaveReader& in, std::uint32_t)
        {
            std::array<FormId, HotKeys::slotCount> slots{};
            const std::size_t count = in.count(4);
            for (std::size_t i = 0; i < count; ++i)
            {
                // A form of a plugin that is no longer loaded leaves its slot empty
                const std::optional<FormId> item = in.form();
                if (item && i < slots.size() && HotKeys::assignable(static_cast<int>(i)))
                    slots[i] = *item;
            }
            const std::lock_guard lock(keys.mMutex);
            keys.mSlots = slots;
        }
    };

    void writeHotKeys(HotKeys& keys, SaveWriter& out)
    {
        SaveAccess<HotKeys>::write(keys, out);
    }

    void readHotKeys(HotKeys& keys, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<HotKeys>::read(keys, in, version);
    }
}

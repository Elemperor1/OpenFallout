#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_EQUIPMENT_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_EQUIPMENT_H

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <vector>

#include "saveaccess.hpp"
#include "script.hpp"

namespace FalloutScript
{
    class ActorState;
    class Interpreter;

    /// What an item is for a character who carries it
    enum class ItemKind : std::uint8_t
    {
        Other, // not worn or held: miscellaneous items, books, keys, notes
        Weapon,
        Armor, // armor and clothing
        Ammo,
        Aid, // food, drink, chems: used up, not worn
    };

    /// What the records say about an item
    struct ItemInfo
    {
        ItemKind mKind = ItemKind::Other;
        /// Armor: the parts of the body it covers (the biped flags of the record)
        std::uint32_t mSlots = 0;
        /// Weapon: how it is held (the animation type of the record)
        std::uint32_t mAnimationType = 0;
        /// Armor: the rating (what the games show) and the damage threshold (New Vegas)
        double mRating = 0;
        double mThreshold = 0;
        /// What one of the item weighs
        double mWeight = 0;
    };

    /// The biped flags that armor covers a body part with that the rest of this component has to know by name
    namespace BipedSlot
    {
        constexpr std::uint32_t head = 0x00000001;
        constexpr std::uint32_t hair = 0x00000002;
        constexpr std::uint32_t upperBody = 0x00000004;
        constexpr std::uint32_t leftHand = 0x00000008;
        constexpr std::uint32_t rightHand = 0x00000010;
        constexpr std::uint32_t weapon = 0x00000020;
    }

    /// Where the game finds out what an item is
    class ItemRecords
    {
    public:
        virtual ~ItemRecords() = default;

        /// What the item is. False when the form is no item the host knows (it counts as nothing that can be worn).
        virtual bool itemInfo(FormId item, ItemInfo& info) = 0;

        /// The armor a character wears when it first appears in the world (its record lists it, or a list that the
        /// record names gives it). False when the host can not tell yet, and it is asked again; by default a
        /// character wears nothing until a script puts something on it.
        virtual bool startingArmor(FormId actor, std::vector<FormId>& armor)
        {
            (void)actor;
            (void)armor;
            return true;
        }
    };

    /// What characters hold and wear: one weapon and its ammunition, and any armor whose body parts do not overlap. An
    /// item is held or worn only as long as the character has it (ActorState has the items), so that an item that a
    /// script removed, or that was given away, is off the character without anyone telling this. It can be used from
    /// any thread.
    class Equipment
    {
        template <class T>
        friend struct SaveAccess;

    public:
        Equipment(ItemRecords& records, ActorState& state);

        /// Forgets everything, as for a new game
        void clear();

        enum class Outcome
        {
            Equipped,
            AlreadyEquipped,
            NotCarried, // the character does not have the item
            NotEquippable, // no weapon, armor or ammunition
        };

        /// Puts the item on. A weapon takes the place of the weapon held, armor the place of the armor that covers
        /// any of the same body parts, and ammunition the place of the ammunition chosen.
        Outcome equip(FormId actor, FormId item);
        /// Takes the item off; false when it was not on
        bool unequip(FormId actor, FormId item);
        bool isEquipped(FormId actor, FormId item);

        /// The weapon held (0 for none, hands only), the ammunition chosen, and the armor worn
        FormId weapon(FormId actor);
        FormId ammo(FormId actor);
        std::vector<FormId> armor(FormId actor);

        /// The animation type of the weapon held (0, hands, for none)
        std::uint32_t weaponAnimationType(FormId actor);

        /// The rating and damage threshold of all that is worn, and of what covers the upper body
        double armorRating(FormId actor);
        double damageThreshold(FormId actor);
        double upperBodyRating(FormId actor);

        /// Whether the weapon is out of its holster
        bool weaponDrawn(FormId actor);
        void setWeaponDrawn(FormId actor, bool drawn);

        /// What everything the character carries weighs; false when the host can not tell yet what it carries
        bool inventoryWeight(FormId actor, double& weight);

        /// An item that was put on or taken off
        struct WornChange
        {
            FormId mActor = 0;
            FormId mItem = 0;
            bool mWorn = false;
        };
        /// The changes since this was last called (at most 1024 are kept), for the events of the items (OnEquip,
        /// OnUnequip), which the game raises when it is ready for scripts to run
        std::vector<WornChange> takeChanges();

    private:
        struct Worn
        {
            FormId mWeapon = 0;
            FormId mAmmo = 0;
            std::vector<FormId> mArmor; // in the order it was put on
            bool mDrawn = false;
            bool mSeeded = false; // what the character wears at the start is in mArmor
        };

        /// What the actor holds and wears; a character that is looked at for the first time wears what its record says
        /// (the changes this makes are no events: the character was dressed from the start). The caller holds the lock.
        Worn& wornOf(FormId actor);

        /// Takes off what the actor no longer has, and notes it. The caller holds the lock.
        void tidy(FormId actor, Worn& worn);

        /// Puts the armor on in the place of what covers any of the same parts of the body. The caller holds the lock.
        void wearArmor(FormId actor, Worn& worn, FormId item, const ItemInfo& info, bool note);

        void noteChange(FormId actor, FormId item, bool worn);

        ItemRecords& mRecords;
        ActorState& mState;
        mutable std::mutex mMutex;
        std::map<FormId, Worn> mWorn;
        std::vector<WornChange> mChanges;
    };

    /// Gives the commands about what characters hold and wear to the interpreter: EquipItem, UnequipItem, GetEquipped,
    /// IsWeaponOut, GetWeaponAnimType, GetArmorRating and GetArmorRatingUpperBody. Those that the command table does
    /// not have are left out.
    void addEquipmentCommands(Interpreter& interpreter, Equipment& equipment);
}

#endif

#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_EQUIPMENT_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_EQUIPMENT_H

#include <cstdint>
#include <functional>
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
        /// Weapons and armor: the most health a copy has (the record's health; 0 for an item that does not wear out)
        double mHealth = 0;
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
            Broken, // the copy has no health left (the executable's EquipItem refuses it: sCantEquipBrokenItem)
        };

        /// Puts the item on. A weapon takes the place of the weapon held, armor the place of the armor that covers
        /// any of the same body parts, and ammunition the place of the ammunition chosen. `condition` says which copy of
        /// the item goes on (the condition it has, as ActorState counts it); a negative one is the best copy. A copy of
        /// the item that is on already is put in place of by the other copy (it is not a change of what is worn).
        Outcome equip(FormId actor, FormId item, int condition = -1);
        /// Takes the item off; false when it was not on
        bool unequip(FormId actor, FormId item);
        bool isEquipped(FormId actor, FormId item);
        /// Whether that copy of the item (the one at that condition) is the one on
        bool isEquipped(FormId actor, FormId item, int condition);
        /// The condition of the copy that is on, -1 when the item is not on. Ammunition has none: it is full.
        int equippedCondition(FormId actor, FormId item);
        /// The copy of the item that is on wears out or is mended to that condition (the copy in ActorState goes with
        /// it). False when the item is not on.
        bool setEquippedCondition(FormId actor, FormId item, int condition);

        /// The copy of the weapon or armor that is on loses `points` of health (what is less than a tenth of a percent is
        /// kept for the next time) and tells the listener when it did. Returns the condition after, -1 when the item is
        /// not on or has no health to lose.
        int wear(FormId actor, FormId item, double points);
        /// Told after an item that is on lost health: the condition before and after
        using WearListener = std::function<void(FormId actor, FormId item, int before, int after)>;
        void setWearListener(WearListener listener);
        /// Gives copies of an item with a share of its health (1 is whole); an item that does not wear out is whole
        void addWithHealth(FormId actor, FormId item, int count, double fraction);
        /// The health of the weapon in the hands in percent, 100 for the hands
        double weaponPercent(FormId actor);
        /// What SetWeaponHealthPerc and ModWeaponHealthPerc do to the weapon in the hands (0xA20..: the executable lowers
        /// health through the code of wear, raises it by writing it, and a set never reaches the whole): false when there
        /// is no weapon in the hands
        bool setWeaponPercent(FormId actor, double percent);
        bool modWeaponPercent(FormId actor, double percent);

        /// The weapon held (0 for none, hands only), the ammunition chosen, and the armor worn
        FormId weapon(FormId actor);
        FormId ammo(FormId actor);
        std::vector<FormId> armor(FormId actor);

        /// The armor worn with the condition of the copy that is on, in the order it was put on
        struct WornPiece
        {
            FormId mItem = 0;
            int mCondition = 1000;
        };
        std::vector<WornPiece> wornArmor(FormId actor);

        /// The animation type of the weapon held (0, hands, for none)
        std::uint32_t weaponAnimationType(FormId actor);

        /// The rating and damage threshold of all that is worn, and of what covers the upper body. A piece that is worn
        /// below half its health gives less (`Wear::armorFactor`).
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
            /// The condition of the copies that are on (a copy of full condition is not listed for armor)
            int mWeaponCondition = 1000;
            std::map<FormId, int> mArmorConditions;
            bool mDrawn = false;
            bool mSeeded = false; // what the character wears at the start is in mArmor
        };

        /// What the actor holds and wears; a character that is looked at for the first time wears what its record says
        /// (the changes this makes are no events: the character was dressed from the start). The caller holds the lock.
        Worn& wornOf(FormId actor);

        /// Takes off what the actor no longer has, and notes it. The caller holds the lock.
        void tidy(FormId actor, Worn& worn);

        /// Puts the armor on in the place of what covers any of the same parts of the body. The caller holds the lock.
        void wearArmor(FormId actor, Worn& worn, FormId item, const ItemInfo& info, bool note, int condition);
        /// The condition that the copy of the item that is on has, in the copies that the actor holds. The caller holds
        /// the lock.
        int conditionOn(const Worn& worn, FormId item) const;
        /// Which copy: the condition asked for if the actor has a copy like that, else the best (-1 for no copy).
        int chooseCopy(FormId actor, FormId item, int asked);

        void noteChange(FormId actor, FormId item, bool worn);

        /// What a quantity of each piece of armor is, with the piece's condition in it, added up
        template <class Pick>
        double sumOfWornArmor(FormId actor, Pick pick);

        /// How much of the next tenth of a percent a copy has lost, for the next time
        std::map<std::pair<FormId, FormId>, double> mWearRemainders;
        WearListener mWearListener;

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

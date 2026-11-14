#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PLACEDEXPLOSIVES_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PLACEDEXPLOSIVES_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include "savestate.hpp"

namespace FalloutScript
{
    /// Mines and the explosives that are laid and set off later. All numbers come from FalloutNV.exe (the addresses are
    /// given where a rule is used); the settings are those of the files, the executable's own defaults where the files
    /// have none.
    ///
    /// A laid explosive is a projectile (a reference of the world, the class that the executable calls GrenadeProjectile)
    /// of a projectile record (PROJ) whose data say what it is:
    ///  - "alt trigger" (flag 4) with a proximity above 0 is a proximity mine; with no proximity it counts the record's
    ///    timer down at once (a timed charge);
    ///  - "detonates" (flag 0x400) is an explosive that goes off when a script says so (DetonatePlacedExplosives);
    ///  - "can be disabled" (flag 0x20) is what DisableAllMines and a disarm can switch off;
    ///  - what explodes is the record's explosion (EXPL).
    namespace Explosives
    {
        using Lookup = std::function<std::optional<double>(std::string_view)>;

        struct Rules
        {
            /// fMineExteriorRadiusMult: what a mine's trigger radius is multiplied by outside (0x9C39E0)
            double mExteriorRadiusMult = 1.6;
            /// fMinesDelayMin: the least seconds between a mine being triggered and exploding (0x9C3D1C)
            double mDelayMin = 0.2;
            /// fMineAgeMax (the executable's own default is 0, which is no limit) and fGrenadeAgeMax (90): the seconds
            /// after which a laid projectile is taken away (0x9B4342, 0x9B4369, 0x9B437E)
            double mMineAgeMax = 0.0;
            double mGrenadeAgeMax = 90.0;
            /// iProjectileMineShooterCanTrigger above 0: the one who laid the mine can set it off too (0x9C3930)
            bool mShooterCanTrigger = true;
            /// iMineDisarmExperience: the experience a disarm gives the player (0x9C4587)
            int mDisarmExperience = 5;
        };
        Rules rules(const Lookup& setting);

        /// What the projectile record says (the flags of its DATA and two of its numbers)
        struct Kind
        {
            bool mAltTrigger = false;
            bool mDetonates = false;
            bool mCanBeDisabled = false;
            bool mHasExplosion = false;
            /// The proximity (+0x7C of the record, 0x45CD80) and the timer (+0x80, 0x644790), in units and seconds
            double mProximity = 0;
            double mFuseTimer = 0;
        };

        /// The bits of the PROJ flags the rules read
        constexpr std::uint16_t flagAltTrigger = 0x0004;
        constexpr std::uint16_t flagCanBeDisabled = 0x0020;
        constexpr std::uint16_t flagDetonates = 0x0400;

        /// Whether the projectile is a mine in the sense of DisableAllMines (the virtual function at +0x318 of a
        /// GrenadeProjectile, 0x9BDF80): the record detonates, or has the alt trigger and a proximity above 0.
        bool isMine(const Kind& kind);

        /// The seconds after which the laid projectile goes away (0 for never): the mine age for what is a mine, the
        /// grenade age for the rest (0x9B4312 to 0x9B4388).
        double ageLimit(const Rules& rules, const Kind& kind);

        /// The reaction of one side to another (the combat reaction of a faction): these two let a mine go off
        enum Reaction : int
        {
            Neutral = 0,
            Enemy = 1,
            Ally = 2,
            Friend = 3,
        };

        /// Whether the character sets the mine off when it is within its radius (0x9C3930): not the one who laid it unless
        /// the setting lets that be, not a character in the faction that the mine belongs to, and, when a character laid it,
        /// only one that the layer sees as neutral or an enemy (a mine with no owner is set off by anyone).
        struct Layer
        {
            std::uint32_t mActor = 0; // the character that laid it, 0 for none
            bool mFactionMember = false; // the character that comes is in the faction that owns the mine
            std::optional<int> mReaction; // the layer's reaction to the character that comes (none: nobody laid it)
        };
        bool triggers(const Rules& rules, std::uint32_t character, const Layer& layer);

        /// How long a mine counts down after it was set off: the least delay, and for the player the record's timer takes
        /// the share of the Explosives skill (0 to 100) on top (0x9C3D32 to 0x9C3D5E).
        double triggerDelay(const Rules& rules, const Kind& kind, bool player, double explosivesSkill);

        /// The run-time flags of a laid projectile (the word at +0xC8)
        constexpr std::uint32_t stateDisabled = 0x200; // set by a disarm
        constexpr std::uint32_t statePlayerSpared = 0x4000; // the player was checked once and the mine let it pass
        constexpr std::uint32_t stateDetonate = 0x100000; // set by DetonatePlacedExplosives

        /// The timer of a mine that has not been set off yet (the float maximum, 0x10231B0)
        constexpr double waiting = 3.4028234663852886e+38;

        struct Placed
        {
            std::uint32_t mId = 0; // the number the rules know it by
            std::uint32_t mOwner = 0; // the character that laid it
            std::uint32_t mProjectile = 0; // the form of the PROJ record
            Kind mKind;
            float mPosition[3] = { 0.f, 0.f, 0.f };
            std::uint32_t mCell = 0; // the cell it lies in (the form), 0 outside any cell
            bool mExterior = true;
            double mAge = 0;
            double mTimer = waiting;
            std::uint32_t mState = 0;
            /// The character that set it off, once someone did (0 until then, and for a charge that counts on its own)
            std::uint32_t mCause = 0;
            /// Laid by an attack that hits and does no damage (UseWeapon with the flag): its blast does none either
            bool mNoDamage = false;
            /// The weapon (WEAP form) of the attack that laid it, 0 for none: the hits and deaths that its blast causes
            /// name it, so that what is filtered by weapon (challenges, OnHitWith) sees them
            std::uint32_t mWeapon = 0;
        };

        /// What the world tells the rules about the characters near a mine (the host answers; the pure rules take the
        /// distances and the rolls)
        struct Surroundings
        {
            /// The first character other than the player within `radius` of the mine that sets it off, none for none
            std::function<std::optional<std::uint32_t>(const Placed&, double radius)> character;
            /// Whether the player is within `radius` of the mine and sets it off
            std::function<bool(const Placed&, double radius)> player;
            /// The chance in percent that the player sets a mine off that it is near (the perk's entry point 4 of 100)
            std::function<double(const Placed&)> playerChance;
            /// A whole number from 0 to 99 (the executable takes rand() % 100)
            std::function<int()> roll;
            /// The player's Explosives skill
            std::function<double()> skill;
        };

        struct Event
        {
            enum class Kind
            {
                Explode, // the projectile's explosion goes off where it lies; it is gone
                Expire, // its age ran out: it is gone with no explosion
            };
            Kind mKind = Kind::Explode;
            std::uint32_t mId = 0;
            /// The character that set it off (0 for none: a timer or a script)
            std::uint32_t mCause = 0;
            /// What the projectile was when it went (it is no longer laid after an event)
            Placed mPlaced;
        };

        class Laid
        {
        public:
            /// Lays a projectile; it counts a timed charge down from its record's timer, and a mine waits for someone.
            std::uint32_t plant(const Placed& placed);

            /// DisableAllMines (0x5DE110): every mine that can be disabled is switched off, with no sound and no
            /// experience (the executable asks each projectile to disable itself for the player, with the "silent" flag
            /// 1). Returns how many were.
            int disableAllMines();

            /// DetonatePlacedExplosives (0x5DDF90): the explosives that `owner` laid, whose record detonates, are told to
            /// go off at the next update. Returns how many were.
            int detonate(std::uint32_t owner);

            /// A disarm by a character (0x9C43E0): the projectile must be one that can be disabled and not disabled
            /// yet. The experience is the setting when it was not silent, the player did it, and the mine had not been set
            /// off yet or would be set off by the one who disarms it.
            struct Disarm
            {
                bool mDisabled = false;
                int mExperience = 0;
            };
            Disarm disable(const Rules& rules, std::uint32_t id, bool silent, bool byPlayer, bool wouldTrigger);

            /// One update of `seconds` (0x9C3190 and 0x9B41B0): grows every laid projectile older, takes it away when its
            /// age runs out, sets mines off and counts the charges down; the events are those that happened.
            std::vector<Event> update(const Rules& rules, double seconds, const Surroundings& around);

            const Placed* find(std::uint32_t id) const;
            const std::vector<Placed>& all() const { return mPlaced; }
            std::size_t count() const { return mPlaced.size(); }
            void clear();

            std::uint32_t next() const { return mNext; }
            /// What a save held
            void restore(std::vector<Placed> placed, std::uint32_t next);

        private:
            std::vector<Placed> mPlaced;
            std::uint32_t mNext = 1;
        };

        /// (version 2 adds the flag of a blast that does no damage; version 1 loads with it off. Version 3 adds the weapon of
        /// the attack that laid it; before it none is known.)
        constexpr std::uint32_t explosivesSectionVersion = 3;
        void writeExplosives(const Laid& laid, SaveWriter& out);
        void readExplosives(Laid& laid, SaveReader& in, std::uint32_t version);
    }
}

#endif

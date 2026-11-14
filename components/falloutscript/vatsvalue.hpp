#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_VATSVALUE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_VATSVALUE_H

#include <cstdint>
#include <functional>
#include <set>

#include "savestate.hpp"

namespace FalloutScript
{
    /// What the scripts and the conditions can ask of V.A.T.S. (GetVATSValue, FalloutNV.exe 0x594E40, a jump table of 18
    /// cases at 0x5952BC over the state of V.A.T.S. at 0x11F2250), and the rules of the commands that only read the
    /// targets (SetVATSTarget 0x5DAAE0, GetVatsTargetHeight 0x5A25F0, the free area and the visible target queries 0x8BD830
    /// and 0x8BDBD0).
    namespace VatsValue
    {
        /// The mode of V.A.T.S. (the word at +8 of the state): the executable reads the selection of the player in the
        /// modes 2 and 3 (the data of the screen) and the attack that is being played in the mode 4 (the first of the list
        /// of actions); in every other mode there is no data and every value is 0.
        enum Mode : int
        {
            None = 0,
            Selecting = 2,
            Playback = 4,
        };

        /// The one action that the values describe (the action record of the executable: +0 the kind, +4 the byte that
        /// says that the attack hits, +0xC the target reference, +0x10 the body part, and behind +0x14 the hit data with
        /// the weapon at +0x30, what it shoots at +0x50 and the flags at +0x58).
        struct Action
        {
            /// The kind of the action (+0). 0x16 is the executable's value for none, 0x12 an attack that the player queued
            /// on a target (written by the function at 0x9CA2C0 with the target and the body part), 0xF the action of the
            /// player itself.
            std::uint32_t mKind = 0x16;
            /// +4: that the attack hits (the record is made with it set; a missed shot clears it)
            bool mHits = true;
            /// +6 and +0x24, +7: the bytes behind the values 16 and 17
            bool mFlag6 = false;
            bool mFlag24 = false;
            bool mFlag7 = false;
            /// The target: whether there is one, and the record that its reference is of (+0xC, then the base form)
            bool mHasTarget = false;
            std::uint32_t mTargetBase = 0;
            /// The body part (+0x10): the actor value of its condition (25 to 31) or -1 for none; the record's own default
            /// is 0x4D
            std::int32_t mPart = -1;
            /// The weapon (+0x30 of the hit data) and its kind, the animation type of the record (the byte at +0xF4 of the
            /// weapon); with no weapon the kind is 0 (the hands)
            std::uint32_t mWeapon = 0;
            std::uint32_t mWeaponKind = 0;
            /// What the weapon shoots (+0x50 of the hit data)
            std::uint32_t mAmmo = 0;
            /// The flags of the hit data (+0x58); see Flag
            std::uint32_t mFlags = 0;
            /// How far the target is (units): the value 4
            double mDistance = 0;
            /// The target's reference (the host's own, for the commands that need the object; the values read the base)
            std::uint32_t mTargetReference = 0;
        };

        /// The bits of the hit data's flags that the values read: 4 is value 8, 0x10 value 11, 0x40 value 12, 0x20 value
        /// 13, 0x80 value 14. The names are what the camera paths of both games' files do with them (a path for a fatal
        /// hit asks 11, one for a limb that is gone 13, one for a crippled limb 14, the bloody mess paths 12 and 13); the
        /// executable only tests the bits.
        enum Flag : std::uint32_t
        {
            Flag4 = 0x04,
            Fatal = 0x10,
            LimbGone = 0x20,
            LimbBurst = 0x40,
            LimbCrippled = 0x80,
        };

        /// The data that makes values 1, 3 and 10 (is it in this form list) answerable
        using InList = std::function<bool(std::uint32_t list, std::uint32_t form)>;

        /// The value, 0 or 1 unless the value is a distance (4). `mode` is the mode of V.A.T.S., `action` the action of
        /// that mode (none: a null record, every answer 0), `index` 0 to 17 (any other is 0), `form` the second
        /// argument, a form, a form list or a number by the value. The cases, by the executable's jump table:
        ///  0 the weapon is that form; 1 the weapon is in that list; 2 the target's base is that form; 3 the target's base
        ///  is in that list; 4 the distance to the target; 5 the body part is that number; 6 the kind of action is that
        ///  number; 7 the attack hits; 8 flag 4; 9 what it shoots is that form; 10 what it shoots is in that list; 11 flag
        ///  0x10; 12 flag 0x40; 13 flag 0x20; 14 flag 0x80; 15 the kind of the weapon is that number (no weapon: 0 is
        ///  true only for a form of 0); 16 byte +6 or byte +0x24; 17 byte +7.
        double value(int mode, const Action* action, int index, std::uint32_t form, const InList& inList);

        /// Whether the mode has an action to read (the mode 4 or a selection)
        bool readsAction(int mode);

        /// SetVATSTarget (0x5DAAE0): the reference carries a mark (bit 0x04000000 of its flags) that turns the record's own
        /// answer round. `wanted` is what the script asks, `recordTargetable` the record's destruction data flag 1 ("VATS
        /// targetable", the byte at +5 of its header bit 0, read at 0x576100). The mark is set when the two differ and
        /// cleared when they agree.
        bool targetMark(bool wanted, bool recordTargetable);

        /// Whether the reference can be picked as a target when the record says `recordTargetable` and the reference has
        /// the mark.
        bool targetable(bool recordTargetable, bool marked);

        /// The areas around an actor (GetVATS{Front,Back,Left,Right}AreaFree, GetVATS...TargetVisible): the angle that is
        /// added to the actor's heading, in radians (0x101FF38 is 1.5708, 0x101FF34 4.71239, 0x102B3C8 3.14159).
        enum class Side
        {
            Front,
            Right,
            Back,
            Left,
        };
        double sideAngle(Side side);

        /// The distance that GetVATSValue's neighbours cast their rays over (0x1022958) and the answer when the ray meets
        /// nothing (the float maximum, 0x1016970).
        constexpr double areaRayLength = 10000.0;
        constexpr double nothingHit = 3.4028234663852886e+38;

        /// The free area: the ray's distance to the first thing that is not the actor, `nothingHit` when it meets none.
        /// `hit` is the fraction of the ray (0 to 1) at which the first obstacle is, negative for none.
        double freeArea(double hitFraction);

        /// The target-visible query steps the start of the ray out along the side, one step at a time (each fVATSSmartCamera
        /// CheckStepDistance long, at most fVATSSmartCameraCheckStepCount of them), and answers the distance of the last
        /// step from which the target is still seen; the first step that is blocked ends it. `seen(i)` tells for the step i
        /// (counting from 0) whether the target is seen from that step's point.
        double visibleDistance(double stepDistance, double stepCount, const std::function<bool(int step)>& seen);

        /// GetVatsTargetHeight (0x5A25F0): in the playback mode, for an action that is an attack, how far above the player's
        /// feet the part that is aimed at is (the world height of its node, or of the target when it has no such part,
        /// minus the player's height). 0 in every other case.
        double targetHeight(int mode, bool attackKind, bool hasTarget, double partOrTargetHeight, double playerHeight);


        /// The references that SetVATSTarget turned round (the executable keeps the mark in bit 0x04000000 of the flags of
        /// the reference, which the save of the world holds; the port keeps the set and saves it itself)
        class TargetMarks
        {
        public:
            bool marked(FormId reference) const { return mMarked.count(reference) != 0; }
            void set(FormId reference, bool mark);
            void clear() { mMarked.clear(); }
            const std::set<FormId>& all() const { return mMarked; }
            void restore(std::set<FormId> marked) { mMarked = std::move(marked); }

        private:
            std::set<FormId> mMarked;
        };
        constexpr std::uint32_t targetMarksSectionVersion = 1;
        void writeTargetMarks(const TargetMarks& marks, SaveWriter& out);
        void readTargetMarks(TargetMarks& marks, SaveReader& in, std::uint32_t version);

        /// GetThreatRatio (the formula of the executable's threat of one actor): its level times its health, divided by
        /// what the armor lets through (the share of a blow the damage threshold stops is the threshold over 30, at most
        /// 0.99). 0 for an actor that has no health or level.
        double threat(double level, double health, double damageThreshold);
        /// The threat of the other actor over the threat of the actor the command is called on (what the port takes the
        /// ratio to mean: 0 when the actor itself has none).
        double threatRatio(double own, double other);

        /// The kinds of action that are attacks (the table at 0x66DDE0: the kinds 0 to 0x15 that the jump table marks).
        bool isAttackKind(std::uint32_t kind);
    }
}

#endif

#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PACKAGECOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PACKAGECOMMANDS_H

#include "interpreter.hpp"

namespace FalloutScript
{
    /// What UseWeapon tells a character to do: shoot `mAttacks` times at the target from the place, with the weapon.
    /// The flags are the ones of the "Use Weapon" package that the games build for it.
    struct UseWeaponOrder
    {
        FormId mWeapon = 0;
        /// The reference the character shoots from (the character itself, or a marker); 0 for where it stands
        FormId mLocation = 0;
        FormId mTarget = 0;
        /// How many attacks it makes before the package is over; 0 is for one that has no end
        int mAttacks = 0;
        bool mCrouchToReload = true;
        bool mHoldFire = true;
        /// Every attack hits
        bool mAlwaysHit = false;
        /// An attack that hits does no damage
        bool mNoDamage = false;
        /// A place to aim at instead of the target (0 for none)
        FormId mTargetLocation = 0;
    };

    /// What the commands about the AI packages of characters ask of the game world. A character is told by the form id
    /// of its reference; the host knows which of them are in loaded cells and keeps what a script tells those that are
    /// not.
    class PackageWorld
    {
    public:
        virtual ~PackageWorld() = default;

        /// The character follows the package before all its own, whatever the time or the conditions say, until a
        /// script removes it
        virtual void addScriptPackage(FormId actor, FormId package) = 0;
        /// Removes the package that a script gave the character last
        virtual void removeScriptPackage(FormId actor) = 0;
        /// The character looks at its packages again, now and not at the next time it does
        virtual void evaluatePackage(FormId actor) = 0;
        /// The character's AI starts over: it drops what it was doing (it gets up from the furniture it uses) and looks
        /// at its packages again
        virtual void resetAI(FormId actor) { evaluatePackage(actor); }

        /// The character drops what it does and attacks as the order says (UseWeapon); it goes back to its packages
        /// when the attacks are made. Not for a character the host does not know or that is not in a loaded cell.
        virtual void useWeapon(FormId /*actor*/, const UseWeaponOrder& /*order*/) {}

        /// The package the character follows now (0 for none, and for a character the host does not know or can not
        /// tell about)
        virtual FormId currentPackage(FormId /*actor*/) { return 0; }
        /// The type of the package the character follows now (the number the record gives it), -1 for none
        virtual int currentPackageType(FormId /*actor*/) { return -1; }
        /// Whether the package the character follows now is one that offers the character's services (to trade)
        virtual bool currentPackageOffersServices(FormId /*actor*/) { return false; }
    };

    /// What GetCurrentAIPackage answers for a type of package that the record gives, -1 for one it has no number for
    int currentPackageNumber(int type);

    /// Gives the commands about packages to the interpreter: AddScriptPackage, RemoveScriptPackage,
    /// EvaluatePackage, GetIsCurrentPackage, GetCurrentAIPackage, GetOffersServicesNow, ResetAI and UseWeapon. Those
    /// that the command table does not have are left out.
    void addPackageCommands(Interpreter& interpreter, PackageWorld& world);
}

#endif

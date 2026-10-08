#ifndef OPENFALLOUT_MWMECHANICS_FALLOUTACTORS_H
#define OPENFALLOUT_MWMECHANICS_FALLOUTACTORS_H

#include <cstddef>
#include <map>
#include <memory>

namespace OFWorld
{
    class CellStore;
    class LiveCellRefBase;
    class Ptr;
}

namespace OFMechanics
{
    /// What the characters and creatures of Fallout 3 and New Vegas do while they are in the world: each follows the AI
    /// package of its list that its schedule says is on at the time of the game (ESM4::choosePackage) and goes where
    /// the package sends it. The game has no mechanics for them yet, so this moves them itself: it asks the navigator
    /// for a path, follows it at the speed of the walking animation (which the animation turns into walking, as it
    /// does for anything that is moved), and keeps their feet on the ground.
    ///
    /// What it does for a package is in ESM4::packageBehaviour: a character goes to the place (Travel, Guard, Eat,
    /// Sleep: it stands near the reference the package names) or goes about around it, stopping now and then (Sandbox,
    /// Wander, Patrol). Packages with conditions, the other kinds of package and places in a cell that is not loaded
    /// are left for the next package in the list.
    class FalloutActors
    {
    public:
        FalloutActors();
        ~FalloutActors();

        /// Whether the reference is a character or creature of Fallout, which this takes care of
        static bool handles(const OFWorld::Ptr& ptr);

        /// Starts to follow the packages of an actor (one that has none is not looked at again)
        void add(const OFWorld::Ptr& ptr);
        void remove(const OFWorld::Ptr& ptr);
        /// An actor that moved to another cell is another reference
        void updatePtr(const OFWorld::Ptr& old, const OFWorld::Ptr& ptr);
        void drop(const OFWorld::CellStore* cell);

        void update(float duration);

        std::size_t size() const { return mMinds.size(); }

        /// What the game keeps of one actor
        struct Mind;

    private:
        std::map<const OFWorld::LiveCellRefBase*, std::unique_ptr<Mind>> mMinds;
    };
}

#endif

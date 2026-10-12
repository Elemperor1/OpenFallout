#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_TRIGGERVOLUMES_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_TRIGGERVOLUMES_H

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <vector>

#include "script.hpp"

namespace FalloutScript
{
    struct Point
    {
        float x = 0;
        float y = 0;
        float z = 0;
    };

    enum class VolumeShape : std::uint8_t
    {
        Box,
        Sphere,
        Plane,
    };

    /// The volume of a trigger (the XPRM of its reference) and where it stands. The extents are those of the record:
    /// the half sizes of a box, the radius of a sphere (in x), the sides of a plane. The reference stands at the
    /// centre, and the axes are the directions of its own x, y and z in the world (unit vectors).
    struct Volume
    {
        VolumeShape mShape = VolumeShape::Box;
        Point mExtents;
        Point mCenter;
        Point mAxisX{ 1, 0, 0 };
        Point mAxisY{ 0, 1, 0 };
        Point mAxisZ{ 0, 0, 1 };
    };

    /// An actor that can set off a trigger: a capsule that stands upright on the point of its feet
    struct TriggerBody
    {
        FormId mReference = 0;
        Point mFeet;
        float mHeight = 0;
        float mRadius = 0;
    };

    /// Whether the capsule of the body touches the volume
    bool volumeTouches(const Volume& volume, const TriggerBody& body);

    /// The trigger volumes that are loaded, and who is inside each, which decides when the OnTrigger, OnTriggerEnter
    /// and OnTriggerLeave blocks of the script of its reference run. This follows what the executable of New Vegas does
    /// with the physics volumes of its triggers: the actors that touch a volume are asked about on every pass; one that
    /// was not inside is new and queues an Enter, one that was inside and still is runs OnTrigger, and one that was
    /// inside and no longer is queues a Leave. A trigger runs at most one of the queued events on a pass. There is no
    /// delay before an actor that left can set a trigger off again.
    class TriggerVolumes
    {
    public:
        /// Runs a block of the script of the trigger: its reference, the type of the block and the actor
        using Raise = std::function<void(FormId trigger, std::uint16_t blockType, FormId actor)>;

        /// Puts a volume in (or moves the one the trigger has already, which keeps who is inside)
        void set(FormId trigger, const Volume& volume);
        /// Takes a volume out with whoever was inside it, without any block running
        void remove(FormId trigger);
        void clear();

        bool has(FormId trigger) const { return mTriggers.count(trigger) != 0; }
        std::size_t size() const { return mTriggers.size(); }
        /// The actors that are inside the volume of the trigger now, as the last pass left them
        std::set<FormId> occupants(FormId trigger) const;

        /// One pass. A trigger that `active` refuses (one that is disabled) is left as it is, with whoever is inside.
        void update(
            const std::vector<TriggerBody>& bodies, const std::function<bool(FormId)>& active, const Raise& raise);

    private:
        struct Event
        {
            std::uint16_t mBlockType;
            FormId mActor;
        };

        struct Trigger
        {
            Volume mVolume;
            std::set<FormId> mOccupants;
            std::deque<Event> mQueue;
        };

        std::map<FormId, Trigger> mTriggers;
    };
}

#endif

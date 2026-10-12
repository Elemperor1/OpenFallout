#include "triggervolumes.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace FalloutScript
{
    namespace
    {
        float dot(const Point& a, const Point& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        // How far a ball of the radius around the point reaches into the volume: true when it touches it
        bool ballTouches(const Volume& volume, const Point& point, float radius)
        {
            const Point from{ point.x - volume.mCenter.x, point.y - volume.mCenter.y, point.z - volume.mCenter.z };
            const Point local{ dot(from, volume.mAxisX), dot(from, volume.mAxisY), dot(from, volume.mAxisZ) };
            if (volume.mShape == VolumeShape::Sphere)
            {
                const float reach = volume.mExtents.x + radius;
                return dot(local, local) <= reach * reach;
            }
            // A plane is a box as thin as the record says (at least a little, so that nobody walks through it between
            // two passes)
            constexpr float thinnest = 0.5f;
            const Point half{ std::abs(volume.mExtents.x), std::max(std::abs(volume.mExtents.y), thinnest),
                std::abs(volume.mExtents.z) };
            const float dx = std::max(std::abs(local.x) - half.x, 0.f);
            const float dy = std::max(std::abs(local.y) - half.y, 0.f);
            const float dz = std::max(std::abs(local.z) - half.z, 0.f);
            return dx * dx + dy * dy + dz * dz <= radius * radius;
        }
    }

    bool volumeTouches(const Volume& volume, const TriggerBody& body)
    {
        // The axis of the capsule runs from the centre of the ball at the feet to the centre of the ball at the head;
        // balls along it, no further apart than their radius, stand in for the whole
        const float radius = std::max(body.mRadius, 0.f);
        const float length = std::max(body.mHeight - 2 * radius, 0.f);
        const int steps = radius > 0 ? std::clamp(static_cast<int>(std::ceil(length / radius)), 1, 16) : 1;
        for (int i = 0; i <= steps; ++i)
        {
            const float up = radius + length * static_cast<float>(i) / static_cast<float>(steps);
            if (ballTouches(volume, Point{ body.mFeet.x, body.mFeet.y, body.mFeet.z + up }, radius))
                return true;
            if (length == 0)
                break;
        }
        return false;
    }

    void TriggerVolumes::set(FormId trigger, const Volume& volume)
    {
        mTriggers[trigger].mVolume = volume;
    }

    void TriggerVolumes::remove(FormId trigger)
    {
        mTriggers.erase(trigger);
    }

    void TriggerVolumes::clear()
    {
        mTriggers.clear();
    }

    std::set<FormId> TriggerVolumes::occupants(FormId trigger) const
    {
        const auto found = mTriggers.find(trigger);
        return found == mTriggers.end() ? std::set<FormId>() : found->second.mOccupants;
    }

    void TriggerVolumes::update(
        const std::vector<TriggerBody>& bodies, const std::function<bool(FormId)>& active, const Raise& raise)
    {
        // What runs can load and unload cells, and so change the triggers: the pass works on the references it began
        // with
        std::vector<FormId> triggers;
        triggers.reserve(mTriggers.size());
        for (const auto& [reference, trigger] : mTriggers)
            triggers.push_back(reference);
        for (const FormId reference : triggers)
        {
            auto found = mTriggers.find(reference);
            if (found == mTriggers.end() || (active && !active(reference)))
                continue;
            Trigger& trigger = found->second;

            std::set<FormId> inside;
            for (const TriggerBody& body : bodies)
                if (body.mReference != 0 && volumeTouches(trigger.mVolume, body))
                    inside.insert(body.mReference);

            // Whoever was inside and still is runs OnTrigger; whoever is new waits for an Enter, and whoever is gone
            // for a Leave. The events are raised after the pass has been worked out, as the blocks they run can change
            // anything.
            std::vector<FormId> staying;
            for (const FormId actor : inside)
            {
                if (trigger.mOccupants.count(actor) != 0)
                    staying.push_back(actor);
                else
                {
                    trigger.mOccupants.insert(actor);
                    trigger.mQueue.push_back(Event{ BlockType::OnTriggerEnter, actor });
                }
            }
            for (auto it = trigger.mOccupants.begin(); it != trigger.mOccupants.end();)
            {
                if (inside.count(*it) == 0)
                {
                    trigger.mQueue.push_back(Event{ BlockType::OnTriggerLeave, *it });
                    it = trigger.mOccupants.erase(it);
                }
                else
                    ++it;
            }
            std::optional<Event> next;
            if (!trigger.mQueue.empty())
            {
                next = trigger.mQueue.front();
                trigger.mQueue.pop_front();
            }
            for (const FormId actor : staying)
                raise(reference, BlockType::OnTrigger, actor);
            if (next)
                raise(reference, next->mBlockType, next->mActor);
        }
    }
}

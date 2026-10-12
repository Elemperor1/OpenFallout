#include <components/falloutscript/triggervolumes.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <tuple>
#include <vector>

namespace
{
    using namespace testing;
    using FalloutScript::FormId;
    using FalloutScript::Point;
    using FalloutScript::TriggerBody;
    using FalloutScript::TriggerVolumes;
    using FalloutScript::Volume;
    using FalloutScript::VolumeShape;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId player = 0x14;
    constexpr FormId trigger = 0x01000100;

    // A box that stands on the floor at z = 100: the reference is at the middle of it
    Volume box(float x, float y, float halfX, float halfY, float halfZ)
    {
        Volume volume;
        volume.mShape = VolumeShape::Box;
        volume.mExtents = { halfX, halfY, halfZ };
        volume.mCenter = { x, y, 100.f + halfZ };
        return volume;
    }

    TriggerBody body(FormId reference, float x, float y, float z = 100.f)
    {
        TriggerBody result;
        result.mReference = reference;
        result.mFeet = { x, y, z };
        result.mHeight = 128.f;
        result.mRadius = 20.f;
        return result;
    }

    TEST(FalloutScriptTriggerVolumesTest, a_box_is_touched_by_a_body_inside_it)
    {
        EXPECT_TRUE(FalloutScript::volumeTouches(box(0, 0, 100, 50, 60), body(player, 10, -20)));
    }

    TEST(FalloutScriptTriggerVolumesTest, a_box_is_touched_by_the_edge_of_a_body_and_not_by_a_body_further_off)
    {
        const Volume volume = box(0, 0, 100, 50, 60);

        // The side of the box is at x = 100, the capsule is 20 wide around its axis
        EXPECT_TRUE(FalloutScript::volumeTouches(volume, body(player, 119, 0)));
        EXPECT_FALSE(FalloutScript::volumeTouches(volume, body(player, 121, 0)));
    }

    TEST(FalloutScriptTriggerVolumesTest, a_body_above_or_below_a_box_does_not_touch_it)
    {
        // The box reaches from z = 100 to 220: a body that stands on a floor 300 above does not touch it, and one
        // whose head ends below it does not either
        const Volume volume = box(0, 0, 100, 50, 60);

        EXPECT_FALSE(FalloutScript::volumeTouches(volume, body(player, 0, 0, 300)));
        EXPECT_FALSE(FalloutScript::volumeTouches(volume, body(player, 0, 0, -50)));
        EXPECT_TRUE(FalloutScript::volumeTouches(volume, body(player, 0, 0, 200)));
    }

    TEST(FalloutScriptTriggerVolumesTest, a_box_that_is_turned_is_measured_in_its_own_frame)
    {
        // Long in x, turned a quarter around z so that its long side runs along y
        Volume volume = box(0, 0, 200, 10, 60);
        volume.mAxisX = { 0, 1, 0 };
        volume.mAxisY = { -1, 0, 0 };

        EXPECT_TRUE(FalloutScript::volumeTouches(volume, body(player, 0, 150)));
        EXPECT_FALSE(FalloutScript::volumeTouches(volume, body(player, 150, 0)));
    }

    TEST(FalloutScriptTriggerVolumesTest, a_sphere_is_measured_from_its_middle)
    {
        Volume volume;
        volume.mShape = VolumeShape::Sphere;
        volume.mExtents = { 70.f, 70.f, 70.f };
        volume.mCenter = { 0, 0, 110.f };

        EXPECT_TRUE(FalloutScript::volumeTouches(volume, body(player, 85, 0)));
        EXPECT_FALSE(FalloutScript::volumeTouches(volume, body(player, 95, 0)));
    }

    TEST(FalloutScriptTriggerVolumesTest, a_plane_is_a_thin_box)
    {
        Volume volume;
        volume.mShape = VolumeShape::Plane;
        volume.mExtents = { 100.f, 0.f, 100.f };
        volume.mCenter = { 0, 0, 150.f };

        EXPECT_TRUE(FalloutScript::volumeTouches(volume, body(player, 50, 0)));
        EXPECT_FALSE(FalloutScript::volumeTouches(volume, body(player, 50, 40)));
    }

    // Records the blocks that run
    struct Raised
    {
        std::vector<std::tuple<FormId, std::uint16_t, FormId>> mBlocks;
        TriggerVolumes::Raise raise()
        {
            return [this](FormId reference, std::uint16_t type, FormId actor) {
                mBlocks.emplace_back(reference, type, actor);
            };
        }
    };

    using Block = std::tuple<FormId, std::uint16_t, FormId>;

    TEST(FalloutScriptTriggerVolumesTest, an_actor_that_comes_in_stays_and_leaves_runs_enter_ontrigger_and_leave)
    {
        TriggerVolumes volumes;
        volumes.set(trigger, box(0, 0, 100, 100, 100));
        Raised run;

        volumes.update({ body(player, 0, 0) }, {}, run.raise());
        volumes.update({ body(player, 0, 0) }, {}, run.raise());
        volumes.update({ body(player, 0, 0) }, {}, run.raise());
        volumes.update({ body(player, 500, 0) }, {}, run.raise());
        volumes.update({ body(player, 500, 0) }, {}, run.raise());

        // The pass that finds the actor runs Enter only, the next ones run OnTrigger, the pass that loses it runs Leave
        EXPECT_THAT(run.mBlocks,
            ElementsAre(Block{ trigger, BlockType::OnTriggerEnter, player },
                Block{ trigger, BlockType::OnTrigger, player }, Block{ trigger, BlockType::OnTrigger, player },
                Block{ trigger, BlockType::OnTriggerLeave, player }));
        EXPECT_THAT(volumes.occupants(trigger), IsEmpty());
    }

    TEST(FalloutScriptTriggerVolumesTest, a_trigger_runs_one_queued_event_on_each_pass)
    {
        TriggerVolumes volumes;
        volumes.set(trigger, box(0, 0, 100, 100, 100));
        Raised run;
        const FormId first = 0x01000201;
        const FormId second = 0x01000202;

        volumes.update({ body(first, 0, 0), body(second, 10, 0) }, {}, run.raise());
        EXPECT_THAT(run.mBlocks, ElementsAre(Block{ trigger, BlockType::OnTriggerEnter, first }));
        EXPECT_THAT(volumes.occupants(trigger), UnorderedElementsAre(first, second));

        run.mBlocks.clear();
        volumes.update({ body(first, 0, 0), body(second, 10, 0) }, {}, run.raise());
        // The first is inside and known, so OnTrigger runs for it; the second one's Enter was waiting
        EXPECT_THAT(run.mBlocks,
            ElementsAre(Block{ trigger, BlockType::OnTrigger, first }, Block{ trigger, BlockType::OnTrigger, second },
                Block{ trigger, BlockType::OnTriggerEnter, second }));
    }

    TEST(FalloutScriptTriggerVolumesTest, a_trigger_that_is_not_active_keeps_who_is_inside_and_runs_nothing)
    {
        TriggerVolumes volumes;
        volumes.set(trigger, box(0, 0, 100, 100, 100));
        Raised run;
        volumes.update({ body(player, 0, 0) }, {}, run.raise());
        run.mBlocks.clear();

        volumes.update({ body(player, 900, 0) }, [](FormId) { return false; }, run.raise());
        EXPECT_THAT(run.mBlocks, IsEmpty());
        EXPECT_THAT(volumes.occupants(trigger), ElementsAre(player));

        // Enabled again, the actor is found to be gone
        volumes.update({ body(player, 900, 0) }, [](FormId) { return true; }, run.raise());
        EXPECT_THAT(run.mBlocks, ElementsAre(Block{ trigger, BlockType::OnTriggerLeave, player }));
    }

    TEST(FalloutScriptTriggerVolumesTest, a_volume_that_is_taken_out_forgets_who_was_inside_without_running_anything)
    {
        TriggerVolumes volumes;
        volumes.set(trigger, box(0, 0, 100, 100, 100));
        Raised run;
        volumes.update({ body(player, 0, 0) }, {}, run.raise());
        run.mBlocks.clear();

        volumes.remove(trigger);
        volumes.update({ body(player, 0, 0) }, {}, run.raise());
        EXPECT_THAT(run.mBlocks, IsEmpty());
        EXPECT_FALSE(volumes.has(trigger));

        // Put back, the actor that is still there is new
        volumes.set(trigger, box(0, 0, 100, 100, 100));
        volumes.update({ body(player, 0, 0) }, {}, run.raise());
        EXPECT_THAT(run.mBlocks, ElementsAre(Block{ trigger, BlockType::OnTriggerEnter, player }));
    }

    TEST(FalloutScriptTriggerVolumesTest, a_volume_that_is_moved_keeps_who_is_inside)
    {
        TriggerVolumes volumes;
        volumes.set(trigger, box(0, 0, 100, 100, 100));
        Raised run;
        volumes.update({ body(player, 0, 0) }, {}, run.raise());
        run.mBlocks.clear();

        volumes.set(trigger, box(30, 0, 100, 100, 100));
        volumes.update({ body(player, 0, 0) }, {}, run.raise());
        EXPECT_THAT(run.mBlocks, ElementsAre(Block{ trigger, BlockType::OnTrigger, player }));
    }

    TEST(FalloutScriptTriggerVolumesTest, a_body_without_a_reference_sets_nothing_off)
    {
        TriggerVolumes volumes;
        volumes.set(trigger, box(0, 0, 100, 100, 100));
        Raised run;

        volumes.update({ body(0, 0, 0) }, {}, run.raise());
        EXPECT_THAT(run.mBlocks, IsEmpty());
    }
}

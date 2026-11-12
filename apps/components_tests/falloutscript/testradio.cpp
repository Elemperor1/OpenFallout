#include <components/falloutscript/radio.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <vector>

namespace
{
    using namespace FalloutScript::Radio;

    Where outside(float distance)
    {
        Where where;
        where.mExterior = true;
        where.mSameWorldspace = true;
        where.mDistance = distance;
        return where;
    }

    TEST(FalloutScriptRadioTest, aStationWithARadiusIsHeardInsideItAndGetsStaticTowardTheEdge)
    {
        const Settings settings;
        const Signal near = signalOf(Reach::Radius, 6000.f, 100.f, outside(0.f), settings);
        EXPECT_TRUE(near.mInRange);
        EXPECT_FLOAT_EQ(near.mStrength, 1.f);
        EXPECT_FLOAT_EQ(near.mStatic, 0.f);
        const Signal half = signalOf(Reach::Radius, 6000.f, 100.f, outside(3000.f), settings);
        EXPECT_TRUE(half.mInRange);
        EXPECT_FLOAT_EQ(half.mStrength, 0.5f);
        EXPECT_FLOAT_EQ(half.mStatic, 0.5f);
        const Signal edge = signalOf(Reach::Radius, 6000.f, 40.f, outside(6000.f), settings);
        EXPECT_TRUE(edge.mInRange);
        EXPECT_FLOAT_EQ(edge.mStatic, 0.4f);
        EXPECT_FALSE(signalOf(Reach::Radius, 6000.f, 100.f, outside(6000.5f), settings).mInRange);
    }

    TEST(FalloutScriptRadioTest, aStationThatNamesNoStaticTakesTheShareOfTheSettings)
    {
        const Settings settings;
        EXPECT_FLOAT_EQ(signalOf(Reach::Radius, 100.f, 0.f, outside(100.f), settings).mStatic, 0.1f);
    }

    TEST(FalloutScriptRadioTest, aRadiusIsHeardOutOfDoorsAndOnlyWhereItCanBeMeasured)
    {
        const Settings settings;
        Where inside = outside(10.f);
        inside.mExterior = false;
        EXPECT_FALSE(signalOf(Reach::Radius, 6000.f, 100.f, inside, settings).mInRange);
        EXPECT_FALSE(signalOf(Reach::Radius, 6000.f, 100.f, outside(-1.f), settings).mInRange);
        EXPECT_FALSE(signalOf(Reach::Radius, 0.f, 100.f, outside(0.f), settings).mInRange);
    }

    TEST(FalloutScriptRadioTest, theOtherReachesHaveNoRadiusAndNoStatic)
    {
        const Settings settings;
        const Signal everywhere = signalOf(Reach::Everywhere, 0.f, 50.f, Where(), settings);
        EXPECT_TRUE(everywhere.mInRange);
        EXPECT_FLOAT_EQ(everywhere.mStrength, 1.f);
        EXPECT_FLOAT_EQ(everywhere.mStatic, 0.f);
        Where cell;
        cell.mExterior = false;
        EXPECT_FALSE(signalOf(Reach::CurrentCell, 0.f, 0.f, cell, settings).mInRange);
        EXPECT_FALSE(signalOf(Reach::LinkedInteriors, 0.f, 0.f, cell, settings).mInRange);
        cell.mSameCell = true;
        EXPECT_TRUE(signalOf(Reach::CurrentCell, 0.f, 0.f, cell, settings).mInRange);
        EXPECT_TRUE(signalOf(Reach::LinkedInteriors, 0.f, 0.f, cell, settings).mInRange);
        Where world = outside(-1.f);
        world.mSameWorldspace = false;
        EXPECT_FALSE(signalOf(Reach::Worldspace, 0.f, 0.f, world, settings).mInRange);
        world.mSameWorldspace = true;
        EXPECT_TRUE(signalOf(Reach::Worldspace, 0.f, 0.f, world, settings).mInRange);
    }

    TEST(FalloutScriptRadioTest, aWorldspaceReachIsHeardIndoorsOnlyInTheInteriorsLinkedToItsWorldspace)
    {
        const Settings settings;
        Where inside;
        inside.mExterior = false;
        EXPECT_FALSE(signalOf(Reach::Worldspace, 0.f, 0.f, inside, settings).mInRange);
        inside.mSameWorldspace = true;
        EXPECT_TRUE(signalOf(Reach::Worldspace, 0.f, 0.f, inside, settings).mInRange);
        // (a station placed in an interior is heard where it is, whatever the worldspace)
        Where here;
        here.mExterior = false;
        here.mSameCell = true;
        EXPECT_TRUE(signalOf(Reach::Worldspace, 0.f, 0.f, here, settings).mInRange);
    }

    TEST(FalloutScriptRadioTest, aLinkedInteriorsReachIsHeardInTheCellAndTheInteriorsLinkedToIt)
    {
        const Settings settings;
        Where linked;
        linked.mExterior = false;
        linked.mLinkedInterior = true;
        EXPECT_TRUE(signalOf(Reach::LinkedInteriors, 0.f, 0.f, linked, settings).mInRange);
        // (the current-cell reach does not go further than the cell)
        EXPECT_FALSE(signalOf(Reach::CurrentCell, 0.f, 0.f, linked, settings).mInRange);
        // (and out of doors it is heard in no cell but its own)
        Where outsideLinked = outside(-1.f);
        outsideLinked.mLinkedInterior = true;
        EXPECT_FALSE(signalOf(Reach::LinkedInteriors, 0.f, 0.f, outsideLinked, settings).mInRange);
    }

    TEST(FalloutScriptRadioTest, theDoorsLinkTheInteriorsThatCanBeWalkedToWithoutGoingOutOfDoors)
    {
        Links<std::uint32_t> links;
        // A vault: 1 - 2 - 3 (3 has a door out to worldspace 100), and a house 7 - 8 that opens on worldspace 200
        links.addDoor(1, 2);
        links.addDoor(3, 2);
        links.addExit(3, 100);
        links.addDoor(7, 8);
        links.addExit(8, 200);
        const Links<std::uint32_t>::Group vault = links.groupOf(1);
        EXPECT_EQ(vault.mInteriors, (std::set<std::uint32_t>{ 1, 2, 3 }));
        EXPECT_EQ(vault.mWorldspaces, (std::set<std::uint32_t>{ 100 }));
        EXPECT_EQ(links.groupOf(3).mInteriors, vault.mInteriors);
        const Links<std::uint32_t>::Group house = links.groupOf(8);
        EXPECT_EQ(house.mInteriors, (std::set<std::uint32_t>{ 7, 8 }));
        EXPECT_EQ(house.mWorldspaces, (std::set<std::uint32_t>{ 200 }));
        // An interior with no doors is linked to itself only
        EXPECT_EQ(links.groupOf(9).mInteriors, (std::set<std::uint32_t>{ 9 }));
        EXPECT_TRUE(links.groupOf(9).mWorldspaces.empty());
        EXPECT_FALSE(links.empty());
        EXPECT_TRUE(Links<std::uint32_t>().empty());
    }

    TEST(FalloutScriptRadioTest, theVoiceIsTheVolumeOfTheRadioLessStaticAndQuieterInADialogue)
    {
        const Settings settings;
        Signal signal;
        signal.mInRange = true;
        signal.mStatic = 0.2f;
        EXPECT_FLOAT_EQ(gainOf(signal, false, settings), 0.4f);
        EXPECT_FLOAT_EQ(gainOf(signal, true, settings), 0.2f);
        EXPECT_FLOAT_EQ(gainOf(Signal(), false, settings), 0.f);
    }

    std::vector<Sighting> seen(std::initializer_list<std::uint32_t> stations)
    {
        std::vector<Sighting> result;
        for (const std::uint32_t station : stations)
        {
            Sighting sighting;
            sighting.mStation = station;
            sighting.mSignal.mInRange = true;
            sighting.mSignal.mStrength = 1.f;
            result.push_back(sighting);
        }
        return result;
    }

    TEST(FalloutScriptRadioTest, aStationIsDiscoveredOnceAndFoundAgainAfterTheSignalWasLost)
    {
        Receiver receiver;
        std::vector<Event> events = receiver.update(0.25f, seen({ 7 }));
        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].mKind, Event::Kind::Discovered);
        EXPECT_TRUE(receiver.inReach(7));
        EXPECT_TRUE(receiver.discovered(7));
        EXPECT_TRUE(receiver.update(0.25f, seen({ 7 })).empty());
        // out of reach for less than the timeout: still there
        EXPECT_TRUE(receiver.update(0.5f, {}).empty());
        EXPECT_TRUE(receiver.inReach(7));
        events = receiver.update(0.75f, {});
        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].mKind, Event::Kind::Lost);
        EXPECT_FALSE(receiver.inReach(7));
        EXPECT_TRUE(receiver.discovered(7));
        events = receiver.update(0.25f, seen({ 7 }));
        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].mKind, Event::Kind::Found);
    }

    TEST(FalloutScriptRadioTest, aBlinkOfLostSignalDoesNotMakeItDiscoveredOrFoundAgain)
    {
        Receiver receiver;
        receiver.update(0.25f, seen({ 7 }));
        EXPECT_TRUE(receiver.update(0.5f, {}).empty());
        EXPECT_TRUE(receiver.update(0.25f, seen({ 7 })).empty());
        EXPECT_TRUE(receiver.update(0.5f, {}).empty());
        EXPECT_TRUE(receiver.inReach(7));
    }

    TEST(FalloutScriptRadioTest, onlyAStationInReachCanBeTunedAndLosingItUntunesIt)
    {
        Receiver receiver;
        EXPECT_FALSE(receiver.tune(7));
        receiver.update(0.25f, seen({ 7, 9 }));
        EXPECT_TRUE(receiver.tune(7));
        EXPECT_EQ(receiver.tuned(), 7u);
        EXPECT_TRUE(receiver.tune(9));
        EXPECT_EQ(receiver.tuned(), 9u);
        EXPECT_EQ(receiver.stations(), std::vector<std::uint32_t>({ 7u, 9u }));
        receiver.update(2.f, seen({ 7 }));
        EXPECT_EQ(receiver.tuned(), 0u);
        EXPECT_TRUE(receiver.tune(7));
        receiver.untune();
        EXPECT_EQ(receiver.tuned(), 0u);
        EXPECT_FALSE(receiver.tune(0));
    }

    TEST(FalloutScriptRadioTest, aStationOutOfRangeInASightingIsNotInReach)
    {
        Receiver receiver;
        std::vector<Sighting> out(1);
        out[0].mStation = 7;
        EXPECT_TRUE(receiver.update(0.25f, out).empty());
        EXPECT_FALSE(receiver.inReach(7));
        receiver.update(0.25f, seen({ 7 }));
        EXPECT_FLOAT_EQ(receiver.signal(7).mStrength, 1.f);
        EXPECT_FALSE(receiver.signal(8).mInRange);
        receiver.reset();
        EXPECT_FALSE(receiver.discovered(7));
        EXPECT_FALSE(receiver.inReach(7));
    }
}

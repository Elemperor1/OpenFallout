#include <gtest/gtest.h>

#include <map>

#include <components/esm4/loadwrld.hpp>

#include "apps/openfallout/mwworld/cell.hpp"

namespace OFWorld
{
    namespace
    {
        ESM::FormId formId(std::uint32_t value)
        {
            return ESM::FormId::fromUint32(value);
        }

        ESM4::World makeWorld(std::uint32_t id, std::uint32_t climate, std::uint32_t parent, std::uint16_t useFlags)
        {
            ESM4::World world;
            world.mId = formId(id);
            world.mClimate = formId(climate);
            world.mParent = formId(parent);
            world.mParentUseFlags = useFlags;
            return world;
        }

        struct Worlds
        {
            std::map<ESM::FormId, ESM4::World> mWorlds;

            void add(const ESM4::World& world) { mWorlds[world.mId] = world; }

            ESM::FormId climateOf(std::uint32_t id) const
            {
                return resolveClimate(mWorlds.at(formId(id)), [this](ESM::FormId key) -> const ESM4::World* {
                    auto it = mWorlds.find(key);
                    return it != mWorlds.end() ? &it->second : nullptr;
                });
            }
        };

        TEST(OFWorldResolveClimateTest, aWorldspaceWithoutParentHasItsOwnClimate)
        {
            Worlds worlds;
            worlds.add(makeWorld(0x10, 0x100, 0, ESM4::World::UseFlag_Climate));
            EXPECT_EQ(worlds.climateOf(0x10), formId(0x100));
        }

        TEST(OFWorldResolveClimateTest, aWorldspaceThatUsesTheClimateOfItsParentHasTheClimateOfItsParent)
        {
            Worlds worlds;
            worlds.add(makeWorld(0x10, 0x100, 0, 0));
            worlds.add(makeWorld(0x11, 0, 0x10, ESM4::World::UseFlag_Climate));
            EXPECT_EQ(worlds.climateOf(0x11), formId(0x100));
        }

        TEST(OFWorldResolveClimateTest, theClimateOfTheParentWinsOverTheOwnClimateWhenTheFlagIsSet)
        {
            Worlds worlds;
            worlds.add(makeWorld(0x10, 0x100, 0, 0));
            worlds.add(makeWorld(0x11, 0x101, 0x10, ESM4::World::UseFlag_Climate));
            EXPECT_EQ(worlds.climateOf(0x11), formId(0x100));
        }

        TEST(OFWorldResolveClimateTest, aWorldspaceThatDoesNotUseTheClimateOfItsParentKeepsItsOwn)
        {
            Worlds worlds;
            worlds.add(makeWorld(0x10, 0x100, 0, 0));
            worlds.add(makeWorld(0x11, 0x101, 0x10, ESM4::World::UseFlag_Land | ESM4::World::UseFlag_Water));
            EXPECT_EQ(worlds.climateOf(0x11), formId(0x101));
        }

        TEST(OFWorldResolveClimateTest, theChainOfParentsIsFollowed)
        {
            Worlds worlds;
            worlds.add(makeWorld(0x10, 0x100, 0, 0));
            worlds.add(makeWorld(0x11, 0, 0x10, ESM4::World::UseFlag_Climate));
            worlds.add(makeWorld(0x12, 0, 0x11, ESM4::World::UseFlag_Climate));
            EXPECT_EQ(worlds.climateOf(0x12), formId(0x100));
        }

        TEST(OFWorldResolveClimateTest, aLongChainOfParentsIsFollowedToTheEnd)
        {
            Worlds worlds;
            worlds.add(makeWorld(0x10, 0x100, 0, 0));
            for (std::uint32_t id = 0x11; id < 0x11 + 40; ++id)
                worlds.add(makeWorld(id, 0, id - 1, ESM4::World::UseFlag_Climate));
            EXPECT_EQ(worlds.climateOf(0x11 + 39), formId(0x100));
        }

        TEST(OFWorldResolveClimateTest, aParentThatIsNotLoadedLeavesTheOwnClimate)
        {
            Worlds worlds;
            worlds.add(makeWorld(0x11, 0x101, 0x10, ESM4::World::UseFlag_Climate));
            EXPECT_EQ(worlds.climateOf(0x11), formId(0x101));
        }

        TEST(OFWorldResolveClimateTest, aParentWithoutClimateLeavesTheOwnClimate)
        {
            Worlds worlds;
            worlds.add(makeWorld(0x10, 0, 0, 0));
            worlds.add(makeWorld(0x11, 0x101, 0x10, ESM4::World::UseFlag_Climate));
            EXPECT_EQ(worlds.climateOf(0x11), formId(0x101));
        }

        TEST(OFWorldResolveClimateTest, aLoopOfParentsEnds)
        {
            Worlds worlds;
            worlds.add(makeWorld(0x10, 0x100, 0x11, ESM4::World::UseFlag_Climate));
            worlds.add(makeWorld(0x11, 0x101, 0x10, ESM4::World::UseFlag_Climate));
            const ESM::FormId climate = worlds.climateOf(0x10);
            EXPECT_TRUE(climate == formId(0x100) || climate == formId(0x101));
        }
    }
}

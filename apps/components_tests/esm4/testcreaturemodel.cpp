#include <components/esm4/creaturemodel.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <set>

namespace
{
    using namespace testing;

    ESM::FormId id(std::uint32_t value)
    {
        return ESM::FormId::fromUint32(value);
    }

    constexpr int playerLevel = 5;

    // The records of a few plugins, found by their form IDs
    class TestSource final : public ESM4::CreatureSource
    {
    public:
        std::map<ESM::FormId, ESM4::Creature> mCreatures;
        std::map<ESM::FormId, ESM4::LevelledCreature> mLists;

        const ESM4::Creature* findCreature(ESM::FormId formId) const override
        {
            const auto found = mCreatures.find(formId);
            return found == mCreatures.end() ? nullptr : &found->second;
        }
        const ESM4::LevelledCreature* findLevelledCreature(ESM::FormId formId) const override
        {
            const auto found = mLists.find(formId);
            return found == mLists.end() ? nullptr : &found->second;
        }

        // A creature of Fallout with a skeleton and the models and animations that hang on it
        ESM4::Creature& addCreature(std::uint32_t value, const char* skeleton, std::vector<std::string> parts = {},
            std::uint16_t templateFlags = 0, std::uint32_t templateId = 0, int level = 1)
        {
            ESM4::Creature creature{};
            creature.mId = id(value);
            creature.mIsFONV = true;
            creature.mModel = skeleton;
            creature.mNif = std::move(parts);
            creature.mBaseConfig.fo3.levelOrMult = static_cast<std::int16_t>(level);
            creature.mBaseConfig.fo3.templateFlags = templateFlags;
            if (templateId != 0)
                creature.mBaseTemplate = id(templateId);
            return mCreatures[id(value)] = creature;
        }

        // A list with the entries (level, item); the flags are those of LVLF
        void addList(std::uint32_t value, std::initializer_list<std::pair<int, std::uint32_t>> entries,
            std::uint8_t flags = 0, std::int8_t chanceNone = 0)
        {
            ESM4::LevelledCreature list{};
            list.mId = id(value);
            list.mChanceNone = chanceNone;
            list.mHasLvlCreaFlags = true;
            list.mLvlCreaFlags = flags;
            for (const auto& [level, item] : entries)
                list.mLvlObject.push_back({ static_cast<std::int16_t>(level), 0, item, 1, 0 });
            mLists[id(value)] = list;
        }
    };

    std::set<std::uint32_t> resolvedAcrossSeeds(const TestSource& source, std::uint32_t listId, int level)
    {
        std::set<std::uint32_t> results;
        for (std::uint32_t seed = 1; seed <= 64; ++seed)
            if (const ESM4::Creature* creature = ESM4::resolveCreature(source, id(listId), level, seed))
                results.insert(creature->mId.mIndex);
        return results;
    }

    constexpr std::uint16_t useModel = ESM4::Creature::Template_UseModel;

    TEST(ESM4CreatureModelTest, readsWhetherARecordTakesAThingFromItsTemplate)
    {
        ESM4::Creature creature{};
        creature.mIsFONV = true;
        creature.mBaseConfig.fo3.templateFlags = useModel | ESM4::Creature::Template_UseInventory;
        EXPECT_TRUE(creature.takesFromTemplate(ESM4::Creature::Template_UseModel));
        EXPECT_TRUE(creature.takesFromTemplate(ESM4::Creature::Template_UseInventory));
        EXPECT_FALSE(creature.takesFromTemplate(ESM4::Creature::Template_UseStats));

        // a record of an earlier game has no template, whatever bits its ACBS has there
        creature.mIsFONV = false;
        EXPECT_FALSE(creature.takesFromTemplate(ESM4::Creature::Template_UseModel));
    }

    TEST(ESM4CreatureModelTest, readsTheLevelOfACreature)
    {
        TestSource source;
        ESM4::Creature& fixed = source.addCreature(0x2001, "creatures\\a\\skeleton.nif", {}, 0, 0, 12);
        EXPECT_EQ(ESM4::creatureLevel(fixed, playerLevel), 12);

        ESM4::Creature& follower = source.addCreature(0x2002, "creatures\\a\\skeleton.nif");
        follower.mBaseConfig.fo3.flags = ESM4::Creature::FO3_PCLevelMult;
        follower.mBaseConfig.fo3.levelOrMult = 2000;
        EXPECT_EQ(ESM4::creatureLevel(follower, playerLevel), 10);
        follower.mBaseConfig.fo3.calcMaxlevel = 8;
        EXPECT_EQ(ESM4::creatureLevel(follower, playerLevel), 8);
        follower.mBaseConfig.fo3.levelOrMult = 100;
        follower.mBaseConfig.fo3.calcMinlevel = 2;
        EXPECT_EQ(ESM4::creatureLevel(follower, playerLevel), 2);

        // a level is at least 1, and a record of another game has the level of the player
        fixed.mBaseConfig.fo3.levelOrMult = -3;
        EXPECT_EQ(ESM4::creatureLevel(fixed, playerLevel), 1);
        fixed.mIsFONV = false;
        EXPECT_EQ(ESM4::creatureLevel(fixed, playerLevel), playerLevel);
    }

    TEST(ESM4CreatureModelTest, aNameWithoutAFolderIsInTheFolderOfTheSkeleton)
    {
        EXPECT_EQ(ESM4::creatureFile("creatures\\gecko\\skeleton.nif", "gecko.nif"), "creatures/gecko/gecko.nif");
        EXPECT_EQ(ESM4::creatureFile("Creatures/Gecko/Skeleton.nif", "GeckoBody.NIF"), "Creatures/Gecko/GeckoBody.NIF");
        EXPECT_EQ(ESM4::creatureFile("creatures\\gecko\\skeleton.nif", "creatures\\other\\part.nif"),
            "creatures/other/part.nif");
        EXPECT_EQ(ESM4::creatureFile("skeleton.nif", "part.nif"), "part.nif");
        EXPECT_EQ(ESM4::creatureFile({}, "creatures\\gecko\\skeleton.nif"), "creatures/gecko/skeleton.nif");
    }

    TEST(ESM4CreatureModelTest, aCreatureWithoutATemplateShowsItsOwnModel)
    {
        TestSource source;
        source.addCreature(0x2001, "Creatures\\Gecko\\Skeleton.nif", { "GeckoBody.nif", "GeckoEyes.nif" });
        source.mCreatures.at(id(0x2001)).mKf = { "Idle.kf" };

        const ESM4::CreatureModel model = ESM4::creatureModel(source, source.mCreatures.at(id(0x2001)), playerLevel, 1);

        EXPECT_EQ(model.mOwner, &source.mCreatures.at(id(0x2001)));
        EXPECT_EQ(model.mSkeleton, "Creatures/Gecko/Skeleton.nif");
        EXPECT_THAT(model.mParts, ElementsAre("Creatures/Gecko/GeckoBody.nif", "Creatures/Gecko/GeckoEyes.nif"));
        EXPECT_THAT(model.mAnimations, ElementsAre("Creatures/Gecko/Idle.kf"));
    }

    TEST(ESM4CreatureModelTest, aCreatureThatUsesTheModelOfItsTemplateShowsThatOne)
    {
        TestSource source;
        source.addCreature(0x2001, "creatures\\a\\skeleton.nif", { "a.nif" }, useModel, 0x2002);
        source.addCreature(0x2002, "creatures\\b\\skeleton.nif", { "b.nif" });

        const ESM4::CreatureModel model = ESM4::creatureModel(source, source.mCreatures.at(id(0x2001)), playerLevel, 1);

        EXPECT_EQ(model.mOwner, &source.mCreatures.at(id(0x2002)));
        EXPECT_EQ(model.mSkeleton, "creatures/b/skeleton.nif");
        EXPECT_THAT(model.mParts, ElementsAre("creatures/b/b.nif"));
    }

    TEST(ESM4CreatureModelTest, aCreatureThatDoesNotUseTheModelOfItsTemplateKeepsItsOwn)
    {
        TestSource source;
        source.addCreature(0x2001, "creatures\\a\\skeleton.nif", { "a.nif" }, 0, 0x2002);
        source.addCreature(0x2002, "creatures\\b\\skeleton.nif", { "b.nif" });

        const ESM4::CreatureModel model = ESM4::creatureModel(source, source.mCreatures.at(id(0x2001)), playerLevel, 1);

        EXPECT_EQ(model.mOwner, &source.mCreatures.at(id(0x2001)));
        EXPECT_THAT(model.mParts, ElementsAre("creatures/a/a.nif"));
    }

    TEST(ESM4CreatureModelTest, followsAChainOfTemplates)
    {
        TestSource source;
        source.addCreature(0x2001, "", {}, useModel, 0x2002);
        source.addCreature(0x2002, "", {}, useModel, 0x2003);
        source.addCreature(0x2003, "creatures\\c\\skeleton.nif", { "c.nif" });

        const ESM4::Creature& creature = source.mCreatures.at(id(0x2001));
        const auto chain = ESM4::creatureTemplateChain(source, creature, playerLevel, 1);
        ASSERT_EQ(chain.size(), 3u);
        EXPECT_EQ(chain[2], &source.mCreatures.at(id(0x2003)));
        EXPECT_EQ(ESM4::creatureModel(source, creature, playerLevel, 1).mSkeleton, "creatures/c/skeleton.nif");
    }

    TEST(ESM4CreatureModelTest, endsAChainOfTemplatesThatIsACycle)
    {
        TestSource source;
        source.addCreature(0x2001, "creatures\\a\\skeleton.nif", {}, useModel, 0x2002);
        source.addCreature(0x2002, "creatures\\b\\skeleton.nif", {}, useModel, 0x2001);

        const ESM4::Creature& creature = source.mCreatures.at(id(0x2001));
        EXPECT_EQ(ESM4::creatureTemplateChain(source, creature, playerLevel, 1).size(), 2u);

        // every record takes the model, so the first that has a skeleton shows it
        EXPECT_EQ(ESM4::creatureModel(source, creature, playerLevel, 1).mSkeleton, "creatures/a/skeleton.nif");
    }

    TEST(ESM4CreatureModelTest, aTemplateThatIsNotFoundLeavesTheModelOfTheCreature)
    {
        TestSource source;
        source.addCreature(0x2001, "creatures\\a\\skeleton.nif", { "a.nif" }, useModel, 0x9999);

        const ESM4::CreatureModel model = ESM4::creatureModel(source, source.mCreatures.at(id(0x2001)), playerLevel, 1);

        EXPECT_EQ(model.mOwner, &source.mCreatures.at(id(0x2001)));
        EXPECT_EQ(model.mSkeleton, "creatures/a/skeleton.nif");
    }

    TEST(ESM4CreatureModelTest, aCreatureWithoutAnyModelHasNone)
    {
        TestSource source;
        source.addCreature(0x2001, "");

        const ESM4::CreatureModel model = ESM4::creatureModel(source, source.mCreatures.at(id(0x2001)), playerLevel, 1);

        EXPECT_EQ(model.mOwner, nullptr);
        EXPECT_TRUE(model.mSkeleton.empty());
        EXPECT_TRUE(model.mParts.empty());
    }

    TEST(ESM4CreatureModelTest, aLevelledListOfCreaturesGivesTheCreatureOfTheHighestLevelAtOrBelowTheLevel)
    {
        TestSource source;
        source.addCreature(0x2001, "creatures\\a\\skeleton.nif");
        source.addCreature(0x2002, "creatures\\b\\skeleton.nif");
        source.addCreature(0x2003, "creatures\\c\\skeleton.nif");
        source.addList(0x3001, { { 1, 0x2001 }, { 4, 0x2002 }, { 9, 0x2003 } });

        EXPECT_THAT(resolvedAcrossSeeds(source, 0x3001, 1), ElementsAre(0x2001u));
        EXPECT_THAT(resolvedAcrossSeeds(source, 0x3001, 5), ElementsAre(0x2002u));
        EXPECT_THAT(resolvedAcrossSeeds(source, 0x3001, 30), ElementsAre(0x2003u));
        // nothing at or below the level
        EXPECT_THAT(resolvedAcrossSeeds(source, 0x3001, 0), IsEmpty());
    }

    TEST(ESM4CreatureModelTest, aListThatCalculatesFromAllLevelsChoosesAmongTheEntriesAtOrBelowTheLevel)
    {
        TestSource source;
        source.addCreature(0x2001, "creatures\\a\\skeleton.nif");
        source.addCreature(0x2002, "creatures\\b\\skeleton.nif");
        source.addCreature(0x2003, "creatures\\c\\skeleton.nif");
        source.addList(0x3001, { { 1, 0x2001 }, { 4, 0x2002 }, { 9, 0x2003 } }, 0x01);

        EXPECT_THAT(resolvedAcrossSeeds(source, 0x3001, 5), UnorderedElementsAre(0x2001u, 0x2002u));
    }

    TEST(ESM4CreatureModelTest, followsListsInListsAndEndsListsThatContainEachOther)
    {
        TestSource source;
        source.addCreature(0x2001, "creatures\\a\\skeleton.nif");
        source.addList(0x3002, { { 1, 0x2001 } });
        source.addList(0x3001, { { 1, 0x3002 } });
        EXPECT_THAT(resolvedAcrossSeeds(source, 0x3001, 5), ElementsAre(0x2001u));

        source.addList(0x3003, { { 1, 0x3004 } });
        source.addList(0x3004, { { 1, 0x3003 } });
        EXPECT_THAT(resolvedAcrossSeeds(source, 0x3003, 5), IsEmpty());
    }

    TEST(ESM4CreatureModelTest, aListThatGivesNothingByChanceGivesNoCreature)
    {
        TestSource source;
        source.addCreature(0x2001, "creatures\\a\\skeleton.nif");
        source.addList(0x3001, { { 1, 0x2001 } }, 0, 100);
        source.addList(0x3002, { { 1, 0x2001 } }, 0, 0);

        EXPECT_THAT(resolvedAcrossSeeds(source, 0x3001, 5), IsEmpty());
        EXPECT_THAT(resolvedAcrossSeeds(source, 0x3002, 5), ElementsAre(0x2001u));
    }

    TEST(ESM4CreatureModelTest, theSameSeedMakesTheSameChoice)
    {
        TestSource source;
        source.addCreature(0x2001, "creatures\\a\\skeleton.nif");
        source.addCreature(0x2002, "creatures\\b\\skeleton.nif");
        source.addList(0x3001, { { 1, 0x2001 }, { 1, 0x2002 } });

        std::set<const ESM4::Creature*> seen;
        for (std::uint32_t seed = 1; seed <= 64; ++seed)
        {
            const ESM4::Creature* first = ESM4::resolveCreature(source, id(0x3001), 5, seed);
            EXPECT_EQ(first, ESM4::resolveCreature(source, id(0x3001), 5, seed));
            seen.insert(first);
        }
        // and different seeds make different choices
        EXPECT_EQ(seen.size(), 2u);
    }

    TEST(ESM4CreatureModelTest, aCreatureWhoseTemplateIsAListTakesTheModelOfOneOfItsCreatures)
    {
        TestSource source;
        source.addCreature(0x2001, "creatures\\a\\skeleton.nif", {}, useModel, 0x3001);
        source.addCreature(0x2002, "creatures\\b\\skeleton.nif");
        source.addCreature(0x2003, "creatures\\c\\skeleton.nif");
        source.addList(0x3001, { { 1, 0x2002 }, { 1, 0x2003 } });

        std::set<std::string> skeletons;
        for (std::uint32_t seed = 1; seed <= 64; ++seed)
            skeletons.insert(
                ESM4::creatureModel(source, source.mCreatures.at(id(0x2001)), playerLevel, seed).mSkeleton);
        EXPECT_THAT(skeletons, UnorderedElementsAre("creatures/b/skeleton.nif", "creatures/c/skeleton.nif"));
    }
}

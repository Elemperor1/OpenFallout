#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/crafting.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <map>
#include <set>
#include <thread>
#include <vector>

namespace
{
    using namespace testing;
    using FalloutScript::ActorState;
    using FalloutScript::ActorValues;
    using FalloutScript::Crafting;
    using FalloutScript::FormId;
    using FalloutScript::Recipe;

    constexpr FormId thePlayer = 0x14;
    constexpr int repair = 38;
    constexpr int science = 43;
    constexpr FormId workbench = 0x01000500;
    constexpr FormId campfire = 0x01000501;
    constexpr FormId aid = 0x01000510;
    constexpr FormId scrapMetal = 0x01000001;
    constexpr FormId tinCan = 0x01000002;
    constexpr FormId wonderglue = 0x01000003;
    constexpr FormId caps = 0x0000000F;
    constexpr FormId pipePistol = 0x01000010;
    constexpr FormId pistolAmmo = 0x01000011;
    constexpr FormId rations = 0x01000012;
    constexpr FormId recipeBox = 0x01000100;
    constexpr FormId recipePistol = 0x01000101;
    constexpr FormId recipeRations = 0x01000102;
    constexpr FormId recipeSecret = 0x01000103;

    class NoRecords : public FalloutScript::ActorRecords
    {
    public:
        bool initialItems(FormId, std::vector<std::pair<FormId, int>>&) override { return true; }
        bool initialFactions(FormId, std::vector<std::pair<FormId, int>>&) override { return true; }
    };

    class TestRecipes final : public FalloutScript::RecipeRecords
    {
    public:
        std::map<FormId, Recipe> mRecipes;
        std::map<FormId, std::vector<FormId>> mCategories;
        std::set<FormId> mForbidden;

        void add(FormId category, FormId id, int skill, int level, std::vector<FalloutScript::RecipeItem> ingredients,
            std::vector<FalloutScript::RecipeItem> outputs, FormId subCategory = 0)
        {
            Recipe& recipe = mRecipes[id];
            recipe.mId = id;
            recipe.mName = "recipe";
            recipe.mSkill = skill;
            recipe.mLevel = level;
            recipe.mCategory = category;
            recipe.mSubCategory = subCategory;
            recipe.mIngredients = std::move(ingredients);
            recipe.mOutputs = std::move(outputs);
            mCategories[category].push_back(id);
        }

        std::vector<FormId> recipesOf(FormId category) override
        {
            const auto found = mCategories.find(category);
            return found == mCategories.end() ? std::vector<FormId>() : found->second;
        }
        bool recipe(FormId id, Recipe& result) override
        {
            const auto found = mRecipes.find(id);
            if (found == mRecipes.end())
                return false;
            result = found->second;
            return true;
        }
        bool allowed(FormId id, FormId) override { return mForbidden.count(id) == 0; }
    };

    struct Fixture
    {
        NoRecords mActorRecords;
        TestRecipes mRecipes;
        ActorState mState{ mActorRecords };
        ActorValues mValues;
        Crafting mCrafting{ mRecipes, mState, mValues };

        Fixture()
        {
            mRecipes.add(
                workbench, recipeBox, repair, 25, { { scrapMetal, 3 }, { tinCan, 1 } }, { { pipePistol, 1 } }, aid);
            mRecipes.add(workbench, recipePistol, science, 50, { { scrapMetal, 1 } }, { { pistolAmmo, 10 } });
            mRecipes.add(workbench, recipeSecret, -1, 0, {}, { { caps, 5 } });
            mRecipes.add(campfire, recipeRations, -1, 0, { { wonderglue, 1 } }, { { rations, 2 } });
            mRecipes.mForbidden.insert(recipeSecret);
            mState.addItem(thePlayer, scrapMetal, 4);
            mState.addItem(thePlayer, tinCan, 1);
            mValues.setBase(thePlayer, repair, 30);
            mValues.setBase(thePlayer, science, 20);
        }
    };

    TEST(FalloutScriptCraftingTest, listsTheRecipesOfTheCategoryThatTheCharacterIsAllowed)
    {
        Fixture f;
        const std::vector<Crafting::Entry> entries = f.mCrafting.entries(thePlayer, workbench);
        ASSERT_EQ(entries.size(), 2u);
        EXPECT_EQ(entries[0].mRecipe.mId, recipeBox);
        EXPECT_EQ(entries[1].mRecipe.mId, recipePistol);
        EXPECT_THAT(f.mCrafting.entries(thePlayer, 0x01000999), IsEmpty());
    }

    TEST(FalloutScriptCraftingTest, tellsWhatIsMissingForEachRecipe)
    {
        Fixture f;
        const std::vector<Crafting::Entry> entries = f.mCrafting.entries(thePlayer, workbench);
        ASSERT_EQ(entries.size(), 2u);
        EXPECT_EQ(entries[0].mStatus, Crafting::Status::Ready);
        EXPECT_THAT(entries[0].mHave, ElementsAre(4, 1));
        EXPECT_DOUBLE_EQ(entries[0].mSkill, 30);
        // The skill of the second is too low although the scrap is there
        EXPECT_EQ(entries[1].mStatus, Crafting::Status::NeedsSkill);
        EXPECT_THAT(entries[1].mHave, ElementsAre(4));
    }

    TEST(FalloutScriptCraftingTest, aRecipeNeedsEnoughOfEveryIngredient)
    {
        Fixture f;
        f.mState.removeItem(thePlayer, scrapMetal, 2);
        EXPECT_EQ(f.mCrafting.status(thePlayer, recipeBox), Crafting::Status::NeedsItems);
        f.mState.addItem(thePlayer, scrapMetal, 1);
        EXPECT_EQ(f.mCrafting.status(thePlayer, recipeBox), Crafting::Status::Ready);
    }

    TEST(FalloutScriptCraftingTest, makingTakesTheIngredientsAndGivesTheResults)
    {
        Fixture f;
        EXPECT_EQ(f.mCrafting.craft(thePlayer, recipeBox), Crafting::Status::Ready);
        EXPECT_EQ(f.mState.itemCount(thePlayer, scrapMetal), 1);
        EXPECT_EQ(f.mState.itemCount(thePlayer, tinCan), 0);
        EXPECT_EQ(f.mState.itemCount(thePlayer, pipePistol), 1);
        // The ingredients are gone, so it can not be made again
        EXPECT_EQ(f.mCrafting.craft(thePlayer, recipeBox), Crafting::Status::NeedsItems);
        EXPECT_EQ(f.mState.itemCount(thePlayer, pipePistol), 1);
    }

    TEST(FalloutScriptCraftingTest, twoCraftsAtOnceForTheLastIngredientsMakeOnlyOne)
    {
        for (int round = 0; round < 200; ++round)
        {
            Fixture f;
            f.mState.removeItem(thePlayer, scrapMetal, 4);
            f.mState.addItem(thePlayer, scrapMetal, 3);
            std::atomic<int> ready = 0;
            std::atomic<int> made = 0;
            std::vector<std::thread> crafters;
            for (int i = 0; i < 2; ++i)
                crafters.emplace_back([&] {
                    ++ready;
                    while (ready < 2)
                        std::this_thread::yield();
                    if (f.mCrafting.craft(thePlayer, recipeBox) == Crafting::Status::Ready)
                        ++made;
                });
            for (std::thread& crafter : crafters)
                crafter.join();

            ASSERT_EQ(made, 1) << "round " << round;
            ASSERT_EQ(f.mState.itemCount(thePlayer, pipePistol), 1) << "round " << round;
            ASSERT_EQ(f.mState.itemCount(thePlayer, scrapMetal), 0) << "round " << round;
        }
    }

    TEST(FalloutScriptCraftingTest, nothingIsTakenWhenARecipeCanNotBeMade)
    {
        Fixture f;
        EXPECT_EQ(f.mCrafting.craft(thePlayer, recipePistol), Crafting::Status::NeedsSkill);
        EXPECT_EQ(f.mState.itemCount(thePlayer, scrapMetal), 4);
        EXPECT_EQ(f.mState.itemCount(thePlayer, pistolAmmo), 0);
    }

    TEST(FalloutScriptCraftingTest, aRecipeThatNeedsNoSkillOnlyNeedsItsIngredients)
    {
        Fixture f;
        EXPECT_EQ(f.mCrafting.craft(thePlayer, recipeRations), Crafting::Status::NeedsItems);
        f.mState.addItem(thePlayer, wonderglue, 1);
        EXPECT_EQ(f.mCrafting.craft(thePlayer, recipeRations), Crafting::Status::Ready);
        EXPECT_EQ(f.mState.itemCount(thePlayer, rations), 2);
    }

    TEST(FalloutScriptCraftingTest, aRecipeTheCharacterIsNotAllowedIsNotMadeEither)
    {
        Fixture f;
        EXPECT_EQ(f.mCrafting.craft(thePlayer, recipeSecret), Crafting::Status::NotAllowed);
        EXPECT_EQ(f.mState.itemCount(thePlayer, caps), 0);
        EXPECT_EQ(f.mCrafting.status(thePlayer, 0x01000777), Crafting::Status::Unknown);
    }
}

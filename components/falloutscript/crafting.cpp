#include "crafting.hpp"

#include "actors.hpp"
#include "actorvalues.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace FalloutScript
{
    Crafting::Crafting(RecipeRecords& records, ActorState& state, ActorValues& values)
        : mRecords(records)
        , mState(state)
        , mValues(values)
    {
    }

    Crafting::Entry Crafting::describe(FormId crafter, const Recipe& recipe)
    {
        Entry entry;
        entry.mRecipe = recipe;
        entry.mStatus = Status::Ready;
        if (recipe.mSkill >= 0)
            entry.mSkill = mValues.current(crafter, recipe.mSkill);
        if (recipe.mSkill >= 0 && entry.mSkill < recipe.mLevel)
            entry.mStatus = Status::NeedsSkill;
        // An item the recipe names more than once is needed as many times as it is named in all
        std::map<FormId, std::int64_t> needed;
        for (const RecipeItem& ingredient : recipe.mIngredients)
            needed[ingredient.mItem] += std::max(ingredient.mCount, 0);
        for (const RecipeItem& ingredient : recipe.mIngredients)
        {
            const int have = mState.itemCount(crafter, ingredient.mItem);
            entry.mHave.push_back(have);
            if (have < needed[ingredient.mItem] && entry.mStatus == Status::Ready)
                entry.mStatus = Status::NeedsItems;
        }
        return entry;
    }

    std::vector<Crafting::Entry> Crafting::entries(FormId crafter, FormId category)
    {
        std::vector<Entry> result;
        for (const FormId id : mRecords.recipesOf(category))
        {
            Recipe recipe;
            if (!mRecords.recipe(id, recipe) || !mRecords.allowed(id, crafter))
                continue;
            result.push_back(describe(crafter, recipe));
        }
        return result;
    }

    Crafting::Status Crafting::status(FormId crafter, FormId recipe)
    {
        Recipe found;
        if (!mRecords.recipe(recipe, found))
            return Status::Unknown;
        if (!mRecords.allowed(recipe, crafter))
            return Status::NotAllowed;
        return describe(crafter, found).mStatus;
    }

    Crafting::Status Crafting::craft(FormId crafter, FormId recipe)
    {
        const Status state = status(crafter, recipe);
        if (state != Status::Ready)
            return state;
        Recipe found;
        mRecords.recipe(recipe, found);
        // The ingredients are taken in one step, so that of two crafts at once for the last of them only one is made
        std::vector<std::pair<FormId, int>> wanted;
        for (const RecipeItem& ingredient : found.mIngredients)
            wanted.emplace_back(ingredient.mItem, ingredient.mCount);
        if (!mState.takeItems(crafter, wanted))
            return Status::NeedsItems;
        for (const RecipeItem& output : found.mOutputs)
            mState.addItem(crafter, output.mItem, output.mCount);
        return Status::Ready;
    }
}

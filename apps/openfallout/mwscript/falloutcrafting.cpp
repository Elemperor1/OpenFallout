#include "falloutscripts.hpp"

#include <algorithm>

#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadimod.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadrcct.hpp>
#include <components/esm4/loadrcpe.hpp>
#include <components/esm4/loadweap.hpp>

#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    namespace
    {
        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }

        FalloutScript::FormId numberOf(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }

        FalloutScript::FormId numberOf(ESM::FormId32 form)
        {
            return numberOf(ESM::FormId::fromUint32(form));
        }

        /// The actor value a recipe names, -1 for none (the records write 0xFFFFFFFF)
        int skillOf(std::int32_t value)
        {
            return value >= 0 && value < FalloutScript::ActorValue::count ? value : -1;
        }

        const char* statusWord(FalloutScript::Crafting::Status status)
        {
            switch (status)
            {
                case FalloutScript::Crafting::Status::Ready:
                    return "ready";
                case FalloutScript::Crafting::Status::NeedsSkill:
                    return "skill";
                case FalloutScript::Crafting::Status::NeedsItems:
                    return "items";
                case FalloutScript::Crafting::Status::NotAllowed:
                case FalloutScript::Crafting::Status::Unknown:
                    break;
            }
            return "";
        }
    }

    std::vector<FalloutScript::FormId> FalloutScripts::recipesOf(FalloutScript::FormId category)
    {
        std::vector<FalloutScript::FormId> result;
        for (const ESM4::Recipe& recipe : mStore.get<ESM4::Recipe>())
            if (numberOf(recipe.mData.mCategory) == category)
                result.push_back(numberOf(recipe.mId));
        return result;
    }

    bool FalloutScripts::recipe(FalloutScript::FormId id, FalloutScript::Recipe& result)
    {
        const ESM4::Recipe* record = mStore.get<ESM4::Recipe>().search(refId(id));
        if (record == nullptr)
            return false;
        result = FalloutScript::Recipe();
        result.mId = id;
        result.mName = record->mFullName.empty() ? record->mEditorId : record->mFullName;
        result.mSkill = skillOf(record->mData.mSkill);
        result.mLevel = static_cast<int>(record->mData.mLevel);
        result.mCategory = numberOf(record->mData.mCategory);
        result.mSubCategory = numberOf(record->mData.mSubCategory);
        for (const ESM4::Recipe::Item& item : record->mIngredients)
            result.mIngredients.push_back({ numberOf(item.mItem), static_cast<int>(item.mCount) });
        for (const ESM4::Recipe::Item& item : record->mOutputs)
            result.mOutputs.push_back({ numberOf(item.mItem), static_cast<int>(item.mCount) });
        return true;
    }

    bool FalloutScripts::allowed(FalloutScript::FormId recipe, FalloutScript::FormId crafter)
    {
        const ESM4::Recipe* record = mStore.get<ESM4::Recipe>().search(refId(recipe));
        if (record == nullptr)
            return false;
        if (record->mConditions.empty())
            return true;
        FalloutScript::ConditionContext context;
        context.mSubject = crafter;
        return mConditions.holds(record->mConditions, context);
    }

    std::string FalloutScripts::itemName(FalloutScript::FormId item)
    {
        const ESM::RefId record = refId(item);
        std::string name;
        const auto from = [&name](const auto* found) {
            if (found == nullptr)
                return false;
            name = found->mFullName;
            return true;
        };
        const bool known = from(mStore.get<ESM4::Weapon>().search(record))
            || from(mStore.get<ESM4::Armor>().search(record)) || from(mStore.get<ESM4::Ammunition>().search(record))
            || from(mStore.get<ESM4::Potion>().search(record)) || from(mStore.get<ESM4::MiscItem>().search(record))
            || from(mStore.get<ESM4::Book>().search(record)) || from(mStore.get<ESM4::Ingredient>().search(record))
            || from(mStore.get<ESM4::ItemMod>().search(record)) || from(mStore.get<ESM4::Key>().search(record));
        if (!known)
            return std::string();
        return name.empty() ? editorId(item) : name;
    }

    bool FalloutScripts::openRecipes(FalloutScript::FormId category)
    {
        const ESM4::RecipeCategory* record = mStore.get<ESM4::RecipeCategory>().search(refId(category));
        if (record == nullptr)
            return false;
        const std::lock_guard lock(mRecipeMutex);
        mRecipeMenu = RecipeMenu{ category, record->mFullName.empty() ? record->mEditorId : record->mFullName,
            ++mRecipeSerial };
        return true;
    }

    std::optional<FalloutScripts::RecipeMenu> FalloutScripts::recipeMenu()
    {
        const std::lock_guard lock(mRecipeMutex);
        return mRecipeMenu;
    }

    void FalloutScripts::closeRecipes()
    {
        const std::lock_guard lock(mRecipeMutex);
        mRecipeMenu.reset();
    }

    std::vector<FalloutScripts::RecipeLine> FalloutScripts::recipeLines()
    {
        std::vector<RecipeLine> lines;
        const std::optional<RecipeMenu> menu = recipeMenu();
        if (!menu)
            return lines;
        const auto items = [this](const std::vector<FalloutScript::RecipeItem>& wanted, const std::vector<int>& have) {
            std::vector<RecipeItem> result;
            for (std::size_t i = 0; i < wanted.size(); ++i)
            {
                RecipeItem item;
                item.mItem = wanted[i].mItem;
                item.mName = itemName(wanted[i].mItem);
                item.mCount = wanted[i].mCount;
                item.mHave = i < have.size() ? have[i] : mActors.itemCount(mPlayer, wanted[i].mItem);
                result.push_back(std::move(item));
            }
            return result;
        };
        for (const FalloutScript::Crafting::Entry& entry : mCrafting.entries(mPlayer, menu->mCategory))
        {
            RecipeLine line;
            line.mId = entry.mRecipe.mId;
            line.mName = entry.mRecipe.mName;
            line.mSubCategory = entry.mRecipe.mSubCategory;
            if (const ESM4::RecipeCategory* sub = mStore.get<ESM4::RecipeCategory>().search(refId(line.mSubCategory)))
                line.mSubCategoryName = sub->mFullName.empty() ? sub->mEditorId : sub->mFullName;
            line.mStatus = statusWord(entry.mStatus);
            line.mSkill = entry.mRecipe.mSkill;
            line.mLevel = entry.mRecipe.mLevel;
            line.mSkillValue = entry.mSkill;
            line.mIngredients = items(entry.mRecipe.mIngredients, entry.mHave);
            line.mOutputs = items(entry.mRecipe.mOutputs, {});
            lines.push_back(std::move(line));
        }
        return lines;
    }

    std::string FalloutScripts::craftRecipe(FalloutScript::FormId recipe)
    {
        const std::optional<RecipeMenu> menu = recipeMenu();
        if (!menu)
            return std::string();
        FalloutScript::Recipe record;
        if (!this->recipe(recipe, record) || record.mCategory != menu->mCategory)
            return std::string();
        const FalloutScript::Crafting::Status status = mCrafting.craft(mPlayer, recipe);
        return status == FalloutScript::Crafting::Status::Ready ? "made" : statusWord(status);
    }
}

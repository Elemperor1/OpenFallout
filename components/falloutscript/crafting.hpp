#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CRAFTING_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CRAFTING_H

#include <string>
#include <vector>

#include "script.hpp"

namespace FalloutScript
{
    class ActorState;
    class ActorValues;

    /// An item and how many of it a recipe takes or gives
    struct RecipeItem
    {
        FormId mItem = 0;
        int mCount = 0;
    };

    /// What a recipe (RCPE) of New Vegas says: the skill and the level of it the character needs, where the menu lists
    /// it, what it takes and what it gives
    struct Recipe
    {
        FormId mId = 0;
        std::string mName;
        /// The actor value of the skill, -1 for a recipe that needs none
        int mSkill = -1;
        int mLevel = 0;
        FormId mCategory = 0;
        FormId mSubCategory = 0;
        std::vector<RecipeItem> mIngredients;
        std::vector<RecipeItem> mOutputs;
    };

    class RecipeRecords
    {
    public:
        virtual ~RecipeRecords() = default;

        /// The recipes of a category (RCCT) in the order of the files, those of its sub-categories too
        virtual std::vector<FormId> recipesOf(FormId category) = 0;
        /// False for a form that is no recipe
        virtual bool recipe(FormId id, Recipe& result) = 0;
        /// Whether the conditions of the recipe hold for the character: a recipe that needs a perk or a note is not
        /// listed for one who has neither
        virtual bool allowed(FormId recipe, FormId crafter) = 0;
    };

    /// Making things from other things at a workbench, a reloading bench or a campfire: the menu lists the recipes of a
    /// category that the character may use, says which it has the skill and the things for, and making one takes the
    /// ingredients from the character and gives it what comes out. It can be used from any thread.
    class Crafting
    {
    public:
        Crafting(RecipeRecords& records, ActorState& state, ActorValues& values);

        enum class Status
        {
            /// Everything that is needed is there
            Ready,
            /// The skill is below the level of the recipe
            NeedsSkill,
            /// The character has too few of an ingredient
            NeedsItems,
            /// The conditions of the recipe do not hold for the character (a perk or a note it lacks)
            NotAllowed,
            /// The form is no recipe
            Unknown,
        };

        /// A recipe of the menu as the character sees it
        struct Entry
        {
            Recipe mRecipe;
            Status mStatus = Status::Ready;
            /// How many of each ingredient the character has, in the order of the recipe
            std::vector<int> mHave;
            /// The skill of the character
            double mSkill = 0;
        };

        /// The recipes of the category the character may use (those it is not allowed are left out)
        std::vector<Entry> entries(FormId crafter, FormId category);

        Status status(FormId crafter, FormId recipe);

        /// Makes the recipe: when the status is Ready the ingredients are taken and the results given. Returns the
        /// status, which is Ready when the recipe was made.
        Status craft(FormId crafter, FormId recipe);

    private:
        Entry describe(FormId crafter, const Recipe& recipe);

        RecipeRecords& mRecords;
        ActorState& mState;
        ActorValues& mValues;
    };
}

#endif

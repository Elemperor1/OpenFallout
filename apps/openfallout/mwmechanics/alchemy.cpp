#include "alchemy.hpp"

#include <cassert>
#include <cmath>

#include <algorithm>
#include <format>
#include <map>
#include <stdexcept>

#include <components/misc/rng.hpp>

#include <components/esm3/loadappa.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadskil.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"

#include "creaturestats.hpp"
#include "magiceffects.hpp"

namespace
{
    constexpr size_t sNumEffects = 4;

    std::optional<OFMechanics::EffectKey> toKey(const ESM::Ingredient& ingredient, size_t i)
    {
        if (ingredient.mData.mEffectID[i].empty())
            return {};
        ESM::RefId arg = ingredient.mData.mSkills[i];
        if (arg.empty())
            arg = ingredient.mData.mAttributes[i];
        return OFMechanics::EffectKey(ingredient.mData.mEffectID[i], arg);
    }

    bool containsEffect(const ESM::Ingredient& ingredient, const OFMechanics::EffectKey& effect)
    {
        for (size_t j = 0; j < sNumEffects; ++j)
        {
            if (toKey(ingredient, j) == effect)
                return true;
        }
        return false;
    }
}

OFMechanics::Alchemy::Alchemy()
    : mValue(0)
{
}

std::vector<OFMechanics::EffectKey> OFMechanics::Alchemy::listEffects() const
{
    // We care about the order of these effects as each effect can affect the next when applied.
    // The player can affect effect order by placing ingredients into different slots
    std::vector<EffectKey> effects;
    for (size_t slotI = 0; slotI < mIngredients.size() - 1; ++slotI)
    {
        if (mIngredients[slotI].isEmpty())
            continue;
        const ESM::Ingredient* ingredient = mIngredients[slotI].get<ESM::Ingredient>()->mBase;
        for (size_t slotJ = slotI + 1; slotJ < mIngredients.size(); ++slotJ)
        {
            if (mIngredients[slotJ].isEmpty())
                continue;
            const ESM::Ingredient* ingredient2 = mIngredients[slotJ].get<ESM::Ingredient>()->mBase;
            for (size_t i = 0; i < sNumEffects; ++i)
            {
                if (const auto key = toKey(*ingredient, i))
                {
                    if (std::find(effects.begin(), effects.end(), *key) != effects.end())
                        continue;
                    if (containsEffect(*ingredient2, *key))
                        effects.push_back(*key);
                }
            }
        }
    }
    return effects;
}

void OFMechanics::Alchemy::applyTools(int flags, float& value) const
{
    bool magnitude = !(flags & ESM::MagicEffect::NoMagnitude);
    bool duration = !(flags & ESM::MagicEffect::NoDuration);
    bool negative = (flags & ESM::MagicEffect::Harmful) != 0;

    int tool = negative ? ESM::Apparatus::Alembic : ESM::Apparatus::Retort;

    int setup = 0;

    if (!mTools[tool].isEmpty() && !mTools[ESM::Apparatus::Calcinator].isEmpty())
        setup = 1;
    else if (!mTools[tool].isEmpty())
        setup = 2;
    else if (!mTools[ESM::Apparatus::Calcinator].isEmpty())
        setup = 3;
    else
        return;

    float toolQuality = setup == 1 || setup == 2 ? mTools[tool].get<ESM::Apparatus>()->mBase->mData.mQuality : 0;
    float calcinatorQuality = setup == 1 || setup == 3
        ? mTools[ESM::Apparatus::Calcinator].get<ESM::Apparatus>()->mBase->mData.mQuality
        : 0;

    float quality = 1;

    switch (setup)
    {
        case 1:

            quality = negative ? 2 * toolQuality + 3 * calcinatorQuality
                               : (magnitude && duration ? 2 * toolQuality + calcinatorQuality
                                                        : 2 / 3.0f * (toolQuality + calcinatorQuality) + 0.5f);
            break;

        case 2:

            quality = negative ? 1 + toolQuality : (magnitude && duration ? toolQuality : toolQuality + 0.5f);
            break;

        case 3:

            quality = magnitude && duration ? calcinatorQuality : calcinatorQuality + 0.5f;
            break;
    }

    if (setup == 3 || !negative)
    {
        value += quality;
    }
    else
    {
        if (quality == 0)
            throw std::runtime_error("invalid derived alchemy apparatus quality");

        value /= quality;
    }
}

void OFMechanics::Alchemy::updateEffects()
{
    mEffects.clear();
    mValue = 0;

    if (countIngredients() < 2 || mAlchemist.isEmpty() || mTools[ESM::Apparatus::MortarPestle].isEmpty())
        return;

    // find effects
    std::vector<EffectKey> effects = listEffects();

    // general alchemy factor
    float x = getAlchemyFactor();

    x *= mTools[ESM::Apparatus::MortarPestle].get<ESM::Apparatus>()->mBase->mData.mQuality;
    x *= OFBase::Environment::get()
             .getESMStore()
             ->get<ESM::GameSetting>()
             .find("fPotionStrengthMult")
             ->mValue.getFloat();

    // value
    mValue = static_cast<int>(
        x * OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>().find("iAlchemyMod")->mValue.getFloat());

    // build quantified effect list
    for (const auto& effectKey : effects)
    {
        const ESM::MagicEffect* magicEffect
            = OFBase::Environment::get().getESMStore()->get<ESM::MagicEffect>().find(effectKey.mId);

        if (magicEffect->mData.mBaseCost <= 0)
        {
            const std::string os = std::format("invalid base cost for magic effect {}", effectKey.mId.getRefIdString());
            throw std::runtime_error(os);
        }

        float fPotionT1MagMul = OFBase::Environment::get()
                                    .getESMStore()
                                    ->get<ESM::GameSetting>()
                                    .find("fPotionT1MagMult")
                                    ->mValue.getFloat();

        if (fPotionT1MagMul <= 0)
            throw std::runtime_error("invalid gmst: fPotionT1MagMul");

        float fPotionT1DurMult = OFBase::Environment::get()
                                     .getESMStore()
                                     ->get<ESM::GameSetting>()
                                     .find("fPotionT1DurMult")
                                     ->mValue.getFloat();

        if (fPotionT1DurMult <= 0)
            throw std::runtime_error("invalid gmst: fPotionT1DurMult");

        float magnitude = (magicEffect->mData.mFlags & ESM::MagicEffect::NoMagnitude)
            ? 1.0f
            : (x / fPotionT1MagMul) / magicEffect->mData.mBaseCost;
        float duration = (magicEffect->mData.mFlags & ESM::MagicEffect::NoDuration)
            ? 1.0f
            : (x / fPotionT1DurMult) / magicEffect->mData.mBaseCost;

        if (!(magicEffect->mData.mFlags & ESM::MagicEffect::NoMagnitude))
            applyTools(magicEffect->mData.mFlags, magnitude);

        if (!(magicEffect->mData.mFlags & ESM::MagicEffect::NoDuration))
            applyTools(magicEffect->mData.mFlags, duration);

        duration = roundf(duration);
        magnitude = roundf(magnitude);

        if (magnitude > 0 && duration > 0)
        {
            ESM::ENAMstruct effect;
            effect.mEffectID = effectKey.mId;

            if (magicEffect->mData.mFlags & ESM::MagicEffect::TargetSkill)
                effect.mSkill = effectKey.mArg;
            else if (magicEffect->mData.mFlags & ESM::MagicEffect::TargetAttribute)
                effect.mAttribute = effectKey.mArg;

            effect.mRange = 0;
            effect.mArea = 0;

            effect.mDuration = static_cast<int>(duration);
            effect.mMagnMin = effect.mMagnMax = static_cast<int>(magnitude);

            mEffects.push_back(effect);
        }
    }
}

const ESM::Potion* OFMechanics::Alchemy::getRecord(const ESM::Potion& toFind) const
{
    const OFWorld::Store<ESM::Potion>& potions = OFBase::Environment::get().getESMStore()->get<ESM::Potion>();

    OFWorld::Store<ESM::Potion>::iterator iter = potions.begin();
    for (; iter != potions.end(); ++iter)
    {
        if (iter->mEffects.mList.size() != mEffects.size())
            continue;

        if (iter->mName != toFind.mName || iter->mScript != toFind.mScript
            || iter->mData.mWeight != toFind.mData.mWeight || iter->mData.mValue != toFind.mData.mValue
            || iter->mData.mFlags != toFind.mData.mFlags)
            continue;

        // Don't choose an ID that came from the content files, would have unintended side effects
        // where alchemy can be used to produce quest-relevant items
        if (!potions.isDynamic(iter->mId))
            continue;

        bool mismatch = false;

        for (size_t i = 0; i < iter->mEffects.mList.size(); ++i)
        {
            const ESM::IndexedENAMstruct& first = iter->mEffects.mList[i];
            const ESM::ENAMstruct& second = mEffects[i];

            if (first.mData.mEffectID != second.mEffectID || first.mData.mArea != second.mArea
                || first.mData.mRange != second.mRange || first.mData.mSkill != second.mSkill
                || first.mData.mAttribute != second.mAttribute || first.mData.mMagnMin != second.mMagnMin
                || first.mData.mMagnMax != second.mMagnMax || first.mData.mDuration != second.mDuration)
            {
                mismatch = true;
                break;
            }
        }

        if (!mismatch)
            return &(*iter);
    }

    return nullptr;
}

void OFMechanics::Alchemy::removeIngredients()
{
    for (TIngredientsContainer::iterator iter(mIngredients.begin()); iter != mIngredients.end(); ++iter)
        if (!iter->isEmpty())
        {
            iter->getContainerStore()->remove(*iter, 1);

            if (iter->getCellRef().getCount() < 1)
                *iter = OFWorld::Ptr();
        }

    updateEffects();
}

void OFMechanics::Alchemy::addPotion(const std::string& name)
{
    ESM::Potion newRecord;

    newRecord.mData.mWeight = 0;

    for (TIngredientsIterator iter(beginIngredients()); iter != endIngredients(); ++iter)
        if (!iter->isEmpty())
            newRecord.mData.mWeight += iter->get<ESM::Ingredient>()->mBase->mData.mWeight;

    if (countIngredients() > 0)
        newRecord.mData.mWeight /= countIngredients();

    newRecord.mData.mValue = mValue;
    newRecord.mData.mFlags = 0;
    newRecord.mRecordFlags = 0;

    newRecord.mName = name;

    auto& prng = OFBase::Environment::get().getWorld()->getPrng();
    int index = Misc::Rng::rollDice(6, prng);
    assert(index >= 0 && index < 6);

    constexpr std::string_view meshes[] = { "standard", "bargain", "cheap", "fresh", "exclusive", "quality" };

    const std::string_view mesh = meshes[index];

    newRecord.mModel = std::format("m\\misc_potion_{}_01.nif", mesh);
    newRecord.mIcon = std::format("m\\tx_potion_{}_01.dds", mesh);

    newRecord.mEffects.populate(mEffects);

    const ESM::Potion* record = getRecord(newRecord);
    if (!record)
        record = OFBase::Environment::get().getESMStore()->insert(newRecord);

    mAlchemist.getClass().getContainerStore(mAlchemist).add(record->mId, 1);
}

void OFMechanics::Alchemy::increaseSkill()
{
    mAlchemist.getClass().skillUsageSucceeded(mAlchemist, ESM::Skill::Alchemy, ESM::Skill::Alchemy_CreatePotion);
}

float OFMechanics::Alchemy::getAlchemyFactor() const
{
    const CreatureStats& creatureStats = mAlchemist.getClass().getCreatureStats(mAlchemist);

    return (mAlchemist.getClass().getSkill(mAlchemist, ESM::Skill::Alchemy)
        + 0.1f * creatureStats.getAttribute(ESM::Attribute::Intelligence).getModified()
        + 0.1f * creatureStats.getAttribute(ESM::Attribute::Luck).getModified());
}

int OFMechanics::Alchemy::countIngredients() const
{
    int ingredients = 0;

    for (TIngredientsIterator iter(beginIngredients()); iter != endIngredients(); ++iter)
        if (!iter->isEmpty())
            ++ingredients;

    return ingredients;
}

int OFMechanics::Alchemy::countPotionsToBrew() const
{
    Result readyStatus = getReadyStatus();
    if (readyStatus != Result_Success)
        return 0;

    int toBrew = -1;

    for (TIngredientsIterator iter(beginIngredients()); iter != endIngredients(); ++iter)
        if (!iter->isEmpty())
        {
            int count = iter->getCellRef().getCount();
            if ((count > 0 && count < toBrew) || toBrew < 0)
                toBrew = count;
        }

    return toBrew;
}

void OFMechanics::Alchemy::setAlchemist(const OFWorld::Ptr& npc)
{
    mAlchemist = npc;

    mIngredients.resize(4);

    std::fill(mIngredients.begin(), mIngredients.end(), OFWorld::Ptr());

    mTools.resize(4);

    std::vector<OFWorld::Ptr> prevTools(mTools);

    std::fill(mTools.begin(), mTools.end(), OFWorld::Ptr());

    mEffects.clear();

    OFWorld::ContainerStore& store = npc.getClass().getContainerStore(npc);

    for (auto iter(store.begin(OFWorld::ContainerStore::Type_Apparatus)); iter != store.end(); ++iter)
    {
        OFWorld::LiveCellRef<ESM::Apparatus>* ref = iter->get<ESM::Apparatus>();

        int type = ref->mBase->mData.mType;

        if (type < 0 || type >= static_cast<int>(mTools.size()))
            throw std::runtime_error("invalid apparatus type");

        if (prevTools[type] == *iter)
            mTools[type] = *iter; // prefer the previous tool if still in the container

        if (!mTools[type].isEmpty() && !prevTools[type].isEmpty() && mTools[type] == prevTools[type])
            continue;

        if (!mTools[type].isEmpty())
            if (ref->mBase->mData.mQuality <= mTools[type].get<ESM::Apparatus>()->mBase->mData.mQuality)
                continue;

        mTools[type] = *iter;
    }
}

OFMechanics::Alchemy::TToolsIterator OFMechanics::Alchemy::beginTools() const
{
    return mTools.begin();
}

OFMechanics::Alchemy::TToolsIterator OFMechanics::Alchemy::endTools() const
{
    return mTools.end();
}

OFMechanics::Alchemy::TIngredientsIterator OFMechanics::Alchemy::beginIngredients() const
{
    return mIngredients.begin();
}

OFMechanics::Alchemy::TIngredientsIterator OFMechanics::Alchemy::endIngredients() const
{
    return mIngredients.end();
}

void OFMechanics::Alchemy::clear()
{
    mAlchemist = OFWorld::Ptr();
    mIngredients.clear();
    mEffects.clear();
    setPotionName("");
}

void OFMechanics::Alchemy::setPotionName(const std::string& name)
{
    mPotionName = name;
}

int OFMechanics::Alchemy::addIngredient(const OFWorld::Ptr& ingredient)
{
    // find a free slot
    int slot = -1;

    for (int i = 0; i < static_cast<int>(mIngredients.size()); ++i)
        if (mIngredients[i].isEmpty())
        {
            slot = i;
            break;
        }

    if (slot == -1)
        return -1;

    for (TIngredientsIterator iter(mIngredients.begin()); iter != mIngredients.end(); ++iter)
        if (!iter->isEmpty() && ingredient.getCellRef().getRefId() == iter->getCellRef().getRefId())
            return -1;

    mIngredients[slot] = ingredient;

    updateEffects();

    return slot;
}

void OFMechanics::Alchemy::removeIngredient(size_t index)
{
    if (index < mIngredients.size())
    {
        mIngredients[index] = OFWorld::Ptr();
        updateEffects();
    }
}

void OFMechanics::Alchemy::addApparatus(const OFWorld::Ptr& apparatus)
{
    int32_t slot = apparatus.get<ESM::Apparatus>()->mBase->mData.mType;

    mTools[slot] = apparatus;

    updateEffects();
}

void OFMechanics::Alchemy::removeApparatus(size_t index)
{
    if (index < mTools.size())
    {
        mTools[index] = OFWorld::Ptr();
        updateEffects();
    }
}

OFMechanics::Alchemy::TEffectsIterator OFMechanics::Alchemy::beginEffects() const
{
    return mEffects.begin();
}

OFMechanics::Alchemy::TEffectsIterator OFMechanics::Alchemy::endEffects() const
{
    return mEffects.end();
}

bool OFMechanics::Alchemy::knownEffect(size_t potionEffectIndex, const OFWorld::Ptr& npc)
{
    float alchemySkill = npc.getClass().getSkill(npc, ESM::Skill::Alchemy);
    static const float fWortChanceValue
        = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>().find("fWortChanceValue")->mValue.getFloat();
    return (potionEffectIndex <= 1 && alchemySkill >= fWortChanceValue)
        || (potionEffectIndex <= 3 && alchemySkill >= fWortChanceValue * 2)
        || (potionEffectIndex <= 5 && alchemySkill >= fWortChanceValue * 3)
        || (potionEffectIndex <= 7 && alchemySkill >= fWortChanceValue * 4);
}

OFMechanics::Alchemy::Result OFMechanics::Alchemy::getReadyStatus() const
{
    if (mTools[ESM::Apparatus::MortarPestle].isEmpty())
        return Result_NoMortarAndPestle;

    if (countIngredients() < 2)
        return Result_LessThanTwoIngredients;

    if (mPotionName.empty())
        return Result_NoName;

    if (listEffects().empty())
        return Result_NoEffects;

    return Result_Success;
}

OFMechanics::Alchemy::Result OFMechanics::Alchemy::create(const std::string& name, int& count)
{
    setPotionName(name);
    Result readyStatus = getReadyStatus();

    if (readyStatus == Result_NoEffects)
        removeIngredients();

    if (readyStatus != Result_Success)
        return readyStatus;

    OFBase::Environment::get().getWorld()->breakInvisibility(mAlchemist);

    Result result = Result_RandomFailure;
    int brewedCount = 0;
    for (int i = 0; i < count; ++i)
    {
        if (createSingle() == Result_Success)
        {
            result = Result_Success;
            brewedCount++;
        }
    }

    count = brewedCount;
    return result;
}

OFMechanics::Alchemy::Result OFMechanics::Alchemy::createSingle()
{
    if (beginEffects() == endEffects())
    {
        // all effects were nullified due to insufficient skill
        removeIngredients();
        return Result_RandomFailure;
    }
    auto& prng = OFBase::Environment::get().getWorld()->getPrng();
    if (getAlchemyFactor() < Misc::Rng::roll0to99(prng))
    {
        removeIngredients();
        return Result_RandomFailure;
    }

    addPotion(mPotionName);

    removeIngredients();

    increaseSkill();

    return Result_Success;
}

std::string OFMechanics::Alchemy::suggestPotionName()
{
    std::vector<OFMechanics::EffectKey> effects = listEffects();
    if (effects.empty())
        return {};

    return effects.begin()->toString();
}

std::vector<std::string> OFMechanics::Alchemy::effectsDescription(
    const OFWorld::ConstPtr& ptr, const float alchemySkill)
{
    std::vector<std::string> effects;

    const auto& item = ptr.get<ESM::Ingredient>()->mBase;
    const auto& store = OFBase::Environment::get().getESMStore();
    const auto& mgef = store->get<ESM::MagicEffect>();
    const static auto fWortChanceValue = store->get<ESM::GameSetting>().find("fWortChanceValue")->mValue.getFloat();
    const auto& data = item->mData;

    for (size_t i = 0; i < sNumEffects; ++i)
    {
        const auto effectID = data.mEffectID[i];

        if (alchemySkill < fWortChanceValue * static_cast<int>(i + 1))
            break;

        if (!effectID.empty())
        {
            const ESM::Attribute* attribute = store->get<ESM::Attribute>().search(data.mAttributes[i]);
            const ESM::Skill* skill = store->get<ESM::Skill>().search(data.mSkills[i]);
            std::string effect = getMagicEffectString(*mgef.find(effectID), attribute, skill);

            effects.push_back(std::move(effect));
        }
    }
    return effects;
}

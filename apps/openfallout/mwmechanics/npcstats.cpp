#include "npcstats.hpp"

#include <cassert>
#include <format>

#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadfact.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/npcstats.hpp>

#include <MyGUI_TextIterator.h>

#include "../mwworld/esmstore.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

OFMechanics::NpcStats::NpcStats()
    : mDisposition(0)
    , mCrimeDispositionModifier(0)
    , mReputation(0)
    , mCrimeId(-1)
    , mBounty(0)
    , mWerewolfKills(0)
    , mLevelProgress(0)
    , mTimeToStartDrowning(-1.0) // set breath to special value, it will be replaced during actor update
    , mIsWerewolf(false)
{
    mSpecIncreases.resize(3, 0);
    for (const ESM::Skill& skill : OFBase::Environment::get().getESMStore()->get<ESM::Skill>())
        mSkills.emplace(skill.mId, SkillValue{});
}

int OFMechanics::NpcStats::getBaseDisposition() const
{
    return mDisposition;
}

void OFMechanics::NpcStats::setBaseDisposition(int disposition)
{
    mDisposition = disposition;
}

int OFMechanics::NpcStats::getCrimeDispositionModifier() const
{
    return mCrimeDispositionModifier;
}

void OFMechanics::NpcStats::setCrimeDispositionModifier(int value)
{
    mCrimeDispositionModifier = value;
}

void OFMechanics::NpcStats::modCrimeDispositionModifier(int value)
{
    mCrimeDispositionModifier += value;
}

const OFMechanics::SkillValue& OFMechanics::NpcStats::getSkill(ESM::RefId id) const
{
    auto it = mSkills.find(id);
    if (it == mSkills.end())
        throw std::runtime_error("skill not found");
    return it->second;
}

OFMechanics::SkillValue& OFMechanics::NpcStats::getSkill(ESM::RefId id)
{
    auto it = mSkills.find(id);
    if (it == mSkills.end())
        throw std::runtime_error("skill not found");
    return it->second;
}

void OFMechanics::NpcStats::setSkill(ESM::RefId id, const OFMechanics::SkillValue& value)
{
    auto it = mSkills.find(id);
    if (it == mSkills.end())
        throw std::runtime_error("skill not found");
    it->second = value;
}

const std::map<ESM::RefId, int>& OFMechanics::NpcStats::getFactionRanks() const
{
    return mFactionRank;
}

int OFMechanics::NpcStats::getFactionRank(const ESM::RefId& faction) const
{
    auto it = mFactionRank.find(faction);
    if (it != mFactionRank.end())
        return it->second;

    return -1;
}

void OFMechanics::NpcStats::joinFaction(const ESM::RefId& faction)
{
    auto it = mFactionRank.find(faction);
    if (it == mFactionRank.end())
        mFactionRank[faction] = 0;
}

void OFMechanics::NpcStats::setFactionRank(const ESM::RefId& faction, int newRank)
{
    auto it = mFactionRank.find(faction);
    if (it != mFactionRank.end())
    {
        const ESM::Faction* factionPtr = OFBase::Environment::get().getESMStore()->get<ESM::Faction>().find(faction);
        if (newRank < 0)
        {
            mFactionRank.erase(it);
            mExpelled.erase(faction);
        }
        else if (newRank < static_cast<int>(factionPtr->mData.mRankData.size()))
            do
                it->second = newRank;
            // Does the new rank exist?
            while (newRank > 0 && factionPtr->mRanks[newRank--].empty());
    }
}

bool OFMechanics::NpcStats::getExpelled(const ESM::RefId& factionID) const
{
    return mExpelled.find(factionID) != mExpelled.end();
}

void OFMechanics::NpcStats::expell(const ESM::RefId& factionID, bool printMessage)
{
    if (mExpelled.find(factionID) == mExpelled.end())
    {
        mExpelled.insert(factionID);
        if (!printMessage)
            return;

        std::string message = "#{sExpelledMessage}";
        message += OFBase::Environment::get().getESMStore()->get<ESM::Faction>().find(factionID)->mName;
        OFBase::Environment::get().getWindowManager()->messageBox(message);
    }
}

void OFMechanics::NpcStats::clearExpelled(const ESM::RefId& factionID)
{
    mExpelled.erase(factionID);
}

bool OFMechanics::NpcStats::isInFaction(const ESM::RefId& faction) const
{
    return (mFactionRank.find(faction) != mFactionRank.end());
}

int OFMechanics::NpcStats::getFactionReputation(const ESM::RefId& faction) const
{
    auto iter = mFactionReputation.find(faction);

    if (iter == mFactionReputation.end())
        return 0;

    return iter->second;
}

void OFMechanics::NpcStats::setFactionReputation(const ESM::RefId& faction, int value)
{
    mFactionReputation[faction] = value;
}

namespace
{
    float getTypeFactor(ESM::RefId id, const ESM::Class& npcClass)
    {
        const auto& gmst = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
        for (const auto& skill : npcClass.mData.mMinorSkills)
        {
            if (skill == id)
                return gmst.find("fMinorSkillBonus")->mValue.getFloat();
        }
        for (const auto& skill : npcClass.mData.mMajorSkills)
        {
            if (skill == id)
                return gmst.find("fMajorSkillBonus")->mValue.getFloat();
        }
        return gmst.find("fMiscSkillBonus")->mValue.getFloat();
    }
}

float OFMechanics::NpcStats::getSkillProgressRequirement(ESM::RefId id, const ESM::Class& npcClass) const
{
    float progressRequirement = 1.f + getSkill(id).getBase();

    const OFWorld::Store<ESM::GameSetting>& gmst = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
    const ESM::Skill* skill = OFBase::Environment::get().getESMStore()->get<ESM::Skill>().find(id);

    const float typeFactor = getTypeFactor(id, npcClass);

    progressRequirement *= typeFactor;

    if (typeFactor <= 0)
        throw std::runtime_error("invalid skill type factor");

    float specialisationFactor = 1;

    if (skill->mData.mSpecialization == npcClass.mData.mSpecialization)
    {
        specialisationFactor = gmst.find("fSpecialSkillBonus")->mValue.getFloat();

        if (specialisationFactor <= 0)
            throw std::runtime_error("invalid skill specialisation factor");
    }
    progressRequirement *= specialisationFactor;

    return progressRequirement;
}

int OFMechanics::NpcStats::getLevelProgress() const
{
    return mLevelProgress;
}

void OFMechanics::NpcStats::setLevelProgress(int progress)
{
    mLevelProgress = progress;
}

void OFMechanics::NpcStats::levelUp()
{
    const OFWorld::Store<ESM::GameSetting>& gmst = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

    mLevelProgress -= gmst.find("iLevelUpTotal")->mValue.getInteger();
    mLevelProgress = std::max(0, mLevelProgress); // might be necessary when levelup was invoked via console

    mSkillIncreases.clear();

    const float endurance = getAttribute(ESM::Attribute::Endurance).getBase();

    // "When you gain a level, in addition to increasing three primary attributes, your Health
    // will automatically increase by 10% of your Endurance attribute. If you increased Endurance this level,
    // the Health increase is calculated from the increased Endurance"
    // Note: we should add bonus Health points to current level too.
    float healthGain = endurance * gmst.find("fLevelUpHealthEndMult")->mValue.getFloat();
    OFMechanics::DynamicStat<float> health(getHealth());
    health.setBase(getHealth().getBase() + healthGain);
    health.setCurrent(std::max(1.f, getHealth().getCurrent() + healthGain));
    setHealth(health);

    setLevel(getLevel() + 1);
}

void OFMechanics::NpcStats::updateHealth()
{
    const float endurance = getAttribute(ESM::Attribute::Endurance).getBase();
    const float strength = getAttribute(ESM::Attribute::Strength).getBase();

    setHealth(floor(0.5f * (strength + endurance)));
}

int OFMechanics::NpcStats::getLevelupAttributeMultiplier(ESM::RefId attribute) const
{
    const auto it = mSkillIncreases.find(attribute);
    if (it == mSkillIncreases.end())
        return 1;
    const int num = std::clamp(it->second, 0, 10);
    if (num == 0)
        return 1;

    // iLevelUp01Mult - iLevelUp10Mult
    const std::string id = std::format("iLevelUp{:0>2}Mult", num);
    if (const ESM::GameSetting* gmst = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>().search(id))
        return gmst->mValue.getInteger();

    return 1;
}

int OFMechanics::NpcStats::getSkillIncreasesForAttribute(ESM::RefId attribute) const
{
    auto it = mSkillIncreases.find(attribute);
    if (it == mSkillIncreases.end())
        return 0;
    return it->second;
}

void OFMechanics::NpcStats::setSkillIncreasesForAttribute(ESM::RefId attribute, int increases)
{
    if (increases == 0)
        mSkillIncreases.erase(attribute);
    else
        mSkillIncreases[attribute] = increases;
}

int OFMechanics::NpcStats::getSkillIncreasesForSpecialization(ESM::Class::Specialization spec) const
{
    return mSpecIncreases[spec];
}

void OFMechanics::NpcStats::setSkillIncreasesForSpecialization(ESM::Class::Specialization spec, int increases)
{
    assert(spec >= 0 && spec < 3);
    mSpecIncreases[spec] = increases;
}

void OFMechanics::NpcStats::flagAsUsed(const ESM::RefId& id)
{
    mUsedIds.insert(id);
}

bool OFMechanics::NpcStats::hasBeenUsed(const ESM::RefId& id) const
{
    return mUsedIds.find(id) != mUsedIds.end();
}

int OFMechanics::NpcStats::getBounty() const
{
    return mBounty;
}

void OFMechanics::NpcStats::setBounty(int bounty)
{
    mBounty = bounty;
}

int OFMechanics::NpcStats::getReputation() const
{
    return mReputation;
}

void OFMechanics::NpcStats::setReputation(int reputation)
{
    // Reputation is capped in original engine
    mReputation = std::clamp(reputation, 0, 255);
}

int OFMechanics::NpcStats::getCrimeId() const
{
    return mCrimeId;
}

void OFMechanics::NpcStats::setCrimeId(int id)
{
    mCrimeId = id;
}

bool OFMechanics::NpcStats::hasSkillsForRank(const ESM::RefId& factionId, int rank) const
{
    const ESM::Faction& faction = *OFBase::Environment::get().getESMStore()->get<ESM::Faction>().find(factionId);

    const ESM::RankData& rankData = faction.mData.mRankData.at(rank);

    std::vector<int> skills;

    for (const ESM::RefId& id : faction.mData.mSkills)
    {
        if (!id.empty())
            skills.push_back(static_cast<int>(getSkill(id).getBase()));
    }

    if (skills.empty())
        return true;

    std::sort(skills.begin(), skills.end());

    std::vector<int>::const_reverse_iterator iter = skills.rbegin();

    if (*iter < rankData.mPrimarySkill)
        return false;

    if (skills.size() < 2)
        return true;

    iter++;
    if (*iter < rankData.mFavouredSkill)
        return false;

    if (skills.size() < 3)
        return true;

    iter++;
    if (*iter < rankData.mFavouredSkill)
        return false;

    return true;
}

bool OFMechanics::NpcStats::isWerewolf() const
{
    return mIsWerewolf;
}

void OFMechanics::NpcStats::setWerewolf(bool set)
{
    if (mIsWerewolf == set)
        return;

    if (set != false)
    {
        mWerewolfKills = 0;
    }
    mIsWerewolf = set;
}

int OFMechanics::NpcStats::getWerewolfKills() const
{
    return mWerewolfKills;
}

void OFMechanics::NpcStats::addWerewolfKill()
{
    ++mWerewolfKills;
}

float OFMechanics::NpcStats::getTimeToStartDrowning() const
{
    return mTimeToStartDrowning;
}

void OFMechanics::NpcStats::setTimeToStartDrowning(float time)
{
    mTimeToStartDrowning = time;
}

void OFMechanics::NpcStats::writeState(ESM::CreatureStats& state) const
{
    CreatureStats::writeState(state);
}

void OFMechanics::NpcStats::writeState(ESM::NpcStats& state) const
{
    for (std::map<ESM::RefId, int>::const_iterator iter(mFactionRank.begin()); iter != mFactionRank.end(); ++iter)
        state.mFactions[iter->first].mRank = iter->second;

    state.mDisposition = mDisposition;
    state.mCrimeDispositionModifier = mCrimeDispositionModifier;

    for (const auto& [id, value] : mSkills)
        value.writeState(state.mSkills[id]);

    state.mIsWerewolf = mIsWerewolf;

    state.mCrimeId = mCrimeId;

    state.mBounty = mBounty;

    for (auto iter(mExpelled.begin()); iter != mExpelled.end(); ++iter)
        state.mFactions[*iter].mExpelled = true;

    for (auto iter(mFactionReputation.begin()); iter != mFactionReputation.end(); ++iter)
        state.mFactions[iter->first].mReputation = iter->second;

    state.mReputation = mReputation;
    state.mWerewolfKills = mWerewolfKills;
    state.mLevelProgress = mLevelProgress;

    state.mSkillIncrease = mSkillIncreases;

    for (size_t i = 0; i < state.mSpecIncreases.size(); ++i)
        state.mSpecIncreases[i] = mSpecIncreases[i];

    std::copy(mUsedIds.begin(), mUsedIds.end(), std::back_inserter(state.mUsedIds));

    state.mTimeToStartDrowning = mTimeToStartDrowning;
}
void OFMechanics::NpcStats::readState(const ESM::CreatureStats& state)
{
    CreatureStats::readState(state);
}

void OFMechanics::NpcStats::readState(const ESM::NpcStats& state)
{
    const OFWorld::ESMStore& store = *OFBase::Environment::get().getESMStore();

    for (auto iter(state.mFactions.begin()); iter != state.mFactions.end(); ++iter)
        if (store.get<ESM::Faction>().search(iter->first))
        {
            if (iter->second.mExpelled)
                mExpelled.insert(iter->first);

            if (iter->second.mRank >= 0)
                mFactionRank[iter->first] = iter->second.mRank;

            if (iter->second.mReputation)
                mFactionReputation[iter->first] = iter->second.mReputation;
        }

    mDisposition = state.mDisposition;
    mCrimeDispositionModifier = state.mCrimeDispositionModifier;

    for (const auto& [id, value] : state.mSkills)
        mSkills[id].readState(value);

    mIsWerewolf = state.mIsWerewolf;

    mCrimeId = state.mCrimeId;
    mBounty = state.mBounty;
    mReputation = state.mReputation;
    mWerewolfKills = state.mWerewolfKills;
    mLevelProgress = state.mLevelProgress;

    mSkillIncreases = state.mSkillIncrease;

    for (size_t i = 0; i < state.mSpecIncreases.size(); ++i)
        mSpecIncreases[i] = state.mSpecIncreases[i];

    for (auto iter(state.mUsedIds.begin()); iter != state.mUsedIds.end(); ++iter)
        if (store.find(*iter))
            mUsedIds.insert(*iter);

    mTimeToStartDrowning = state.mTimeToStartDrowning;
}

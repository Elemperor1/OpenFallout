#include "chargen.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>

#include "arguments.hpp"
#include "interpreter.hpp"

namespace FalloutScript
{
    namespace
    {
        // The numbers of the attributes as actor values
        constexpr int strength = 5;
        constexpr int perception = 6;
        constexpr int endurance = 7;
        constexpr int charisma = 8;
        constexpr int intelligence = 9;
        constexpr int agility = 10;
        constexpr int luck = 11;

        /// A number that a command asks for (points, skills), within what a screen can show: a value that is huge or
        /// not a number is no more than a thousand
        int countOf(double value)
        {
            constexpr double largest = 1000.0;
            if (!(value > 0.0))
                return 0;
            return static_cast<int>(std::lround(std::min(value, largest)));
        }

        bool isControl(unsigned char c)
        {
            return c < 0x20 || c == 0x7f;
        }

        bool isSpace(unsigned char c)
        {
            return c == ' ' || c == '\t';
        }
    }

    Chargen::Chargen(Game game)
        : mGame(game)
        , mSkills(skillsOf(game))
    {
        mAttributes.fill(5);
    }

    std::vector<ChargenSkill> Chargen::skillsOf(Game game)
    {
        // Skills are the actor values 32 to 45. The one at 33 (Big Guns) and the one at 44 (Survival) are in one game
        // each: Fallout 3 does not use 44, and New Vegas shows 13 skills without Big Guns.
        std::vector<ChargenSkill> skills = {
            { 32, charisma, "Barter", "Barter" },
        };
        if (game == Game::Fallout3)
            skills.push_back({ 33, endurance, "BigGuns", "Big Guns" });
        skills.push_back({ 34, perception, "EnergyWeapons", "Energy Weapons" });
        skills.push_back({ 35, perception, "Explosives", "Explosives" });
        skills.push_back({ 36, perception, "Lockpick", "Lockpick" });
        skills.push_back({ 37, intelligence, "Medicine", "Medicine" });
        skills.push_back({ 38, strength, "MeleeWeapons", "Melee Weapons" });
        skills.push_back({ 39, intelligence, "Repair", "Repair" });
        skills.push_back({ 40, intelligence, "Science", "Science" });
        if (game == Game::Fallout3)
            skills.push_back({ 41, agility, "SmallGuns", "Small Guns" });
        else
            skills.push_back({ 41, agility, "Guns", "Guns" });
        skills.push_back({ 42, agility, "Sneak", "Sneak" });
        skills.push_back({ 43, charisma, "Speech", "Speech" });
        if (game == Game::NewVegas)
            skills.push_back({ 44, endurance, "Survival", "Survival" });
        skills.push_back({ 45, endurance, "Unarmed", "Unarmed" });
        return skills;
    }

    void Chargen::clear()
    {
        const std::lock_guard lock(mMutex);
        mInChargen = false;
        mSpecialMade = false;
        mScreens.clear();
        mPointTotal = attributePoints;
        mTagsAllowed = tagLimit;
        mValues.clear();
        mSerial = 0;
        mTagsAtStart.clear();
        mLastAdd = false;
        mLastTagged.clear();
        mLastUntagged.clear();
        mName.clear();
        mFemale.reset();
        mRace = 0;
        mAttributes.fill(5);
        mTagSlots.clear();
        mTraits.clear();
        mGivenTraits.clear();
    }

    void Chargen::setInChargen(bool inChargen)
    {
        const std::lock_guard lock(mMutex);
        mInChargen = inChargen;
    }

    bool Chargen::inChargen() const
    {
        const std::lock_guard lock(mMutex);
        return mInChargen;
    }

    void Chargen::open(
        const std::vector<ChargenPage>& pages, const std::vector<double>& values, int menu, int count, bool add)
    {
        const std::lock_guard lock(mMutex);
        const bool wasOpen = !mScreens.empty();
        for (const ChargenPage page : pages)
        {
            // A game without traits has no screen for them
            if (page == ChargenPage::Traits && mGame != Game::NewVegas)
                continue;
            mScreens.push_back({ page, menu, count, add });
        }
        if (!wasOpen && !mScreens.empty())
        {
            mValues = values;
            ++mSerial;
            activate();
        }
    }

    void Chargen::activate()
    {
        if (mScreens.empty())
            return;
        const Screen& screen = mScreens.front();
        if (screen.mPage == ChargenPage::Special)
        {
            const int sum = attributeSum();
            const int total = screen.mCount <= 0 ? attributePoints : screen.mAdd ? sum + screen.mCount : screen.mCount;
            mPointTotal = std::clamp(total, attributeCount * minimumAttribute, attributeCount * maximumAttribute);
        }
        else if (screen.mPage == ChargenPage::Skills)
        {
            const int allowed = screen.mCount <= 0 ? tagLimit
                : screen.mAdd                      ? taggedCount() + screen.mCount
                                                   : screen.mCount;
            mTagsAllowed = std::clamp(allowed, 0, static_cast<int>(mSkills.size()));
            mTagsAtStart = tagsLocked();
        }
    }

    int Chargen::attributeSum() const
    {
        return std::accumulate(mAttributes.begin(), mAttributes.end(), 0);
    }

    int Chargen::taggedCount() const
    {
        return static_cast<int>(
            std::count_if(mTagSlots.begin(), mTagSlots.end(), [](int skill) { return skill != 0; }));
    }

    int Chargen::menu() const
    {
        const std::lock_guard lock(mMutex);
        return mScreens.empty() ? 0 : mScreens.front().mMenu;
    }

    std::optional<ChargenPage> Chargen::page() const
    {
        const std::lock_guard lock(mMutex);
        if (mScreens.empty())
            return std::nullopt;
        return mScreens.front().mPage;
    }

    std::vector<double> Chargen::values() const
    {
        const std::lock_guard lock(mMutex);
        return mValues;
    }

    std::uint32_t Chargen::serial() const
    {
        const std::lock_guard lock(mMutex);
        return mSerial;
    }

    std::optional<ChargenPage> Chargen::finishPage()
    {
        const std::lock_guard lock(mMutex);
        if (mScreens.empty())
            return std::nullopt;
        const Screen screen = mScreens.front();
        const ChargenPage done = screen.mPage;
        // More points given than the screen has: the player takes some back before going on
        if (done == ChargenPage::Special && attributeSum() > mPointTotal)
            return std::nullopt;
        mLastAdd = screen.mAdd;
        mLastTagged.clear();
        mLastUntagged.clear();
        if (done == ChargenPage::Skills)
        {
            const std::vector<int> now = tagsLocked();
            std::set_difference(
                now.begin(), now.end(), mTagsAtStart.begin(), mTagsAtStart.end(), std::back_inserter(mLastTagged));
            std::set_difference(
                mTagsAtStart.begin(), mTagsAtStart.end(), now.begin(), now.end(), std::back_inserter(mLastUntagged));
        }
        mScreens.pop_front();
        if (done == ChargenPage::Special)
            mSpecialMade = true;
        if (!mScreens.empty())
        {
            ++mSerial;
            activate();
        }
        return done;
    }

    bool Chargen::lastFinishAdded() const
    {
        const std::lock_guard lock(mMutex);
        return mLastAdd;
    }

    std::vector<int> Chargen::lastTagged() const
    {
        const std::lock_guard lock(mMutex);
        return mLastTagged;
    }

    std::vector<int> Chargen::lastUntagged() const
    {
        const std::lock_guard lock(mMutex);
        return mLastUntagged;
    }

    bool Chargen::specialMade() const
    {
        const std::lock_guard lock(mMutex);
        return mSpecialMade;
    }

    bool Chargen::menuOpen() const
    {
        const std::lock_guard lock(mMutex);
        return !mScreens.empty();
    }

    std::string Chargen::setName(std::string_view name)
    {
        std::string cleaned;
        cleaned.reserve(name.size());
        for (const char c : name)
            if (!isControl(static_cast<unsigned char>(c)))
                cleaned += c;
        const auto first = std::find_if(cleaned.begin(), cleaned.end(), [](char c) { return !isSpace(c); });
        cleaned.erase(cleaned.begin(), first);
        while (!cleaned.empty() && isSpace(static_cast<unsigned char>(cleaned.back())))
            cleaned.pop_back();
        if (cleaned.size() > nameLimit)
        {
            // Not in the middle of a character of UTF-8 (a byte that continues one starts with the bits 10)
            std::size_t cut = nameLimit;
            while (cut > 0 && (static_cast<unsigned char>(cleaned[cut]) & 0xC0) == 0x80)
                --cut;
            cleaned.resize(cut);
            while (!cleaned.empty() && isSpace(static_cast<unsigned char>(cleaned.back())))
                cleaned.pop_back();
        }
        const std::lock_guard lock(mMutex);
        mName = cleaned;
        return cleaned;
    }

    std::string Chargen::name() const
    {
        const std::lock_guard lock(mMutex);
        return mName;
    }

    void Chargen::setFemale(bool female)
    {
        const std::lock_guard lock(mMutex);
        mFemale = female;
    }

    std::optional<bool> Chargen::female() const
    {
        const std::lock_guard lock(mMutex);
        return mFemale;
    }

    void Chargen::setRace(FormId race)
    {
        const std::lock_guard lock(mMutex);
        mRace = race;
    }

    FormId Chargen::race() const
    {
        const std::lock_guard lock(mMutex);
        return mRace;
    }

    int Chargen::attribute(int number) const
    {
        if (number < firstAttribute || number >= firstAttribute + attributeCount)
            return 0;
        const std::lock_guard lock(mMutex);
        return mAttributes[static_cast<std::size_t>(number - firstAttribute)];
    }

    void Chargen::setAttributes(const std::array<int, attributeCount>& values)
    {
        const std::lock_guard lock(mMutex);
        for (std::size_t i = 0; i < values.size(); ++i)
            mAttributes[i] = std::clamp(values[i], minimumAttribute, maximumAttribute);
        // A screen that gives out points on top of what the attributes are when it opens counts them from here
        if (!mScreens.empty() && mScreens.front().mPage == ChargenPage::Special)
            activate();
    }

    int Chargen::pointsLeft() const
    {
        const std::lock_guard lock(mMutex);
        return mPointTotal - attributeSum();
    }

    int Chargen::pointTotal() const
    {
        const std::lock_guard lock(mMutex);
        return mPointTotal;
    }

    bool Chargen::adjustAttribute(int number, int delta)
    {
        if (number < firstAttribute || number >= firstAttribute + attributeCount)
            return false;
        const std::lock_guard lock(mMutex);
        int& value = mAttributes[static_cast<std::size_t>(number - firstAttribute)];
        // A change that is out of the whole range is no change (and adding it could overflow)
        if (delta < minimumAttribute - maximumAttribute || delta > maximumAttribute - minimumAttribute)
            return false;
        const int wanted = value + delta;
        if (wanted < minimumAttribute || wanted > maximumAttribute)
            return false;
        // Taking points back is always allowed; giving them needs them to be left
        const int left = mPointTotal - attributeSum();
        if (delta > 0 && delta > left)
            return false;
        value = wanted;
        return true;
    }

    bool Chargen::isSkill(int number) const
    {
        return std::any_of(
            mSkills.begin(), mSkills.end(), [number](const ChargenSkill& s) { return s.mNumber == number; });
    }

    int Chargen::skillValue(int skill) const
    {
        const auto found = std::find_if(
            mSkills.begin(), mSkills.end(), [skill](const ChargenSkill& s) { return s.mNumber == skill; });
        if (found == mSkills.end())
            return 0;
        const std::lock_guard lock(mMutex);
        const int governing = mAttributes[static_cast<std::size_t>(found->mAttribute - firstAttribute)];
        const int lucky = mAttributes[static_cast<std::size_t>(luck - firstAttribute)];
        const bool isTagged = std::find(mTagSlots.begin(), mTagSlots.end(), skill) != mTagSlots.end();
        return skillBase + 2 * governing + lucky / 2 + (isTagged ? tagBonus : 0);
    }

    bool Chargen::tagged(int skill) const
    {
        const std::lock_guard lock(mMutex);
        return skill != 0 && std::find(mTagSlots.begin(), mTagSlots.end(), skill) != mTagSlots.end();
    }

    std::vector<int> Chargen::tags() const
    {
        const std::lock_guard lock(mMutex);
        return tagsLocked();
    }

    std::vector<int> Chargen::tagsLocked() const
    {
        std::vector<int> result;
        for (const int skill : mTagSlots)
            if (skill != 0)
                result.push_back(skill);
        std::sort(result.begin(), result.end());
        return result;
    }

    void Chargen::setTags(const std::vector<int>& skills)
    {
        const std::lock_guard lock(mMutex);
        mTagSlots.clear();
        for (const int skill : skills)
        {
            if (static_cast<int>(mTagSlots.size()) >= mTagsAllowed)
                break;
            if (isSkill(skill) && std::find(mTagSlots.begin(), mTagSlots.end(), skill) == mTagSlots.end())
                mTagSlots.push_back(skill);
        }
    }

    int Chargen::tagsAllowed() const
    {
        const std::lock_guard lock(mMutex);
        return mTagsAllowed;
    }

    bool Chargen::toggleTag(int skill)
    {
        if (!isSkill(skill))
            return false;
        const std::lock_guard lock(mMutex);
        const auto found = std::find(mTagSlots.begin(), mTagSlots.end(), skill);
        if (found != mTagSlots.end())
        {
            *found = 0;
            return true;
        }
        if (taggedCount() >= mTagsAllowed)
            return false;
        const auto free = std::find(mTagSlots.begin(), mTagSlots.end(), 0);
        if (free != mTagSlots.end())
            *free = skill;
        else
            mTagSlots.push_back(skill);
        return true;
    }

    bool Chargen::setTagSlot(int slot, int skill)
    {
        constexpr int slotLimit = 16;
        if (slot < 0 || slot >= slotLimit || !isSkill(skill))
            return false;
        const std::lock_guard lock(mMutex);
        std::replace(mTagSlots.begin(), mTagSlots.end(), skill, 0);
        if (mTagSlots.size() <= static_cast<std::size_t>(slot))
            mTagSlots.resize(static_cast<std::size_t>(slot) + 1, 0);
        mTagSlots[static_cast<std::size_t>(slot)] = skill;
        return true;
    }

    bool Chargen::hasTrait(FormId trait) const
    {
        const std::lock_guard lock(mMutex);
        return std::find(mTraits.begin(), mTraits.end(), trait) != mTraits.end();
    }

    std::vector<FormId> Chargen::traits() const
    {
        const std::lock_guard lock(mMutex);
        return mTraits;
    }

    void Chargen::setTraits(const std::vector<FormId>& traits)
    {
        const std::lock_guard lock(mMutex);
        mTraits.clear();
        for (const FormId trait : traits)
        {
            if (mTraits.size() >= static_cast<std::size_t>(traitLimit))
                break;
            if (trait != 0 && std::find(mTraits.begin(), mTraits.end(), trait) == mTraits.end())
                mTraits.push_back(trait);
        }
    }

    bool Chargen::toggleTrait(FormId trait)
    {
        if (trait == 0)
            return false;
        const std::lock_guard lock(mMutex);
        const auto found = std::find(mTraits.begin(), mTraits.end(), trait);
        if (found != mTraits.end())
        {
            mTraits.erase(found);
            return true;
        }
        if (mTraits.size() >= static_cast<std::size_t>(traitLimit))
            return false;
        mTraits.push_back(trait);
        return true;
    }

    std::vector<FormId> Chargen::givenTraits() const
    {
        const std::lock_guard lock(mMutex);
        return mGivenTraits;
    }

    void Chargen::setGivenTraits(std::vector<FormId> traits)
    {
        const std::lock_guard lock(mMutex);
        mGivenTraits = std::move(traits);
    }

    void addChargenCommands(Interpreter& interpreter, Chargen& chargen, std::function<void()> opened)
    {
        // The numbers are those of the menus in the executables (MenuMode N asks for them)
        constexpr int raceSexMenu = 1036;
        constexpr int chargenMenu = 1048;
        constexpr int textEditMenu = 1051;
        constexpr int specialBookMenu = 1060;
        constexpr int loveTesterMenu = 1074;
        constexpr int traitSelectMenu = 1076;
        constexpr int traitMenu = 1084;

        // What a command opens: the screens, the number of the menu, and whether its first argument is a count that the
        // screen gives out (a count of 0 or less opens nothing, unless the screen has a default) and adds to what the
        // character has
        struct Menu
        {
            std::vector<ChargenPage> mPages;
            int mNumber = 0;
            bool mCounted = false;
            bool mNeedsCount = false;
            bool mAdd = false;
        };
        const auto open = [&chargen, opened](const Menu& menu) {
            return [&chargen, opened, menu](CallContext& call) {
                std::vector<double> values;
                for (const Value& argument : call.mArguments)
                    values.push_back(argument.mNumber);
                const int count = menu.mCounted && !values.empty() ? countOf(values[0]) : 0;
                if (menu.mNeedsCount && count <= 0)
                    return 0.0;
                chargen.open(menu.mPages, values, menu.mNumber, count, menu.mAdd);
                if (opened)
                    opened();
                return 0.0;
            };
        };

        // The scripts of the games ask for the name with GetPlayerName, after ShowRaceMenu in the same script; the menu
        // for the name is the same
        interpreter.setHandler("GetPlayerName", open({ { ChargenPage::Name }, textEditMenu }));
        interpreter.setHandler("ShowNameMenu", open({ { ChargenPage::Name }, textEditMenu }));
        interpreter.setHandler("ShowRaceMenu", open({ { ChargenPage::Race }, raceSexMenu }));
        // The scripts of the games give the book and the vigor tester 40 points, and SetSPECIALPoints too; the commands
        // without a number have the default, which is that
        const Menu book{ { ChargenPage::Special }, specialBookMenu, true };
        const Menu tester{ { ChargenPage::Special }, loveTesterMenu, true };
        interpreter.setHandler("ShowSPECIALBookMenu", open(book));
        interpreter.setHandler("ShowSPECIALBookMenuParams", open(book));
        interpreter.setHandler("ShowLoveTesterMenu", open(tester));
        interpreter.setHandler("ShowLoveTesterMenuParams", open(tester));
        // These four are what the scripts of the games use to give out the points of SPECIAL and to pick the tags,
        // when the player makes the character and again with each perk that gives a point or a tag
        interpreter.setHandler("SetSPECIALPoints", open({ { ChargenPage::Special }, chargenMenu, true, true }));
        interpreter.setHandler("AddSPECIALPoints", open({ { ChargenPage::Special }, chargenMenu, true, true, true }));
        interpreter.setHandler("SetTagSkills", open({ { ChargenPage::Skills }, chargenMenu, true, true }));
        interpreter.setHandler("AddTagSkills", open({ { ChargenPage::Skills }, chargenMenu, true, true, true }));
        // The menu with the numbers: the first number is the points of SPECIAL, and when there are none the third is
        // the skills to tag; with none the games open nothing. No script of the games calls these.
        const auto numbered = [&chargen, opened, chargenMenu](CallContext& call) {
            std::vector<double> values;
            for (const Value& argument : call.mArguments)
                values.push_back(argument.mNumber);
            const auto at = [&values](std::size_t index) { return index < values.size() ? countOf(values[index]) : 0; };
            if (at(0) > 0)
                chargen.open({ ChargenPage::Special }, values, chargenMenu, at(0));
            else if (at(2) > 0)
                chargen.open({ ChargenPage::Skills }, values, chargenMenu, at(2));
            else
                return 0.0;
            if (opened)
                opened();
            return 0.0;
        };
        interpreter.setHandler("ShowChargenMenu", numbered);
        interpreter.setHandler("ShowChargenMenuParams", numbered);
        interpreter.setHandler("ShowChargenMenuModValues", numbered);
        interpreter.setHandler("ShowTraitMenu", open({ { ChargenPage::Traits }, traitMenu }));
        interpreter.setHandler("ShowTraitSelectMenu", open({ { ChargenPage::Traits }, traitSelectMenu }));

        // The sex of the player, which a script picks from the answer of a message with two buttons
        interpreter.setHandler("SexChange", [&chargen](CallContext& call) {
            if (call.mArguments.empty() || call.mReference != call.mHost.player())
                return 0.0;
            chargen.setFemale(intArgument(call, 0) != 0);
            return 1.0;
        });
        // The tests of the games fill the places of the tags one after the other, and the screen of the skills starts
        // from them
        interpreter.setHandler("SetPlayerTagSkill", [&chargen](CallContext& call) {
            return chargen.setTagSlot(intArgument(call, 1), intArgument(call, 0)) ? 1.0 : 0.0;
        });
        interpreter.setHandler("IsPlayerTagSkill",
            [&chargen](CallContext& call) { return chargen.tagged(intArgument(call, 0)) ? 1.0 : 0.0; });

        interpreter.setHandler("SetInChargen", [&chargen](CallContext& call) {
            chargen.setInChargen(intArgument(call, 0) != 0);
            return 0.0;
        });
        interpreter.setHandler("GetInCharGen", [&chargen](CallContext&) { return chargen.inChargen() ? 1.0 : 0.0; });
    }
}

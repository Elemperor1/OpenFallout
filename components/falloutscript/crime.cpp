#include "crime.hpp"

#include <algorithm>
#include <cmath>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        /// The crimes that are held at most: the executables keep them in lists of the process lists until they are
        /// forgotten, which no rule of the executables bounds that has been read; the oldest go first
        constexpr std::size_t maxCrimes = 512;

        template <class T>
        bool contains(const std::vector<T>& list, const T& value)
        {
            return std::find(list.begin(), list.end(), value) != list.end();
        }

        void addUnique(std::vector<FormId>& list, FormId value)
        {
            if (value != 0 && !contains(list, value))
                list.push_back(value);
        }

        /// What a kill of a character that was killed in front of its own people costs or pays (0x89E32C to 0x89E3D7)
        double killKarma(const LawSettings& settings, int karmaClass, bool creature)
        {
            double value = creature ? settings.mKarmaMurderNonEvilCreature : settings.mKarmaMurderNonEvilNpc;
            switch (karmaClass)
            {
                case 0:
                    if (settings.mKarmaGrades)
                        value = settings.mKarmaMurderGoodNpc;
                    break;
                case 3:
                    if (settings.mKarmaGrades)
                        value = settings.mKarmaMurderVeryGoodNpc;
                    break;
                case 2:
                    value = settings.mKarmaKillingEvil;
                    break;
                case 4:
                    value = settings.mKarmaGrades ? settings.mKarmaKillingVeryEvil : settings.mKarmaKillingEvil;
                    break;
                default:
                    break;
            }
            return value;
        }

        int roundToInt(double value)
        {
            return static_cast<int>(std::trunc(value));
        }
    }

    LawSettings LawSettings::forGame(Game game)
    {
        LawSettings settings;
        if (game == Game::Fallout3)
        {
            // Fallout 3 has no reputations, no grades of good and evil among the victims of a murder, and charges for
            // the murder of anyone that is not evil (the settings of its executable)
            settings.mReputations = false;
            settings.mKarmaGrades = false;
            settings.mKarmaMurderNonEvilNpc = -100.0;
            settings.mKarmaMurderNonEvilCreature = -25.0;
        }
        return settings;
    }

    LawSettings LawSettings::fromGameSettings(Game game, const Lookup& lookup)
    {
        LawSettings settings = forGame(game);
        if (!lookup)
            return settings;
        const auto take = [&lookup](std::string_view name, double& setting) {
            if (const std::optional<double> value = lookup(name))
                setting = *value;
        };
        const auto takeCount = [&lookup](std::string_view name, int& setting) {
            if (const std::optional<double> value = lookup(name))
                setting = static_cast<int>(*value);
        };
        takeCount("iStealWarnings", settings.mStealWarnings);
        takeCount("iPickPocketWarnings", settings.mPickPocketWarnings);
        take("fAITrespassWarningTimer", settings.mTrespassWarningSeconds);
        take("fReputationMinorCrimeNeg", settings.mMinorCrimeInfamy);
        take("fReputationMajorCrimeNeg", settings.mMajorCrimeInfamy);
        takeCount("iKarmaMin", settings.mKarmaMin);
        takeCount("iKarmaMax", settings.mKarmaMax);
        takeCount("iKarmaChangeThreshold", settings.mKarmaChangeThreshold);
        take("fKarmaModStealing", settings.mKarmaStealing);
        take("fKarmaModMurderingNonEvilNPC", settings.mKarmaMurderNonEvilNpc);
        take("fKarmaModMurderingNonEvilCreature", settings.mKarmaMurderNonEvilCreature);
        take("fKarmaModMurderingGoodNPC", settings.mKarmaMurderGoodNpc);
        take("fKarmaModMurderingVeryGoodNPC", settings.mKarmaMurderVeryGoodNpc);
        take("fKarmaModKillingEvilActor", settings.mKarmaKillingEvil);
        take("fKarmaModKillingVeryEvilActor", settings.mKarmaKillingVeryEvil);
        take("iCrimeAlarmRecDistance", settings.mAlarmDistance);
        take("fCrimeDispAttack", settings.mCrimeDispAttack);
        return settings;
    }

    Law::Law(LawRecords& records, ObjectState& objects)
        : mRecords(records)
        , mObjects(objects)
    {
    }

    void Law::clear()
    {
        const std::lock_guard lock(mMutex);
        mFactions.clear();
        mActorMinor.clear();
        mPlayerMinor = 0;
        mPlayerMajor = 0;
        mValues = {};
        mStealCount = 0;
        mPickpocketCount = 0;
        mWarned = false;
        mWarnedDay = 0;
        mMurderer = false;
        mSoldStolen = 0;
        mCrimes.clear();
        mNextCrime = 1;
        mTrespass.clear();
        mLineVictim = 0;
    }

    bool Law::flag(FormId faction, FactionFlag which)
    {
        if (which == FactionFlag::PlayerEnemy)
            return mObjects.enemyOfFaction(faction);
        const std::lock_guard lock(mMutex);
        const auto found = mFactions.find(faction);
        if (found == mFactions.end())
            return false;
        switch (which)
        {
            case FactionFlag::PlayerExpelled:
                return found->second.mExpelled;
            case FactionFlag::PlayerAttack:
                return found->second.mAttack;
            case FactionFlag::PlayerMurder:
                return found->second.mMurder;
            default:
                return false;
        }
    }

    void Law::setFlag(FormId faction, FactionFlag which, bool on)
    {
        if (which == FactionFlag::PlayerEnemy)
        {
            mObjects.setEnemyOfFaction(faction, on);
            return;
        }
        if (faction == 0)
            return;
        const std::lock_guard lock(mMutex);
        FactionRecord& record = mFactions[faction];
        switch (which)
        {
            case FactionFlag::PlayerExpelled:
                record.mExpelled = on;
                break;
            case FactionFlag::PlayerAttack:
                record.mAttack = on;
                break;
            case FactionFlag::PlayerMurder:
                record.mMurder = on;
                break;
            default:
                break;
        }
    }

    void Law::clearEnemyFlags(const std::vector<FormId>& factions)
    {
        for (const FormId faction : factions)
            mObjects.setEnemyOfFaction(faction, false);
    }

    bool Law::anyEnemy(const std::vector<FormId>& factions)
    {
        return std::any_of(factions.begin(), factions.end(),
            [this](FormId faction) { return mObjects.enemyOfFaction(faction); });
    }

    int Law::minorCrimes(const std::vector<FormId>& factions)
    {
        const std::lock_guard lock(mMutex);
        int sum = 0;
        for (const FormId faction : factions)
            if (const auto found = mFactions.find(faction); found != mFactions.end())
                sum += found->second.mMinor;
        return sum;
    }

    int Law::majorCrimes(const std::vector<FormId>& factions)
    {
        const std::lock_guard lock(mMutex);
        int sum = 0;
        for (const FormId faction : factions)
            if (const auto found = mFactions.find(faction); found != mFactions.end())
                sum += found->second.mMajor;
        return sum;
    }

    int Law::playerMinorCrimes()
    {
        const std::lock_guard lock(mMutex);
        return mPlayerMinor;
    }

    int Law::playerMajorCrimes()
    {
        const std::lock_guard lock(mMutex);
        return mPlayerMajor;
    }

    int Law::actorMinorCrimes(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mActorMinor.find(actor);
        return found == mActorMinor.end() ? 0 : found->second;
    }

    bool Law::murderer()
    {
        const std::lock_guard lock(mMutex);
        return mMurderer;
    }

    void Law::setMurderer(bool on)
    {
        const std::lock_guard lock(mMutex);
        mMurderer = on;
    }

    std::int64_t Law::valueOf(CrimeKind kind)
    {
        const std::lock_guard lock(mMutex);
        return mValues[static_cast<std::size_t>(kind)];
    }

    double Law::amountSoldStolen()
    {
        const std::lock_guard lock(mMutex);
        return mSoldStolen;
    }

    void Law::modAmountSoldStolen(double change)
    {
        const std::lock_guard lock(mMutex);
        mSoldStolen += change;
    }

    bool Law::hasCrime(FormId victim, FormId criminal, int kind, FormId witness, int number)
    {
        const std::lock_guard lock(mMutex);
        return std::any_of(mCrimes.begin(), mCrimes.end(), [&](const Crime& crime) {
            return (victim == 0 || crime.mVictim == victim) && crime.mCriminal == criminal
                && (witness == 0 || contains(crime.mWitnesses, witness))
                && (kind < 0 || static_cast<int>(crime.mKind) == kind) && (number < 0 || crime.mNumber == number);
        });
    }

    std::size_t Law::crimeCount()
    {
        const std::lock_guard lock(mMutex);
        return mCrimes.size();
    }

    std::vector<FormId> Law::stopAlarm(FormId actor)
    {
        std::vector<FormId> cleared;
        {
            // (the flags are cleared under the same lock that new crimes are made under: a crime that was made after the
            // check would otherwise lose the flag it just set)
            const std::lock_guard lock(mMutex);
            std::vector<FormId> released;
            std::vector<Crime> kept;
            for (Crime& crime : mCrimes)
            {
                if (crime.mVictim == actor || contains(crime.mWitnesses, actor))
                {
                    for (const FormId faction : crime.mFactions)
                        addUnique(released, faction);
                }
                else
                    kept.push_back(std::move(crime));
            }
            mCrimes = std::move(kept);
            // A faction stays an enemy while a crime that is left holds it (0x9722E0)
            for (const FormId faction : released)
            {
                const bool held = std::any_of(mCrimes.begin(), mCrimes.end(),
                    [faction](const Crime& crime) { return contains(crime.mFactions, faction); });
                if (!held)
                    cleared.push_back(faction);
            }
            clearEnemyFlags(cleared);
        }
        return cleared;
    }

    std::size_t Law::record(CrimeKind kind, FormId victim, std::int64_t value)
    {
        if (mCrimes.size() >= maxCrimes)
        {
            // A faction that only the crime that goes held as an enemy is released with it: nothing could release it later
            const std::vector<FormId> held = mCrimes.front().mFactions;
            mCrimes.erase(mCrimes.begin());
            std::vector<FormId> released;
            for (const FormId faction : held)
                if (std::none_of(mCrimes.begin(), mCrimes.end(),
                        [faction](const Crime& crime) { return contains(crime.mFactions, faction); }))
                    released.push_back(faction);
            clearEnemyFlags(released);
        }
        Crime crime;
        crime.mNumber = mNextCrime++;
        crime.mKind = kind;
        crime.mVictim = victim;
        crime.mCriminal = mRecords.player();
        crime.mValue = value;
        mCrimes.push_back(std::move(crime));
        return mCrimes.size() - 1;
    }

    void Law::addWitness(std::size_t crime, FormId actor)
    {
        addUnique(mCrimes[crime].mWitnesses, actor);
    }

    bool Law::cares(const std::vector<FormId>& watcherFactions, const std::vector<FormId>& victimFactions,
        std::size_t crime, Outcome& outcome)
    {
        bool any = false;
        for (const FormId faction : watcherFactions)
        {
            if (!mRecords.tracksCrime(faction) || !contains(victimFactions, faction))
                continue;
            mObjects.setEnemyOfFaction(faction, true);
            addUnique(mCrimes[crime].mFactions, faction);
            addUnique(outcome.mEnemies, faction);
            any = true;
        }
        return any;
    }

    bool Law::anyTracking(const std::vector<FormId>& factions)
    {
        return std::any_of(
            factions.begin(), factions.end(), [this](FormId faction) { return mRecords.tracksCrime(faction); });
    }

    void Law::factionCrime(const std::vector<FormId>& victimFactions, bool major, bool infamy,
        const LawSettings& settings, Outcome& outcome)
    {
        for (const FormId faction : victimFactions)
        {
            if (!mRecords.tracksCrime(faction))
                continue;
            FactionRecord& record = mFactions[faction];
            if (major)
                ++record.mMajor;
            else
                ++record.mMinor;
            if (!infamy || !settings.mReputations)
                continue;
            if (const FormId reputation = mRecords.reputationOf(faction); reputation != 0)
                outcome.mInfamy.push_back({ reputation, major ? settings.mMajorCrimeInfamy : settings.mMinorCrimeInfamy });
        }
    }

    Outcome Law::attack(const Blow& blow, const std::vector<Witness>& watchers)
    {
        Outcome outcome;
        if (blow.mVictim == 0)
            return outcome;
        const LawSettings settings = mRecords.lawSettings();
        const std::lock_guard lock(mMutex);
        // A victim that fights already is no one that is wronged: it thinks less of the player and that is all
        if (blow.mFighting)
        {
            outcome.mVictimDisposition = settings.mCrimeDispAttack;
            return outcome;
        }
        const std::size_t crime = record(CrimeKind::Attack, blow.mVictim, 0);
        outcome.mCrime = mCrimes[crime].mNumber;
        mValues[static_cast<std::size_t>(CrimeKind::Attack)] += 1;
        addWitness(crime, blow.mVictim);
        for (const Witness& watcher : watchers)
        {
            if (!watcher.mWatching || watcher.mActor == 0)
                continue;
            // A weapon of the minor kind is the business of the one that was hit, and of nobody else
            const bool concerned = blow.mMinor ? watcher.mActor == blow.mVictim
                                               : cares(watcher.mFactions, blow.mVictimFactions, crime, outcome);
            if (concerned)
            {
                addWitness(crime, watcher.mActor);
                outcome.mReactions.push_back({ watcher.mActor, Reaction::Kind::Say, CrimeKind::Attack });
            }
            else if (watcher.mFriendOfVictim)
                outcome.mReactions.push_back({ watcher.mActor, Reaction::Kind::Attack, CrimeKind::Attack });
            else
                outcome.mReactions.push_back({ watcher.mActor, Reaction::Kind::SayNoCrime, CrimeKind::Attack });
        }
        // The victim is a witness, so the crime always counts
        if (blow.mMinor)
            factionCrime(blow.mVictimFactions, false, false, settings, outcome);
        else
        {
            ++mPlayerMajor;
            factionCrime(blow.mVictimFactions, true, false, settings, outcome);
        }
        return outcome;
    }

    Outcome Law::murder(const Killing& killing, const std::vector<Witness>& watchers)
    {
        Outcome outcome;
        if (killing.mVictim == 0)
            return outcome;
        const LawSettings settings = mRecords.lawSettings();
        const std::lock_guard lock(mMutex);
        // The karma of a kill goes by the class of the victim's karma, and only for a victim of a faction that tracks crime
        if (killing.mKarmaCounts && anyTracking(killing.mVictimFactions))
            outcome.mKarma = roundToInt(killKarma(settings, killing.mKarmaClass, !killing.mCharacter));
        if (!killing.mCrimeOnDeath)
            return outcome;
        // Whoever the player kills that is a character and not evil makes the player a murderer, seen or not
        if (killing.mCharacter && !killing.mEvil)
        {
            mMurderer = true;
            outcome.mMurderer = true;
        }
        const std::size_t crime = record(CrimeKind::Murder, killing.mVictim, 0);
        outcome.mCrime = mCrimes[crime].mNumber;
        mValues[static_cast<std::size_t>(CrimeKind::Murder)] += 1;
        for (const Witness& watcher : watchers)
        {
            if (!watcher.mWatching || watcher.mActor == 0 || watcher.mActor == killing.mVictim)
                continue;
            if (cares(watcher.mFactions, killing.mVictimFactions, crime, outcome))
            {
                addWitness(crime, watcher.mActor);
                outcome.mReactions.push_back({ watcher.mActor, Reaction::Kind::Say, CrimeKind::Murder });
            }
            else if (watcher.mFriendOfVictim)
                outcome.mReactions.push_back({ watcher.mActor, Reaction::Kind::Attack, CrimeKind::Murder });
            else
                outcome.mReactions.push_back({ watcher.mActor, Reaction::Kind::SayNoCrime, CrimeKind::Murder });
        }
        // The crime counts when somebody that cares saw it
        if (!mCrimes[crime].mWitnesses.empty())
        {
            ++mPlayerMajor;
            factionCrime(killing.mVictimFactions, true, true, settings, outcome);
        }
        return outcome;
    }

    void Law::ladder(CrimeKind kind, FormId victim, bool warnOnly, int day, Outcome& outcome)
    {
        // A second theft on the day of a warning is answered with blows; the first is answered with the warning
        // (or, once the player has used up the warnings, with the line of the crime)
        if (mWarned && mWarnedDay == day)
        {
            outcome.mReactions.push_back({ victim, Reaction::Kind::Attack, kind });
            return;
        }
        mWarned = true;
        mWarnedDay = day;
        outcome.mReactions.push_back({ victim, warnOnly ? Reaction::Kind::Warn : Reaction::Kind::Say, kind });
    }

    Outcome Law::steal(const Theft& theft)
    {
        Outcome outcome;
        const LawSettings settings = mRecords.lawSettings();
        const std::lock_guard lock(mMutex);
        mValues[static_cast<std::size_t>(CrimeKind::Steal)] += theft.mValue;
        // The karma goes whoever sees it, but only for what belonged to someone who is not evil
        if (theft.mOwnerNotEvil)
            outcome.mKarma = roundToInt(settings.mKarmaStealing);
        if (theft.mVictim == 0 || !theft.mVictimWatching)
            return outcome;
        const std::size_t crime = record(CrimeKind::Steal, theft.mVictim, theft.mValue);
        outcome.mCrime = mCrimes[crime].mNumber;
        ++mActorMinor[theft.mVictim];
        factionCrime(theft.mVictimFactions, false, true, settings, outcome);
        addWitness(crime, theft.mVictim);
        ladder(CrimeKind::Steal, theft.mVictim, mStealCount < settings.mStealWarnings, theft.mDay, outcome);
        return outcome;
    }

    Outcome Law::pickpocket(const Theft& theft)
    {
        Outcome outcome;
        if (theft.mVictim == 0)
            return outcome;
        const LawSettings settings = mRecords.lawSettings();
        const std::lock_guard lock(mMutex);
        mValues[static_cast<std::size_t>(CrimeKind::Pickpocket)] += theft.mValue;
        const std::size_t crime = record(CrimeKind::Pickpocket, theft.mVictim, theft.mValue);
        outcome.mCrime = mCrimes[crime].mNumber;
        ++mActorMinor[theft.mVictim];
        factionCrime(theft.mVictimFactions, false, true, settings, outcome);
        addWitness(crime, theft.mVictim);
        ladder(CrimeKind::Pickpocket, theft.mVictim, mPickpocketCount < settings.mPickPocketWarnings, theft.mDay,
            outcome);
        return outcome;
    }

    Outcome Law::alarm(const Alarm& alarm)
    {
        Outcome outcome;
        const LawSettings settings = mRecords.lawSettings();
        const std::lock_guard lock(mMutex);
        bool first = true;
        for (const Witness& responder : alarm.mResponders)
        {
            if (!responder.mWatching || responder.mActor == 0)
                continue;
            outcome.mReactions.push_back({ responder.mActor, Reaction::Kind::Attack, CrimeKind::Trespass });
            // The first one that answers marks a minor crime for its factions
            if (first)
                factionCrime(responder.mFactions, false, true, settings, outcome);
            first = false;
        }
        if (!first)
            ++mPlayerMinor;
        return outcome;
    }

    Outcome Law::assaultAlarm(
        FormId victim, const std::vector<FormId>& victimFactions, const std::vector<Witness>& watchers)
    {
        Blow blow;
        blow.mVictim = victim;
        blow.mVictimFactions = victimFactions;
        Outcome outcome = attack(blow, watchers);
        // The one that raised the alarm fights
        outcome.mReactions.insert(
            outcome.mReactions.begin(), Reaction{ victim, Reaction::Kind::Attack, CrimeKind::Attack });
        return outcome;
    }

    Law::TrespassStep Law::trespass(FormId watcher, double seconds, bool offLimits)
    {
        if (watcher == 0)
            return TrespassStep::Wait;
        const LawSettings settings = mRecords.lawSettings();
        const std::lock_guard lock(mMutex);
        const auto [found, fresh] = mTrespass.try_emplace(watcher);
        Trespass& state = found->second;
        // The package starts with a warning due at once, and the warnings follow at the time of the setting
        if (fresh)
            state.mTimer = settings.mTrespassWarningSeconds;
        if (state.mAlarmed)
            return TrespassStep::Wait;
        state.mTimer += std::max(seconds, 0.0);
        if (state.mTimer < settings.mTrespassWarningSeconds)
            return TrespassStep::Wait;
        state.mTimer = 0;
        // A cell that is off limits gives no warning at all
        const int warnings = offLimits ? 0 : settings.mTrespassWarnings;
        if (state.mLevel < warnings)
        {
            ++state.mLevel;
            return TrespassStep::Warn;
        }
        state.mAlarmed = true;
        return TrespassStep::Alarm;
    }

    void Law::endTrespass(FormId watcher)
    {
        const std::lock_guard lock(mMutex);
        mTrespass.erase(watcher);
    }

    int Law::trespassLevel(FormId watcher)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mTrespass.find(watcher);
        return found == mTrespass.end() ? -1 : found->second.mLevel;
    }

    bool Law::trespassing(FormId watcher)
    {
        const std::lock_guard lock(mMutex);
        return mTrespass.count(watcher) != 0;
    }

    bool Law::anyTrespass()
    {
        const std::lock_guard lock(mMutex);
        return !mTrespass.empty();
    }

    std::vector<FormId> Law::trespassWatchers()
    {
        const std::lock_guard lock(mMutex);
        std::vector<FormId> result;
        result.reserve(mTrespass.size());
        for (const auto& entry : mTrespass)
            result.push_back(entry.first);
        return result;
    }

    Law::KarmaChange Law::karma(const LawSettings& settings, int current, int change)
    {
        KarmaChange result;
        const long long target = std::clamp<long long>(
            static_cast<long long>(current) + change, settings.mKarmaMin, settings.mKarmaMax);
        result.mApplied = static_cast<int>(target - current);
        result.mMajor = std::abs(change) >= settings.mKarmaChangeThreshold;
        return result;
    }

    void Law::setLineVictim(FormId victim)
    {
        const std::lock_guard lock(mMutex);
        mLineVictim = victim;
    }

    FormId Law::lineVictim()
    {
        const std::lock_guard lock(mMutex);
        return mLineVictim;
    }

    bool Law::mayTake(FormId owner, FormId characterForm, const std::vector<FormId>& characterFactions)
    {
        return owner == 0 || owner == characterForm || contains(characterFactions, owner);
    }

    FormId Law::lineFor(CrimeKind kind, CrimeLine line)
    {
        // The topics of the default dialogue list that the executables of both games say for a crime (category 2)
        switch (line)
        {
            case CrimeLine::GuardTrespass:
                return 0xf2;
            case CrimeLine::Crime:
                switch (kind)
                {
                    case CrimeKind::Steal:
                        return 0xdf;
                    case CrimeKind::Pickpocket:
                        return 0xe3;
                    case CrimeKind::Attack:
                        return 0xe4;
                    case CrimeKind::Murder:
                        return 0xe5;
                    default:
                        return 0;
                }
            case CrimeLine::NoCrime:
                switch (kind)
                {
                    case CrimeKind::Steal:
                        return 0xea;
                    case CrimeKind::Pickpocket:
                        return 0xe9;
                    case CrimeKind::Trespass:
                        return 0xeb;
                    case CrimeKind::Attack:
                        return 0xe7;
                    case CrimeKind::Murder:
                        return 0xe8;
                }
                return 0;
        }
        return 0;
    }

    template <>
    struct SaveAccess<Law>
    {
        static void writeForms(SaveWriter& out, const std::vector<FormId>& forms)
        {
            out.count(forms.size());
            for (const FormId form : forms)
                out.form(form);
        }

        static std::vector<FormId> readForms(SaveReader& in)
        {
            std::vector<FormId> forms;
            const std::size_t count = in.count(4);
            for (std::size_t i = 0; i < count; ++i)
                if (const std::optional<FormId> form = in.form())
                    forms.push_back(*form);
            return forms;
        }

        static void write(Law& law, SaveWriter& out)
        {
            const std::lock_guard lock(law.mMutex);
            out.count(law.mFactions.size());
            for (const auto& [faction, record] : law.mFactions)
            {
                out.form(faction);
                out.boolean(record.mExpelled);
                out.boolean(record.mAttack);
                out.boolean(record.mMurder);
                out.i32(record.mMajor);
                out.i32(record.mMinor);
            }
            out.count(law.mActorMinor.size());
            for (const auto& [actor, count] : law.mActorMinor)
            {
                out.form(actor);
                out.i32(count);
            }
            out.i32(law.mPlayerMinor);
            out.i32(law.mPlayerMajor);
            for (const std::int64_t value : law.mValues)
                out.i64(value);
            out.i32(law.mStealCount);
            out.i32(law.mPickpocketCount);
            out.boolean(law.mWarned);
            out.i32(law.mWarnedDay);
            out.boolean(law.mMurderer);
            out.f64(law.mSoldStolen);
            out.i32(law.mNextCrime);
            out.count(law.mCrimes.size());
            for (const Law::Crime& crime : law.mCrimes)
            {
                out.i32(crime.mNumber);
                out.u8(static_cast<std::uint8_t>(crime.mKind));
                out.form(crime.mVictim);
                out.form(crime.mCriminal);
                out.i64(crime.mValue);
                writeForms(out, crime.mWitnesses);
                writeForms(out, crime.mFactions);
            }
            out.count(law.mTrespass.size());
            for (const auto& [watcher, state] : law.mTrespass)
            {
                out.form(watcher);
                out.i32(state.mLevel);
                out.f64(state.mTimer);
                out.boolean(state.mAlarmed);
            }
        }

        static void read(Law& law, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, Law::FactionRecord> factions;
            for (std::size_t i = 0, count = in.count(15); i < count; ++i)
            {
                const std::optional<FormId> faction = in.form();
                Law::FactionRecord record;
                record.mExpelled = in.boolean();
                record.mAttack = in.boolean();
                record.mMurder = in.boolean();
                record.mMajor = in.i32();
                record.mMinor = in.i32();
                if (faction)
                    factions[*faction] = record;
            }
            std::map<FormId, int> actorMinor;
            for (std::size_t i = 0, count = in.count(8); i < count; ++i)
            {
                const std::optional<FormId> actor = in.form();
                const int value = in.i32();
                if (actor)
                    actorMinor[*actor] = value;
            }
            const int playerMinor = in.i32();
            const int playerMajor = in.i32();
            std::array<std::int64_t, 5> values{};
            for (std::int64_t& value : values)
                value = in.i64();
            const int stealCount = in.i32();
            const int pickpocketCount = in.i32();
            const bool warned = in.boolean();
            const int warnedDay = in.i32();
            const bool murderer = in.boolean();
            const double soldStolen = in.f64();
            const int nextCrime = in.i32();
            std::vector<Law::Crime> crimes;
            for (std::size_t i = 0, count = in.count(29); i < count; ++i)
            {
                Law::Crime crime;
                crime.mNumber = in.i32();
                const std::uint8_t kind = in.u8();
                const std::optional<FormId> victim = in.form();
                const std::optional<FormId> criminal = in.form();
                crime.mValue = in.i64();
                crime.mWitnesses = readForms(in);
                crime.mFactions = readForms(in);
                // A crime against, or by, a form of a plugin that is gone is dropped
                if (kind > static_cast<std::uint8_t>(CrimeKind::Murder) || !victim || !criminal)
                    continue;
                crime.mKind = static_cast<CrimeKind>(kind);
                crime.mVictim = *victim;
                crime.mCriminal = *criminal;
                crimes.push_back(std::move(crime));
            }
            std::map<FormId, Law::Trespass> trespass;
            for (std::size_t i = 0, count = in.count(17); i < count; ++i)
            {
                const std::optional<FormId> watcher = in.form();
                Law::Trespass state;
                state.mLevel = in.i32();
                state.mTimer = in.f64();
                state.mAlarmed = in.boolean();
                if (watcher)
                    trespass[*watcher] = state;
            }
            const std::lock_guard lock(law.mMutex);
            law.mFactions = std::move(factions);
            law.mActorMinor = std::move(actorMinor);
            law.mPlayerMinor = playerMinor;
            law.mPlayerMajor = playerMajor;
            law.mValues = values;
            law.mStealCount = stealCount;
            law.mPickpocketCount = pickpocketCount;
            law.mWarned = warned;
            law.mWarnedDay = warnedDay;
            law.mMurderer = murderer;
            law.mSoldStolen = soldStolen;
            law.mCrimes = std::move(crimes);
            law.mNextCrime = std::max(nextCrime, 1);
            law.mTrespass = std::move(trespass);
        }
    };

    void writeLaw(Law& law, SaveWriter& out)
    {
        SaveAccess<Law>::write(law, out);
    }

    void readLaw(Law& law, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<Law>::read(law, in, version);
    }

    void addLawCommands(
        Interpreter& interpreter, Law& law, ObjectState& objects, LawRecords& records, LawWorld& world)
    {
        const auto isPlayer = [&records](FormId reference) { return reference != 0 && reference == records.player(); };
        interpreter.setHandler("GetMinorCrimeCount", [&law, &world, isPlayer](CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            return static_cast<double>(isPlayer(call.mReference) ? law.playerMinorCrimes()
                                                                  : law.minorCrimes(world.factionsOf(call.mReference)));
        });
        interpreter.setHandler("GetMajorCrimeCount", [&law, &world, isPlayer](CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            return static_cast<double>(isPlayer(call.mReference) ? law.playerMajorCrimes()
                                                                  : law.majorCrimes(world.factionsOf(call.mReference)));
        });
        // The one that is asked is the victim and a witness; the criminal is the character that the script names; without
        // a kind any kind counts
        interpreter.setHandler("GetCrime", [&law](CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            const int kind = call.mArguments.size() > 1 ? intArgument(call, 1) : -1;
            return law.hasCrime(call.mReference, formArgument(call, 0), kind, call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetCrimeKnown", [&law](CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            return law.hasCrime(formArgument(call, 2), formArgument(call, 1), intArgument(call, 0), call.mReference)
                ? 1.0
                : 0.0;
        });
        interpreter.setHandler("IsGuard", [&world](CallContext& call) {
            return call.mReference != 0 && world.isGuard(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("IsPCAMurderer", [&law](CallContext&) { return law.murderer() ? 1.0 : 0.0; });
        const auto flagCommands = [&](std::string_view get, std::string_view set, FactionFlag which) {
            interpreter.setHandler(get, [&law, which](CallContext& call) {
                const FormId faction = formArgument(call, 0);
                return faction != 0 && law.flag(faction, which) ? 1.0 : 0.0;
            });
            interpreter.setHandler(set, [&law, which](CallContext& call) {
                if (const FormId faction = formArgument(call, 0); faction != 0)
                    law.setFlag(faction, which, argument(call, 1) > 0);
                return 0.0;
            });
        };
        flagCommands("GetPCExpelled", "SetPCExpelled", FactionFlag::PlayerExpelled);
        flagCommands("GetPCFactionAttack", "SetPCFactionAttack", FactionFlag::PlayerAttack);
        flagCommands("GetPCFactionMurder", "SetPCFactionMurder", FactionFlag::PlayerMurder);
        interpreter.setHandler("GetAlarmed", [&world](CallContext& call) {
            return call.mReference != 0 && world.alarmed(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetDetectionLevel", [&world](CallContext& call) {
            const FormId target = formArgument(call, 0);
            return call.mReference != 0 && target != 0
                ? static_cast<double>(world.detectionLevel(call.mReference, target))
                : 0.0;
        });
        interpreter.setHandler("IsActorDetected", [&world](CallContext& call) {
            return call.mReference != 0 && world.detected(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetAmountSoldStolen", [&law](CallContext&) { return law.amountSoldStolen(); });
        interpreter.setHandler("ModAmountSoldStolen", [&law](CallContext& call) {
            law.modAmountSoldStolen(argument(call, 0));
            return 0.0;
        });
        // The alarm of an assault: the crime of attack against the character that calls (or the one named), which that
        // character answers by fighting
        interpreter.setHandler("SendAssaultAlarm", [&law, &world](CallContext& call) {
            const FormId victim = call.mArguments.empty() || formArgument(call, 0) == 0 ? call.mReference
                                                                                          : formArgument(call, 0);
            if (victim == 0)
                return 0.0;
            const Outcome outcome = law.assaultAlarm(victim, world.factionsOf(victim), world.watchers(victim));
            world.carryOut(outcome, victim);
            return 0.0;
        });
        interpreter.setHandler("SendTrespassAlarm", [&law, &world](CallContext& call) {
            const FormId subject = formArgument(call, 0);
            Law::Alarm alarm;
            alarm.mResponders = world.responders(call.mReference, subject);
            const Outcome outcome = law.alarm(alarm);
            world.carryOut(outcome, call.mReference);
            return 0.0;
        });
        interpreter.setHandler("ClearFactionPlayerEnemyFlag", [&law](CallContext& call) {
            if (const FormId faction = formArgument(call, 0); faction != 0)
                law.setFlag(faction, FactionFlag::PlayerEnemy, false);
            return 0.0;
        });
        interpreter.setHandler("ClearActorsFactionsPlayerEnemyFlag", [&law, &world](CallContext& call) {
            if (call.mReference != 0)
                law.clearEnemyFlags(world.factionsOf(call.mReference));
            return 0.0;
        });
        // Whether a faction of the character is an enemy of the player. The command that asks about the crimes of the
        // character holds the same flags in the executables' extra data; here the factions' flags answer both.
        const auto enemyOfPlayer = [&law, &world](CallContext& call) {
            return call.mReference != 0 && law.anyEnemy(world.factionsOf(call.mReference)) ? 1.0 : 0.0;
        };
        interpreter.setHandler("GetActorFactionPlayerEnemy", enemyOfPlayer);
        interpreter.setHandler("GetActorCrimePlayerEnemy", enemyOfPlayer);
        interpreter.setHandler("GetTrespassWarningLevel", [&law](CallContext& call) {
            return call.mReference != 0 ? static_cast<double>(law.trespassLevel(call.mReference)) : -1.0;
        });
        interpreter.setHandler("IsTrespassing", [&world](CallContext& call) {
            return call.mReference != 0 && world.trespassingNow(call.mReference) ? 1.0 : 0.0;
        });
        // Whether the cell the character is in is the character's own (0x546CA0): its owner is the character or a faction
        // that it is in
        interpreter.setHandler("IsInMyOwnedCell", [&objects, &world](CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            const FormId cell = world.cellOf(call.mReference);
            const FormId owner = cell != 0 ? objects.cellOwner(cell) : 0;
            return owner != 0 && Law::mayTake(owner, world.ownerForm(call.mReference), world.factionsOf(call.mReference))
                ? 1.0
                : 0.0;
        });
        // The owner of the cell is the character or faction named (0x546A40 and the form compared at 0x5A2ED0)
        interpreter.setHandler("IsCellOwner", [&objects, &world](CallContext& call) {
            FormId cell = formArgument(call, 0);
            if (cell == 0 && call.mReference != 0)
                cell = world.cellOf(call.mReference);
            const FormId owner = call.mArguments.size() > 1 ? formArgument(call, 1)
                                                            : world.ownerForm(call.mReference);
            return cell != 0 && owner != 0 && objects.cellOwner(cell) == owner ? 1.0 : 0.0;
        });
        // The character is the victim of the crime whose line is being said (0x5A3460)
        interpreter.setHandler("IsActorAVictim", [&law, &world](CallContext& call) {
            const FormId victim = law.lineVictim();
            return call.mReference != 0 && victim != 0
                    && (call.mReference == victim || world.ownerForm(call.mReference) == victim)
                ? 1.0
                : 0.0;
        });
        interpreter.setHandler("ClearOwnership", [&objects](CallContext& call) {
            if (call.mReference != 0)
                objects.clearOwner(call.mReference);
            return 0.0;
        });
    }
}

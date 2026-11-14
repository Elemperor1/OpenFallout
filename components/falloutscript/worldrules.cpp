#include "worldrules.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace FalloutScript::World
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;

        // The signatures of the form types in the order of the executable's table (FalloutNV.exe 0x1187004)
        constexpr std::array<const char*, 0x79> signatures{ "NONE", "TES4", "GRUP", "GMST", "TXST", "MICN", "GLOB", "CLAS",
            "FACT", "HDPT", "HAIR", "EYES", "RACE", "SOUN", "ASPC", "SKIL", "MGEF", "SCPT", "LTEX", "ENCH", "SPEL", "ACTI",
            "TACT", "TERM", "ARMO", "BOOK", "CLOT", "CONT", "DOOR", "INGR", "LIGH", "MISC", "STAT", "SCOL", "MSTT", "PWAT",
            "GRAS", "TREE", "FLOR", "FURN", "WEAP", "AMMO", "NPC_", "CREA", "LVLC", "LVLN", "KEYM", "ALCH", "IDLM", "NOTE",
            "COBJ", "PROJ", "LVLI", "WTHR", "CLMT", "REGN", "NAVI", "CELL", "REFR", "ACHR", "ACRE", "PMIS", "PGRE", "PBEA",
            "PFLA", "WRLD", "LAND", "NAVM", "TLOD", "DIAL", "INFO", "QUST", "IDLE", "PACK", "CSTY", "LSCR", "LVSP", "ANIO",
            "WATR", "EFSH", "TOFT", "EXPL", "DEBR", "IMGS", "IMAD", "FLST", "PERK", "BPTD", "ADDN", "AVIF", "RADS", "CAMS",
            "CPTH", "VTYP", "IPCT", "IPDS", "ARMA", "ECZN", "MESG", "RGDL", "DOBJ", "LGTM", "MUSC", "IMOD", "REPU", "PCBE",
            "RCPE", "RCCT", "CHIP", "CSNO", "LSCT", "MSET", "ALOC", "CHAL", "AMEF", "CCRD", "CMNY", "CDCK", "DEHY", "HUNG",
            "SLPD" };

        char lower(char c)
        {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        bool equalsIgnoringCase(std::string_view a, std::string_view b)
        {
            if (a.size() != b.size())
                return false;
            for (std::size_t i = 0; i < a.size(); ++i)
                if (lower(a[i]) != lower(b[i]))
                    return false;
            return true;
        }

        double lerp(double low, double high, double share)
        {
            return low + (high - low) * share;
        }
    }

    std::string_view formTypeSignature(int type)
    {
        if (type < 0 || static_cast<std::size_t>(type) >= signatures.size())
            return {};
        return signatures[static_cast<std::size_t>(type)];
    }

    int formTypeNumber(std::string_view signature)
    {
        for (std::size_t i = 0; i < signatures.size(); ++i)
            if (equalsIgnoringCase(signature, signatures[i]))
                return static_cast<int>(i);
        return -1;
    }

    std::string signatureOfRecordType(std::uint32_t recordType)
    {
        // The engine marks the records of the games with one bit that no letter has (ESM::sEsm4RecnameFlag, in the third
        // letter)
        constexpr std::uint32_t recordFlag = 0x00800000;
        recordType &= ~recordFlag;
        std::string result;
        for (int shift = 0; shift < 32; shift += 8)
            result.push_back(static_cast<char>((recordType >> shift) & 0xFF));
        return result;
    }

    bool cellNameMatches(std::string_view cellName, std::string_view askedName)
    {
        // _strnicmp(cellName, askedName, strlen(askedName)) == 0
        if (cellName.size() < askedName.size())
            return false;
        return equalsIgnoringCase(cellName.substr(0, askedName.size()), askedName);
    }

    bool isCurrentWeather(const SkyView& sky, FormId weather)
    {
        return weather != 0 && sky.mCurrent.mPresent && sky.mCurrent.mForm == weather;
    }

    double weatherPercent(const SkyView& sky)
    {
        return sky.mCurrent.mPresent ? sky.mPercent : 0.0;
    }

    double classificationShare(const SkyView& sky, std::uint8_t bit)
    {
        double result = 0;
        if (sky.mCurrent.mPresent && (sky.mCurrent.mClassification & bit) != 0)
            result += weatherPercent(sky);
        if (sky.mOutgoing.mPresent && (sky.mOutgoing.mClassification & bit) != 0)
            result += 1.0 - weatherPercent(sky);
        return result;
    }

    bool isSnowing(const SkyView& sky)
    {
        constexpr std::uint8_t snow = 0x08;
        const double percent = weatherPercent(sky);
        if (sky.mCurrent.mPresent && (sky.mCurrent.mClassification & snow) != 0
            && percent > sky.mCurrent.mPrecipitationBeginFadeIn / 255.0 * 0.999)
            return true;
        return sky.mOutgoing.mPresent && (sky.mOutgoing.mClassification & snow) != 0
            && percent < 0.001 + sky.mOutgoing.mPrecipitationEndFadeOut / 255.0 * 0.999;
    }

    double windSpeed(const SkyView& sky)
    {
        if (!sky.mCurrent.mPresent)
            return 0.0;
        double wind = sky.mCurrent.mWindSpeed / 255.0;
        if (sky.mOutgoing.mPresent)
        {
            const double percent = weatherPercent(sky);
            wind = wind * percent + sky.mOutgoing.mWindSpeed / 255.0 * (1.0 - percent);
        }
        return wind;
    }

    namespace
    {
        bool inCone(const SightFacts& facts)
        {
            const double dx = facts.mTarget[0] - facts.mEye[0];
            const double dy = facts.mTarget[1] - facts.mEye[1];
            // The viewer's own place is the one the bearing is taken from, which is the eye's x and y
            const double bearing = std::atan2(dx, dy);
            const double apart = std::abs(std::remainder(bearing - facts.mFacing, 2 * pi));
            // Half of the cone on each side, and strictly inside (FalloutNV.exe 0x88C570)
            return apart < facts.mViewCone * pi / 360.0;
        }

        bool anyRay(const SightFacts& facts, const SightRay& ray, std::size_t count)
        {
            for (std::size_t i = 0; i < count && i < sightFractions.size(); ++i)
            {
                Vec3 to = facts.mTarget;
                to[2] += facts.mTargetHeight * sightFractions[i];
                if (ray(facts.mEye, to))
                    return true;
            }
            return false;
        }
    }

    bool seesTarget(const SightFacts& facts, const SightRay& ray)
    {
        const bool near = facts.mSameSpace && facts.mDistance <= sightNearDistance;
        if (facts.mViewerIsPlayer)
        {
            // Only what the camera shows can be seen by the player; nothing is cast for anything else
            if (!facts.mInFrustum)
                return false;
            // (a target that is not loaded, or is in another space, is no use to a ray: its place means nothing here)
            if (facts.mTargetLoaded && facts.mSameSpace && anyRay(facts, ray, sightFractions.size()))
                return true;
            // What is left is the test of an actor, which for the player holds no sightings but the near distance
            return near;
        }
        if (!facts.mTargetLoaded || facts.mTargetDead)
            return false;
        if (near)
            return true;
        if (!facts.mSameSpace || !inCone(facts))
            return false;
        if (facts.mAllVisible)
            return true;
        const std::size_t rays = facts.mTargetIsPlayer || facts.mTargetIsViewerTarget ? sightFractions.size() : 1;
        return anyRay(facts, ray, rays);
    }

    double depthInBox(const Vec3& from, const Vec3& to, const Vec3& low, const Vec3& high)
    {
        double length = 0;
        for (std::size_t i = 0; i < 3; ++i)
            length += (to[i] - from[i]) * (to[i] - from[i]);
        length = std::sqrt(length);
        if (length <= 0)
            return 0;
        double depth = length;
        for (std::size_t i = 0; i < 3; ++i)
        {
            if (to[i] < low[i] || to[i] > high[i])
                return 0;
            const double direction = (to[i] - from[i]) / length;
            if (std::abs(direction) < 1e-9)
                continue;
            // Going back along the ray, the coordinate falls when the ray rises, and so the low side is the one reached
            const double side = direction > 0 ? low[i] : high[i];
            depth = std::min(depth, (to[i] - side) / direction);
        }
        return std::max(depth, 0.0);
    }

    void PlayerActions::clear()
    {
        const std::lock_guard lock(mMutex);
        mRecords.clear();
    }

    void PlayerActions::raise(int id, double seconds, FormId reference)
    {
        const std::lock_guard lock(mMutex);
        mRecords.erase(std::remove_if(mRecords.begin(), mRecords.end(), [id](const Record& r) { return r.mId == id; }),
            mRecords.end());
        mRecords.insert(mRecords.begin(), Record{ id, seconds, reference });
    }

    void PlayerActions::remove(int id)
    {
        const std::lock_guard lock(mMutex);
        mRecords.erase(std::remove_if(mRecords.begin(), mRecords.end(), [id](const Record& r) { return r.mId == id; }),
            mRecords.end());
    }

    void PlayerActions::update(double seconds)
    {
        const std::lock_guard lock(mMutex);
        for (Record& record : mRecords)
            record.mSeconds -= seconds;
        mRecords.erase(std::remove_if(mRecords.begin(), mRecords.end(),
                           [](const Record& r) { return r.mSeconds <= 0 && !persistent(r.mId); }),
            mRecords.end());
    }

    bool PlayerActions::active(int id) const
    {
        const std::lock_guard lock(mMutex);
        return std::any_of(mRecords.begin(), mRecords.end(), [id](const Record& r) { return r.mId == id; });
    }

    int PlayerActions::last() const
    {
        const std::lock_guard lock(mMutex);
        return mRecords.empty() ? 0 : mRecords.front().mId;
    }

    ScreenSplatter::ScreenSplatter(SplatterSettings settings)
        : mSettings(settings)
        , mFade(settings.mFadeStart)
    {
    }

    void ScreenSplatter::setSettings(const SplatterSettings& settings)
    {
        const std::lock_guard lock(mMutex);
        mSettings = settings;
    }

    SplatterSettings ScreenSplatter::settings() const
    {
        const std::lock_guard lock(mMutex);
        return mSettings;
    }

    void ScreenSplatter::reset()
    {
        const std::lock_guard lock(mMutex);
        mGroups.clear();
        mFade = mSettings.mFadeStart;
        mFadeMode = 0;
        mTextures = mDefaultTextures;
    }

    void ScreenSplatter::setDefaultTextures(const std::array<std::string, 3>& textures)
    {
        const std::lock_guard lock(mMutex);
        mDefaultTextures = textures;
        mTextures = textures;
    }

    namespace
    {
        ScreenSplatter::Group makeGroup(const SplatterSettings& settings, bool custom, int count, double duration,
            double sizeMultiplier, double opacityMultiplier, const Unit& random)
        {
            ScreenSplatter::Group group;
            group.mCustom = custom;
            group.mDuration = duration > 0 ? duration : settings.mDuration;
            for (int i = 0; i < count; ++i)
            {
                ScreenSplatter::Quad quad;
                quad.mX = random() - 0.5;
                quad.mY = random() - 0.5;
                quad.mHalfSize = lerp(settings.mMinSize, settings.mMaxSize, random()) * sizeMultiplier;
                const bool second = random() < settings.mOpacityChance;
                quad.mOpacity = (second ? lerp(settings.mMinOpacity2, settings.mMaxOpacity2, random())
                                        : lerp(settings.mMinOpacity, settings.mMaxOpacity, random()))
                    * opacityMultiplier;
                group.mQuads.push_back(quad);
            }
            return group;
        }
    }

    void ScreenSplatter::spawn(Group group)
    {
        // The textures that are set now are the ones of this group, whatever is set after it
        group.mTextures = mTextures;
        mGroups.push_back(std::move(group));
    }

    bool ScreenSplatter::triggerCustom(
        int count, double duration, double sizeMultiplier, double opacityMultiplier, const Unit& random)
    {
        const std::lock_guard lock(mMutex);
        if (!mSettings.mCustomEnabled || count <= 0)
            return false;
        spawn(makeGroup(mSettings, true, count, duration, sizeMultiplier, opacityMultiplier, random));
        return true;
    }

    int ScreenSplatter::triggerBlood(int count, const Unit& random)
    {
        const std::lock_guard lock(mMutex);
        if (!mSettings.mBloodEnabled)
            return 0;
        int bloody = 0;
        for (const Group& group : mGroups)
            if (!group.mCustom)
                bloody += static_cast<int>(group.mQuads.size());
        const int made = std::min(count, mSettings.mMaxCount - bloody);
        if (made <= 0)
            return 0;
        spawn(makeGroup(mSettings, false, made, 0.0, 1.0, 1.0, random));
        return made;
    }

    void ScreenSplatter::setFade(double fade, int mode)
    {
        const std::lock_guard lock(mMutex);
        mFade = fade;
        mFadeMode = mode;
    }

    bool ScreenSplatter::setTexture(int slot, std::string_view path)
    {
        const std::lock_guard lock(mMutex);
        if (slot < 0 || slot > 2)
            return false;
        mTextures[static_cast<std::size_t>(slot)] = std::string(path);
        return true;
    }

    std::string ScreenSplatter::texture(int slot) const
    {
        const std::lock_guard lock(mMutex);
        return slot >= 0 && slot <= 2 ? mTextures[static_cast<std::size_t>(slot)] : std::string();
    }

    void ScreenSplatter::clear()
    {
        const std::lock_guard lock(mMutex);
        mGroups.clear();
        mFade = 0.0;
        mFadeMode = 0;
        mTextures = mDefaultTextures;
    }

    void ScreenSplatter::update(double seconds)
    {
        const std::lock_guard lock(mMutex);
        for (Group& group : mGroups)
        {
            group.mElapsed += seconds;
            double from = 0;
            bool fading = false;
            if (mFadeMode != 0)
            {
                from = group.mDuration - mFade;
                fading = group.mElapsed >= from;
            }
            else if (group.mDuration > 0 && group.mElapsed / group.mDuration >= mFade)
            {
                from = mSettings.mFadeStart * group.mDuration;
                fading = true;
            }
            if (fading && group.mDuration > from)
                group.mAlpha = 1.0 - (group.mElapsed - from) / (group.mDuration - from);
        }
        mGroups.erase(std::remove_if(mGroups.begin(), mGroups.end(),
                          [](const Group& group) { return group.mElapsed >= group.mDuration; }),
            mGroups.end());
    }

    std::vector<ScreenSplatter::Group> ScreenSplatter::groups() const
    {
        const std::lock_guard lock(mMutex);
        return mGroups;
    }

    int ScreenSplatter::quadCount() const
    {
        const std::lock_guard lock(mMutex);
        int total = 0;
        for (const Group& group : mGroups)
            total += static_cast<int>(group.mQuads.size());
        return total;
    }

    double ScreenSplatter::fade() const
    {
        const std::lock_guard lock(mMutex);
        return mFade;
    }

    int ScreenSplatter::fadeMode() const
    {
        const std::lock_guard lock(mMutex);
        return mFadeMode;
    }

    void HudShudder::start(double durationMs, double amplitude, double frequency)
    {
        mActive = true;
        mTime = 0;
        mDuration = durationMs;
        mAmplitude = amplitude;
        mFrequency = frequency;
        mOffset = { 0, 0 };
    }

    void HudShudder::update(double milliseconds)
    {
        if (!mActive)
            return;
        mTime += milliseconds;
        if (mTime >= mDuration)
        {
            mActive = false;
            mOffset = { 0, 0 };
            return;
        }
        mOffset = { std::sin(mTime * mFrequency) * mAmplitude * (1.0 - mTime / mDuration), 0.0 };
    }

    void StartingPoses::clear()
    {
        const std::lock_guard lock(mMutex);
        mPoses.clear();
    }

    std::optional<Pose> StartingPoses::poseOf(
        FormId reference, const std::optional<Pose>& recorded, const std::function<std::optional<Pose>()>& current)
    {
        const std::lock_guard lock(mMutex);
        if (const auto found = mPoses.find(reference); found != mPoses.end())
            return found->second;
        std::optional<Pose> pose = recorded;
        if (!pose && current)
            pose = current();
        if (pose)
            mPoses.emplace(reference, *pose);
        return pose;
    }

    void StartingPoses::setToNow(FormId reference, const Pose& now)
    {
        const std::lock_guard lock(mMutex);
        mPoses[reference] = now;
    }

    double degreesOf(double radians)
    {
        return radians * 57.2957763671875;
    }

    std::uint8_t controlMask(const std::array<bool, 7>& chosen)
    {
        std::uint8_t mask = 0;
        for (std::size_t i = 0; i < chosen.size(); ++i)
            if (chosen[i])
                mask |= controlBits[i];
        return mask;
    }

    bool isWin32(bool padConnected, bool padDisabled)
    {
        return !(padConnected && !padDisabled);
    }

    std::optional<int> iniInteger(std::string_view text)
    {
        std::size_t at = 0;
        while (at < text.size() && (text[at] == ' ' || text[at] == '\t'))
            ++at;
        bool negative = false;
        if (at < text.size() && (text[at] == '-' || text[at] == '+'))
            negative = text[at++] == '-';
        if (at >= text.size() || text[at] < '0' || text[at] > '9')
            return std::nullopt;
        long long value = 0;
        for (; at < text.size() && text[at] >= '0' && text[at] <= '9'; ++at)
            if (value < 0x7fffffffLL)
                value = value * 10 + (text[at] - '0');
        if (value > 0x7fffffffLL)
            value = 0x7fffffffLL;
        return static_cast<int>(negative ? -value : value);
    }

    bool goreDisabledBy(std::optional<int> setting)
    {
        return setting.has_value() && *setting == 1;
    }

    std::string securitronTexture(std::string_view first, std::string_view second)
    {
        std::string result = "Textures\\creatures\\Securitron\\";
        result += first;
        result += '_';
        result += second;
        result += ".dds";
        return result;
    }

    std::string_view securitronScreenShape()
    {
        return "Screen01:0";
    }

    bool packageTimeIsUp(double durationHundredths, double hoursNow, double hoursStarted)
    {
        return durationHundredths != 0 && std::abs(hoursNow - hoursStarted) * 100.0 >= durationHundredths;
    }

    double modScaleResult(double current, double added)
    {
        const double wanted = std::clamp(current + added, 0.5, 2.0);
        const double rounded = std::round(wanted * 100.0) / 100.0;
        return std::clamp(rounded, 0.01, 10.0);
    }

    double actorSizeResult(double size)
    {
        constexpr double controllerDefault = 2.0;
        constexpr double controllerMaximum = 10.0;
        size = std::min(size, controllerMaximum * 0.5);
        return size <= 0 ? controllerDefault : size;
    }

    std::string codeKey(std::string_view code)
    {
        std::string result(code);
        std::transform(result.begin(), result.end(), result.begin(), lower);
        return result;
    }
}

#include "hud.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::Hud
{
    float secondsFor(const std::string& text)
    {
        // A line is read at about 15 characters a second; what is short keeps the time of the karma messages
        const float read = static_cast<float>(text.size()) / 15.f;
        return std::clamp(read, messageSeconds, 8.f);
    }

    void MessageLine::push(std::string text, std::string icon, float seconds)
    {
        if (text.empty())
            return;
        Message message;
        message.mSeconds = seconds > 0 ? seconds : secondsFor(text);
        message.mText = std::move(text);
        message.mIcon = std::move(icon);
        const std::lock_guard lock(mMutex);
        mMessages.push_back(std::move(message));
        while (mMessages.size() > capacity)
            mMessages.pop_front();
    }

    std::vector<Message> MessageLine::take()
    {
        const std::lock_guard lock(mMutex);
        std::vector<Message> result(mMessages.begin(), mMessages.end());
        mMessages.clear();
        return result;
    }

    std::size_t MessageLine::size()
    {
        const std::lock_guard lock(mMutex);
        return mMessages.size();
    }

    void MessageLine::clear()
    {
        const std::lock_guard lock(mMutex);
        mMessages.clear();
    }

    void SubtitleLine::push(Subtitle line)
    {
        if (line.mText.empty())
            return;
        const std::lock_guard lock(mMutex);
        mLines.push_back(std::move(line));
        while (mLines.size() > capacity)
            mLines.pop_front();
    }

    std::vector<Subtitle> SubtitleLine::take()
    {
        const std::lock_guard lock(mMutex);
        std::vector<Subtitle> result(mLines.begin(), mLines.end());
        mLines.clear();
        return result;
    }

    std::size_t SubtitleLine::size()
    {
        const std::lock_guard lock(mMutex);
        return mLines.size();
    }

    void SubtitleLine::clear()
    {
        const std::lock_guard lock(mMutex);
        mLines.clear();
    }

    void QuestTextLine::push(QuestText text)
    {
        const std::lock_guard lock(mMutex);
        mTexts.push_back(std::move(text));
        while (mTexts.size() > capacity)
            mTexts.pop_front();
    }

    void QuestTextLine::push(ObjectiveLine line)
    {
        if (line.mText.empty())
            return;
        const std::lock_guard lock(mMutex);
        mObjectives.push_back(std::move(line));
        while (mObjectives.size() > capacity)
            mObjectives.pop_front();
    }

    std::vector<QuestText> QuestTextLine::takeTexts()
    {
        const std::lock_guard lock(mMutex);
        std::vector<QuestText> result(mTexts.begin(), mTexts.end());
        mTexts.clear();
        return result;
    }

    std::vector<ObjectiveLine> QuestTextLine::takeObjectives()
    {
        const std::lock_guard lock(mMutex);
        std::vector<ObjectiveLine> result(mObjectives.begin(), mObjectives.end());
        mObjectives.clear();
        return result;
    }

    void QuestTextLine::clear()
    {
        const std::lock_guard lock(mMutex);
        mTexts.clear();
        mObjectives.clear();
    }

    void Watchers::note(FormId watcher, Detection::Sighting sighting, bool hostile, double now)
    {
        if (watcher == 0)
            return;
        const std::lock_guard lock(mMutex);
        mLooks[watcher] = Look{ sighting, hostile, now };
    }

    void Watchers::forget(FormId watcher)
    {
        const std::lock_guard lock(mMutex);
        mLooks.erase(watcher);
    }

    void Watchers::clear()
    {
        const std::lock_guard lock(mMutex);
        mLooks.clear();
    }

    Sneak Watchers::status(double now, double lasting)
    {
        using Detection::Sighting;
        const std::lock_guard lock(mMutex);
        Sneak result = Sneak::Hidden;
        for (auto it = mLooks.begin(); it != mLooks.end();)
        {
            const Look& look = it->second;
            if (now - look.mTime > lasting)
            {
                it = mLooks.erase(it);
                continue;
            }
            if (look.mSighting == Sighting::Seen)
                result = std::max(result, look.mHostile ? Sneak::Danger : Sneak::Detected);
            else if (look.mSighting == Sighting::Noticed)
                result = std::max(result, Sneak::Caution);
            ++it;
        }
        return result;
    }

    int Watchers::seen(double now, double lasting)
    {
        const std::lock_guard lock(mMutex);
        int count = 0;
        for (const auto& [watcher, look] : mLooks)
            if (now - look.mTime <= lasting && look.mSighting == Detection::Sighting::Seen)
                ++count;
        return count;
    }

    double bearing(double dx, double dy)
    {
        constexpr double pi = 3.14159265358979323846;
        double degrees = std::atan2(dx, dy) * 180.0 / pi;
        if (degrees < 0)
            degrees += 360.0;
        // -0.0 and values that round up to a full turn
        return degrees >= 360.0 ? 0.0 : degrees;
    }

    std::optional<double> compassShare(double heading, double bearingOfThing, double field)
    {
        if (!(field > 0) || !std::isfinite(heading) || !std::isfinite(bearingOfThing))
            return std::nullopt;
        // The angle to the right of the way the player faces, from -180 to 180
        double across = std::fmod(bearingOfThing - heading, 360.0);
        if (across > 180.0)
            across -= 360.0;
        else if (across < -180.0)
            across += 360.0;
        if (std::abs(across) > field / 2.0)
            return std::nullopt;
        return 0.5 + across / field;
    }

    double experienceShare(int experience, int levelStart, int nextLevel)
    {
        if (nextLevel <= levelStart)
            return 1.0;
        return std::clamp(static_cast<double>(experience - levelStart) / (nextLevel - levelStart), 0.0, 1.0);
    }

    std::optional<std::string> limbIcon(int partType)
    {
        switch (partType)
        {
            case 0:
                return "torso";
            case 1:
            case 2:
                return "head";
            case 3:
            case 4:
                return "left_arm";
            case 5:
            case 6:
                return "right_arm";
            case 7:
            case 8:
            case 9:
                return "left_leg";
            case 10:
            case 11:
            case 12:
                return "right_leg";
            case 13:
                return "face_00";
            default:
                return std::nullopt;
        }
    }
}

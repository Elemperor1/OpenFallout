#include "loadscreens.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <set>

namespace FalloutScript::LoadScreens
{
    namespace
    {
        std::uint32_t dword(const std::array<std::uint8_t, 88>& data, std::size_t offset)
        {
            std::uint32_t value = 0;
            std::memcpy(&value, data.data() + offset, sizeof(value));
            return value;
        }

        float real(const std::array<std::uint8_t, 88>& data, std::size_t offset)
        {
            float value = 0;
            std::memcpy(&value, data.data() + offset, sizeof(value));
            return value;
        }

        std::string lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }
    }

    std::optional<TipLayout> readTipLayout(const std::array<std::uint8_t, 88>& data)
    {
        // The DATA of the record: the type, the box of the tip on the canvas of 1280 by 960, the angle, the font and
        // the colour and the justification of its text (the rest is for the stats)
        if (dword(data, 0) != 3)
            return std::nullopt;
        TipLayout layout;
        layout.mType = 3;
        layout.mX = static_cast<float>(dword(data, 0x04));
        layout.mY = static_cast<float>(dword(data, 0x08));
        layout.mWidth = static_cast<float>(dword(data, 0x0C));
        layout.mHeight = static_cast<float>(dword(data, 0x10));
        layout.mRotate = real(data, 0x14);
        layout.mFont = static_cast<int>(dword(data, 0x18));
        layout.mColor = { real(data, 0x1C), real(data, 0x20), real(data, 0x24) };
        layout.mJustify = static_cast<int>(dword(data, 0x28));
        return layout;
    }

    Window tipWindow(const TipLayout& layout, float width, float height, float cropX, float cropY)
    {
        Window window;
        window.mLeft = std::max(layout.mX / 1280.f * width, cropX);
        window.mTop = std::max(layout.mY / 960.f * height, cropY);
        window.mRight = std::min(width - cropX, window.mLeft + layout.mWidth);
        window.mBottom = std::min(height - cropY, window.mTop + layout.mHeight);
        return window;
    }

    float tipX(const Window& window, int justify)
    {
        switch (justify)
        {
            case 1:
                return window.mLeft;
            case 2:
                return window.mLeft + (window.mRight - window.mLeft) / 2;
            case 4:
                return window.mRight;
            default:
                return 0;
        }
    }

    bool isForPlace(const Picture& picture, const Destination& destination)
    {
        if (!destination.mSet)
            return false;
        for (const Place& place : picture.mPlaces)
        {
            if (destination.mObject != 0 && place.mObject == destination.mObject)
                return true;
            if (destination.mExterior && place.mObject == 0 && place.mWorld == destination.mWorld
                && place.mGridX == destination.mGridX && place.mGridY == destination.mGridY)
                return true;
        }
        return false;
    }

    namespace
    {
        // Each pick takes one of the candidates at random; a picture whose file is in the list already is dropped when it
        // comes (0x78ADC0, 0x78AB20)
        void pick(const std::vector<Picture>& pool, std::vector<std::size_t> candidates, std::size_t wanted,
            const Random& random, std::vector<std::size_t>& chosen)
        {
            std::set<std::string> taken;
            for (const std::size_t index : chosen)
                taken.insert(lower(pool[index].mIcon));
            while (!candidates.empty() && chosen.size() < wanted)
            {
                const int last = static_cast<int>(candidates.size()) - 1;
                const std::size_t at = static_cast<std::size_t>(last > 0 ? std::clamp(random(0, last), 0, last) : 0);
                const std::size_t index = candidates[at];
                candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(at));
                if (taken.insert(lower(pool[index].mIcon)).second)
                    chosen.push_back(index);
            }
        }
    }

    std::vector<std::size_t> choosePictures(const std::vector<Picture>& pool, bool mainMenu,
        const Destination& destination, std::size_t most, std::size_t mostForPlace, const Random& random)
    {
        std::vector<std::size_t> chosen;
        std::vector<std::size_t> candidates;
        if (mainMenu)
        {
            for (std::size_t i = 0; i < pool.size(); ++i)
                if (!pool[i].mIcon.empty() && pool[i].mMainMenu)
                    candidates.push_back(i);
            pick(pool, std::move(candidates), most, random, chosen);
            return chosen;
        }
        if (destination.mSet)
        {
            for (std::size_t i = 0; i < pool.size(); ++i)
                if (!pool[i].mIcon.empty() && isForPlace(pool[i], destination))
                    candidates.push_back(i);
            pick(pool, std::move(candidates), std::min(most, mostForPlace), random, chosen);
            candidates.clear();
        }
        // What is left up to the most is general pictures (those with no places)
        for (std::size_t i = 0; i < pool.size(); ++i)
            if (!pool[i].mIcon.empty() && !pool[i].local())
                candidates.push_back(i);
        pick(pool, std::move(candidates), most, random, chosen);
        return chosen;
    }

    std::vector<std::size_t> choosePictures(
        const std::vector<Picture>& pool, bool mainMenu, std::size_t most, const Random& random)
    {
        return choosePictures(pool, mainMenu, Destination(), most, mostPlacePictures, random);
    }

    void SlideShow::start(double now)
    {
        mCount = 1;
        // Nothing before the first picture to fade over: it is shown at once
        mStamp = -1;
        mNext = now + mSeconds;
    }

    bool SlideShow::advance(double now)
    {
        if (mCount == 0 || now < mNext)
            return false;
        ++mCount;
        mStamp = now;
        mNext = now + mSeconds;
        return true;
    }

    double SlideShow::newestShare(double now) const
    {
        if (mStamp < 0 || mFade <= 0)
            return 1;
        return std::clamp((now - mStamp) / mFade, 0.0, 1.0);
    }

    std::array<float, 2> SlideShow::alphas(double now) const
    {
        const float share = static_cast<float>(newestShare(now));
        std::array<float, 2> result{ 0.f, 0.f };
        if (mCount == 0)
            return result;
        // The first picture has no one to fade over
        result[newest()] = share * 255.f;
        result[1 - newest()] = mCount > 1 ? (1.f - share) * 255.f : 0.f;
        return result;
    }
}

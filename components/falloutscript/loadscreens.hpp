#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LOADSCREENS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LOADSCREENS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace FalloutScript::LoadScreens
{
    /// A picture of the loading screen and the tip that goes with it (a LSCR record of New Vegas): the path of the
    /// picture, the words of the tip, whether the picture is for the main menu (record flag 0x400) and whether it is
    /// for a place in particular (it has locations)
    struct Picture
    {
        std::string mIcon;
        std::string mText;
        bool mMainMenu = false;
        bool mLocal = false;
    };

    /// How a load screen type (LSCT, FalloutNV.exe 0x78B9B0 and 0x78C770, Mac round 12AR C2) places the tip: the box on
    /// a canvas of 1280 by 960, the font and colour of the text and how it is justified
    struct TipLayout
    {
        /// 1 level progress, 2 objective, 3 tips, 4 stats
        int mType = 0;
        float mX = 0;
        float mY = 0;
        float mWidth = 0;
        float mHeight = 0;
        float mRotate = 0;
        int mFont = 7;
        /// Red, green and blue as the record has them (0 to 255)
        std::array<float, 3> mColor{ 255.f, 255.f, 255.f };
        /// 1 left, 2 centre, 4 right
        int mJustify = 1;
    };

    /// The layout the DATA of a load screen type gives; nothing for a type that is not the tips (the other types draw
    /// groups of the menu that the data of the game does not use)
    std::optional<TipLayout> readTipLayout(const std::array<std::uint8_t, 88>& data);

    /// A box of the screen, in the units of the menu
    struct Window
    {
        float mLeft = 0;
        float mTop = 0;
        float mRight = 0;
        float mBottom = 0;
        float width() const { return mRight - mLeft; }
        float height() const { return mBottom - mTop; }
    };

    /// The box of the layout on a screen of `width` by `height` units that keeps `cropX` and `cropY` free at its edges:
    /// the canvas of 1280 by 960 is stretched to the screen, and the box is kept inside the safe zone
    Window tipWindow(const TipLayout& layout, float width, float height, float cropX, float cropY);

    /// Where the text of the tip begins: the left edge, the middle or the right edge of the box, by the justification
    float tipX(const Window& window, int justify);

    /// The seconds between the pictures: 10 while the game loads, 4 when it starts
    constexpr double pictureSeconds = 10.0;
    /// The time a picture takes to fade in over the one before it, in seconds (1 / 0.00045 ms)
    constexpr double crossFadeSeconds = 1.0 / 0.45;
    /// The time the menu takes to fade in over the last picture of the world, in seconds (menufade)
    constexpr double menuFadeSeconds = 0.75;
    /// How many pictures one loading screen holds (iMaxScreens:Loading)
    constexpr std::size_t mostPictures = 4;

    using Random = std::function<int(int low, int high)>;

    /// The pictures one loading screen goes through: those of the main menu, or those that are not for one place only,
    /// in a random order and none twice (a picture of the same file is the same picture), at most `most` of them (the
    /// indexes into `pool`)
    std::vector<std::size_t> choosePictures(
        const std::vector<Picture>& pool, bool mainMenu, std::size_t most, const Random& random);

    /// The pictures take the screen turn by turn: the first is shown at once, and each after it fades in over the one
    /// before while the tip of that picture does the same (the tiles go by the number of the picture: an even one is
    /// the first of the two, an odd one the second)
    class SlideShow
    {
    public:
        /// `seconds` is how long a picture stays before the next one begins to come
        explicit SlideShow(double seconds = pictureSeconds, double fadeSeconds = crossFadeSeconds)
            : mSeconds(seconds)
            , mFade(fadeSeconds)
        {
        }

        /// The first picture is on the screen from `now` (in seconds)
        void start(double now);
        /// A picture came on when the time of the one before is over: true then
        bool advance(double now);
        /// How many pictures were shown, counting the first
        std::size_t count() const { return mCount; }
        /// The tile (0 or 1) of the picture that was shown last, and of the one before it
        std::size_t newest() const { return (mCount + 1) % 2; }
        /// How much of the newest picture shows (0 to 1); the one before it shows what is left
        double newestShare(double now) const;
        /// The alpha (0 to 255) of each of the two tiles
        std::array<float, 2> alphas(double now) const;

    private:
        double mSeconds;
        double mFade;
        std::size_t mCount = 0;
        double mStamp = -1;
        double mNext = 0;
    };
}

#endif

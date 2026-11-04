#include "falloutloading.hpp"

#include <algorithm>
#include <cmath>

#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_RotatingSkin.h>

#include <components/debug/debuglog.hpp>
#include <components/esm/records.hpp>
#include <components/misc/rng.hpp>

#include "../mwworld/esmstore.hpp"

#include "falloutmenuresources.hpp"

namespace OFGui
{
    namespace
    {
        namespace LS = FalloutScript::LoadScreens;

        // The record flag of a load screen that the main menu shows
        constexpr std::uint32_t mainMenuFlag = 0x400;
        // How many pictures a load from the main menu goes through (a load of a place goes through iMaxScreens, 4)
        constexpr std::size_t mostMainMenuPictures = 20;
        // The menu takes this long to show over the last picture of the world (its `menufade`)
        constexpr double menuFade = LS::menuFadeSeconds;
        // The pictures of the main menu change this often (the pictures of a place every ten seconds)
        constexpr double mainMenuSeconds = 4.0;
        // A load that begins this soon after the end of another is the same loading screen
        constexpr double resumeSeconds = 1.0;

        MyGUI::IntSize viewSize()
        {
            return MyGUI::RenderManager::getInstance().getViewSize();
        }

        ESM::RefId refIdOf(const ESM::FormId& form)
        {
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }

        // The picture of a piece: the part of the texture that is drawn (a mirrored piece reads it from its right edge)
        std::string keyOf(const FalloutMenu::Piece& piece)
        {
            return piece.mTexture + '|' + std::to_string(piece.mSourceX) + '|' + std::to_string(piece.mSourceY) + '|'
                + std::to_string(piece.mSourceWidth) + '|' + std::to_string(piece.mSourceHeight)
                + (piece.mMirrored ? "|m" : "");
        }
    }

    FalloutLoading::FalloutLoading()
    {
        // (the tips of the New Vegas record, for a record whose type the data does not give)
        mDefaultLayout.mType = 3;
        mDefaultLayout.mX = 210;
        mDefaultLayout.mY = 100;
        mDefaultLayout.mWidth = 850;
        mDefaultLayout.mHeight = 400;
        mDefaultLayout.mFont = 6;
        mDefaultLayout.mColor = { 255.f, 180.f, 53.f };
        mDefaultLayout.mJustify = 2;
    }

    FalloutLoading::~FalloutLoading()
    {
        if (mRoot != nullptr && MyGUI::Gui::getInstancePtr() != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    void FalloutLoading::readPool()
    {
        const auto& records = mStore->get<ESM4::LoadScreen>();
        // (the store is filled while the data loads: it is read again when it has grown)
        if (records.getSize() == mPoolSource && !mPool.empty())
            return;
        mPool.clear();
        mRecords.clear();
        mPoolSource = records.getSize();
        for (const ESM4::LoadScreen& record : records)
        {
            if (record.mIcon.empty())
                continue;
            LS::Picture picture;
            picture.mIcon = record.mIcon;
            picture.mText = record.mDescription;
            picture.mMainMenu = (record.mFlags & mainMenuFlag) != 0;
            for (const ESM4::LoadScreen::Location& location : record.mLocations)
                picture.mPlaces.push_back({ static_cast<std::uint32_t>(location.mDirect),
                    static_cast<std::uint32_t>(location.mIndirect), location.mGridX, location.mGridY });
            mPool.push_back(std::move(picture));
            mRecords.push_back(&record);
        }
    }

    bool FalloutLoading::makeMenu()
    {
        if (mMenu)
            return true;
        if (mMenuFailed)
            return false;
        const MyGUI::IntSize size = viewSize();
        if (!mResources)
            mResources = makeFalloutMenuResources();
        std::string error;
        mMenu = LS::LoadingMenu::open(
            mResources, static_cast<float>(size.width), static_cast<float>(size.height), &error);
        if (!mMenu)
        {
            // (once: a game without the menu file keeps the loading screen of the engine)
            mMenuFailed = true;
            Log(Debug::Warning) << "The loading menu of the game could not be opened: " << error;
            return false;
        }
        return true;
    }

    bool FalloutLoading::begin(bool mainMenu, const LS::Destination& destination, double now)
    {
        // The engine begins a loading screen for each step of a change of place (three within a second and a half for one
        // door, Mac round 12BE C4), each with another picture and tip that the player has no time to read; the game shows
        // one screen for all of it, so a screen that begins as the last one ended goes on with it
        if (mCanResume && mainMenu == mLastMainMenu && now - mEndedAt <= resumeSeconds && mRoot != nullptr && mMenu)
        {
            mCanResume = false;
            mActive = true;
            mRoot->setVisible(true);
            update(now);
            return true;
        }
        mCanResume = false;
        mActive = false;
        mWorldAlpha = 1.f;
        if (mStore == nullptr)
            return false;
        readPool();
        // The pictures of the place the load goes to come first (iNumLocationSpecificScreens), then general ones up to
        // iMaxScreens (4; 20 from the main menu): an interior has pictures too (Mac round 12AY R3)
        mChosen = LS::choosePictures(mPool, mainMenu, destination, mainMenu ? mostMainMenuPictures : LS::mostPictures,
            LS::mostPlacePictures, [](int low, int high) { return low + Misc::Rng::rollDice(high - low + 1); });
        if (mChosen.empty())
            return false;
        if (!makeMenu())
            return false;
        if (mRoot == nullptr)
        {
            mRoot = MyGUI::Gui::getInstance().createWidgetReal<MyGUI::Widget>("Widget", MyGUI::FloatCoord(0, 0, 1, 1),
                MyGUI::Align::Stretch, "LoadingScreenBackground", "FalloutLoading");
            mSlides = mRoot->createWidgetReal<MyGUI::Widget>(
                "Widget", MyGUI::FloatCoord(0, 0, 1, 1), MyGUI::Align::Stretch);
            mSlides->setNeedMouseFocus(false);
            // The wheel is an overlapped widget: it has its own layer node, which MyGUI draws after the one of the
            // pictures whatever the order in which the images of each were made (images of one node are drawn in the
            // order they were made, and a picture with more pieces than the one before makes new ones later)
            mTurning = mRoot->createWidget<MyGUI::Widget>(MyGUI::WidgetStyle::Overlapped, "Widget",
                MyGUI::IntCoord(0, 0, mRoot->getWidth(), mRoot->getHeight()), MyGUI::Align::Stretch);
            mTurning->setNeedMouseFocus(false);
        }
        mRoot->setVisible(true);

        mMenu->clear();
        mShow = LS::SlideShow(mainMenu ? mainMenuSeconds : LS::pictureSeconds, LS::crossFadeSeconds);
        if (!mChosen.empty())
        {
            showPicture(0, mChosen[0]);
            mShow.start(now);
        }
        mStart = now;
        mActive = true;
        mLastMainMenu = mainMenu;
        update(now);
        return true;
    }

    void FalloutLoading::showPicture(std::size_t slide, std::size_t pool)
    {
        const LS::Picture& picture = mPool[pool];
        LS::TipLayout layout = mDefaultLayout;
        const ESM4::LoadScreen& record = *mRecords[pool];
        // The layout is the load screen type's of the record (the data of New Vegas has one, of the tips)
        if (const ESM::FormId& type = record.mLoadScreenType; !type.isZeroOrUnset())
        {
            if (const ESM4::LoadScreenType* found = mStore->get<ESM4::LoadScreenType>().search(refIdOf(type)))
            {
                if (const std::optional<LS::TipLayout> read = LS::readTipLayout(found->mData))
                    layout = *read;
            }
        }
        mMenu->setSlide(slide, picture.mIcon, picture.mText, layout);
    }

    void FalloutLoading::update(double now)
    {
        if (!mActive)
            return;
        if (mShow.count() > 0 && mShow.count() < mChosen.size() && mShow.advance(now))
            showPicture(mShow.newest(), mChosen[mShow.count() - 1]);

        const MyGUI::IntSize size = viewSize();
        if (size.width > 0 && size.height > 0
            && (static_cast<float>(size.width) != mMenu->pixelWidth()
                || static_cast<float>(size.height) != mMenu->pixelHeight()))
            mMenu->resize(static_cast<float>(size.width), static_cast<float>(size.height));

        const float shown = static_cast<float>(std::clamp((now - mStart) / menuFade, 0.0, 1.0));
        mMenu->setAlphas(mShow.alphas(now), shown);
        drawWheel(now - mStart, shown);
        // The last frame of the world goes as the menu comes (the executable gives it the inverse of the fade)
        mWorldAlpha = 1.f - shown;
        draw();
    }

    FalloutLoading::Shape& FalloutLoading::shape(std::size_t index)
    {
        while (mShapes.size() <= index)
        {
            Shape made;
            made.mBox = mSlides->createWidget<MyGUI::ImageBox>(
                "ImageBox", MyGUI::IntCoord(0, 0, 1, 1), MyGUI::Align::Default);
            made.mBox->setNeedMouseFocus(false);
            made.mBox->setVisible(false);
            mShapes.push_back(std::move(made));
        }
        return mShapes[index];
    }

    void FalloutLoading::draw()
    {
        const std::vector<FalloutMenu::Piece> pieces = mMenu->pieces();
        const auto drawn = [](const FalloutMenu::Piece& piece) {
            return piece.mWidth > 0 && piece.mHeight > 0 && piece.mSourceWidth > 0 && piece.mSourceHeight > 0
                && !piece.mTexture.empty();
        };
        // A box that is given another picture is made again, with every box after it (the boxes are drawn in the order they
        // were made): boxes that were reused for other pictures showed only a third to four fifths of the letters of a tip
        // in the running game, and new ones nearly all (Mac round 12BE C)
        {
            std::size_t place = 0;
            std::size_t changed = mShapes.size();
            for (const FalloutMenu::Piece& piece : pieces)
            {
                if (!drawn(piece))
                    continue;
                if (place < mShapes.size() && !mShapes[place].mKey.empty() && mShapes[place].mKey != keyOf(piece))
                {
                    changed = place;
                    break;
                }
                ++place;
            }
            while (mShapes.size() > changed)
            {
                MyGUI::Gui::getInstance().destroyWidget(mShapes.back().mBox);
                mShapes.pop_back();
            }
        }
        std::size_t used = 0;
        for (const FalloutMenu::Piece& piece : pieces)
        {
            if (!drawn(piece))
                continue;
            Shape& slot = shape(used++);
            const std::string key = keyOf(piece);
            if (slot.mKey != key)
            {
                slot.mKey = key;
                // (a mirrored piece reads the part of its texture from the right edge to the left)
                const int sourceWidth = piece.mMirrored ? -piece.mSourceWidth : piece.mSourceWidth;
                const int sourceX = piece.mMirrored ? piece.mSourceX + piece.mSourceWidth : piece.mSourceX;
                slot.mBox->setImageTexture(piece.mTexture);
                slot.mBox->setImageTile(MyGUI::IntSize(sourceWidth, piece.mSourceHeight));
                slot.mBox->setImageCoord(MyGUI::IntCoord(sourceX, piece.mSourceY, sourceWidth, piece.mSourceHeight));
            }
            slot.mBox->setCoord(piece.mX, piece.mY, piece.mWidth, piece.mHeight);
            slot.mBox->setColour(MyGUI::Colour(piece.mColor[0], piece.mColor[1], piece.mColor[2]));
            slot.mBox->setAlpha(piece.mColor[3]);
            slot.mBox->setVisible(true);
        }
        for (std::size_t index = used; index < mShapes.size(); ++index)
            mShapes[index].mBox->setVisible(false);
    }

    namespace
    {
        // Where the textures of the wheel are in the data of the game (the model names them by their file; the folders
        // are tried in this order and the first that has the file is the one)
        constexpr const char* wheelFolders[] = { "textures/interface/loading/", "textures/interface/circular loading/",
            "textures/interface/main/", "textures/interface/", nullptr };

        // What `loading01.nif` of New Vegas holds, in the units of the model (Mac round 12AY R3 A1). The model's origin
        // is the tile's x and y; a model unit is `scale` units of the menu; the picture stands upright with x to the right
        // and y down. Each piece is a rectangle round the middle of its node.
        constexpr float wheelCenterX = -80.2141f;
        constexpr float wheelCenterY = 74.6074f;
        constexpr float wheelHalfWidth = 43.01f;
        constexpr float wheelHalfHeight = 43.095f;
        constexpr float ballPivotX = -80.4623f;
        constexpr float ballPivotY = 74.6507f;
        // The ball: its texture seen from the pivot (x and y from the left and the top, in model units)
        constexpr float ballLeft = -2.72f;
        constexpr float ballTop = -42.79f;
        constexpr float ballWidth = 5.59f;
        constexpr float ballHeight = 42.63f;
        constexpr float barsRightX = -6.5167f;
        constexpr float barsLeftX = -154.0758f;
        constexpr float barsY = 74.6315f;
        constexpr float barsHalfWidth = 42.81f;
        constexpr float barsHalfHeight = 43.04f;
        // The wheel turns once in 5.3333 seconds: it goes clockwise at 67.5 degrees a second, the ball against it at 135
        constexpr float wheelDegrees = 67.5f;
        constexpr float ballDegrees = -135.f;

        // What the model of Fallout 3 holds (Mac round 12BA D, in the units of the model): one picture, `main_timer.dds`,
        // 128 by 128, of which the wheel is the part u 0.0358 to 0.8466, v 0.0337 to 0.8783 and the needle the part
        // u 0.9039 to 0.9483, v 0.5215 to 0.9703. The needle turns about a point near its foot in sixteen steps of 22.5
        // degrees in the 5.3333 seconds of the loop (0.3 seconds of movement, eased, then a hold of a thirtieth of a
        // second), the same way round as the ball of New Vegas; the wheel stands still
        constexpr float needleWheelCenterX = -80.228f;
        constexpr float needleWheelCenterY = 74.622f;
        constexpr float needleWheelHalfWidth = 28.122f;
        constexpr float needleWheelHalfHeight = 29.296f;
        constexpr float needleLeft = -82.059f;
        constexpr float needleTop = 45.746f;
        constexpr float needleWidth = 2.675f;
        constexpr float needleHeight = 27.016f;
        constexpr float needlePivotX = -81.123f;
        constexpr float needlePivotY = 72.164f;
        constexpr float needleStepSeconds = 5.3333f / 16;
        constexpr float needleMoveSeconds = 0.3f;
    }

    void FalloutLoading::drawNeedleWheel(
        double seconds, float alpha, float x, float y, float s, double unitsX, double unitsY)
    {
        const float loop = static_cast<float>(std::fmod(seconds, 5.3333));
        const int step = std::min(15, static_cast<int>(loop / needleStepSeconds));
        const float moved = std::clamp((loop - step * needleStepSeconds) / needleMoveSeconds, 0.f, 1.f);
        // (the movement is eased: slow at its ends)
        const float eased = moved * moved * (3.f - 2.f * moved);
        turn(mWheel[2], "main_timer.dds", x + (needleWheelCenterX - needleWheelHalfWidth) * s,
            y + (needleWheelCenterY - needleWheelHalfHeight) * s, 2 * needleWheelHalfWidth * s,
            2 * needleWheelHalfHeight * s, needleWheelHalfWidth * s, needleWheelHalfHeight * s, 0.f, alpha, unitsX,
            unitsY, { 0.0358f, 0.0337f, 0.8466f, 0.8783f });
        turn(mWheel[3], "main_timer.dds", x + needleLeft * s, y + needleTop * s, needleWidth * s, needleHeight * s,
            (needlePivotX - needleLeft) * s, (needlePivotY - needleTop) * s, -22.5f * (step + eased), alpha, unitsX,
            unitsY, { 0.9039f, 0.5215f, 0.9483f, 0.9703f });
    }

    bool FalloutLoading::hasWheelFile(const char* file)
    {
        for (const char* const* directory = wheelFolders; *directory != nullptr; ++directory)
            if (mResources->data().mReadStart(std::string(*directory) + file, 32))
                return true;
        return false;
    }

    void FalloutLoading::turn(Turning& piece, const char* file, float left, float top, float width, float height,
        float pivotX, float pivotY, float degrees, float alpha, double unitsX, double unitsY,
        const std::array<float, 4>& part)
    {
        if (piece.mBox == nullptr)
        {
            piece.mBox = mTurning->createWidget<MyGUI::ImageBox>(
                "RotatingSkin", MyGUI::IntCoord(0, 0, 1, 1), MyGUI::Align::Default);
            piece.mBox->setNeedMouseFocus(false);
            piece.mBox->setVisible(false);
        }
        if (piece.mTexture.empty())
        {
            for (const char* const* directory = wheelFolders; *directory != nullptr; ++directory)
            {
                const std::string path = std::string(*directory) + file;
                // (the data is asked for the first bytes of the file: a size says it is there)
                if (const std::optional<std::string> start = mResources->data().mReadStart(path, 32))
                    if (const std::optional<FalloutMenu::TextSize> size = mResources->textureSize(path))
                    {
                        piece.mTexture = path;
                        piece.mWidth = static_cast<int>(size->mWidth);
                        piece.mHeight = static_cast<int>(size->mHeight);
                        const int partLeft = static_cast<int>(std::lround(part[0] * piece.mWidth));
                        const int partTop = static_cast<int>(std::lround(part[1] * piece.mHeight));
                        const int partWidth = std::max(1, static_cast<int>(std::lround(part[2] * piece.mWidth)) - partLeft);
                        const int partHeight
                            = std::max(1, static_cast<int>(std::lround(part[3] * piece.mHeight)) - partTop);
                        piece.mBox->setImageTexture(path);
                        piece.mBox->setImageTile(MyGUI::IntSize(partWidth, partHeight));
                        piece.mBox->setImageCoord(MyGUI::IntCoord(partLeft, partTop, partWidth, partHeight));
                        break;
                    }
            }
            if (piece.mTexture.empty())
                piece.mTexture = "-";
        }
        if (piece.mTexture == "-")
            return;
        const int x = static_cast<int>(std::lround(left * unitsX));
        const int y = static_cast<int>(std::lround(top * unitsY));
        const int w = std::max(1, static_cast<int>(std::lround(width * unitsX)));
        const int h = std::max(1, static_cast<int>(std::lround(height * unitsY)));
        piece.mBox->setCoord(x, y, w, h);
        if (MyGUI::RotatingSkin* skin = piece.mBox->getSubWidgetMain()->castType<MyGUI::RotatingSkin>(false))
        {
            skin->setCenter(MyGUI::IntPoint(
                static_cast<int>(std::lround(pivotX * unitsX)), static_cast<int>(std::lround(pivotY * unitsY))));
            skin->setAngle(degrees * 3.14159265f / 180.f);
        }
        piece.mBox->setAlpha(alpha);
        piece.mBox->setVisible(alpha > 0.f);
    }

    void FalloutLoading::drawWheel(double seconds, float alpha)
    {
        const FalloutScript::LoadScreens::LoadingMenu::Pinwheel wheel = mMenu->pinwheel();
        if (!wheel.mPresent || mTurning == nullptr)
            return;
        // Menu units to pixels
        const double unitsX = mMenu->designWidth() > 0 ? mMenu->pixelWidth() / mMenu->designWidth() : 1.0;
        const double unitsY = mMenu->designHeight() > 0 ? mMenu->pixelHeight() / mMenu->designHeight() : 1.0;
        const float s = wheel.mScale;
        if (mWheelKind == 0)
            mWheelKind = hasWheelFile("load_roulette_wheel.dds") ? 1 : hasWheelFile("main_timer.dds") ? 2 : 3;
        if (mWheelKind == 3)
            return;
        if (mWheelKind == 2)
        {
            drawNeedleWheel(seconds, alpha, wheel.mX, wheel.mY, s, unitsX, unitsY);
            return;
        }
        const float turned = static_cast<float>(std::fmod(seconds, 360.0 / wheelDegrees));
        // A piece is a rectangle round the middle of its node, turned about its middle; the model's origin is the tile's x
        // and y and a unit of the model is s units of the menu. Everything here is in units of the menu.
        const auto place = [&](Turning& piece, const char* file, float centerX, float centerY, float halfWidth,
                               float halfHeight, float degrees) {
            turn(piece, file, wheel.mX + (centerX - halfWidth) * s, wheel.mY + (centerY - halfHeight) * s,
                2 * halfWidth * s, 2 * halfHeight * s, halfWidth * s, halfHeight * s, degrees, alpha, unitsX, unitsY);
        };
        // Back to front: the bars on both sides (the left ones are the same picture turned half round), the wheel, the ball
        place(mWheel[0], "load_roulette_bars.dds", barsLeftX, barsY, barsHalfWidth, barsHalfHeight, 180.f);
        place(mWheel[1], "load_roulette_bars.dds", barsRightX, barsY, barsHalfWidth, barsHalfHeight, 0.f);
        place(mWheel[2], "load_roulette_wheel.dds", wheelCenterX, wheelCenterY, wheelHalfWidth, wheelHalfHeight,
            wheelDegrees * turned);
        // The ball hangs from its pivot: its picture stands above the pivot, which is a little below its foot, and the
        // widget turns about the pivot
        turn(mWheel[3], "load_roulette_ball.dds", wheel.mX + (ballPivotX + ballLeft) * s,
            wheel.mY + (ballPivotY + ballTop) * s, ballWidth * s, ballHeight * s, -ballLeft * s, -ballTop * s,
            ballDegrees * turned, alpha, unitsX, unitsY);
    }

    void FalloutLoading::end(double now)
    {
        mCanResume = mActive;
        mEndedAt = now;
        mActive = false;
        mWorldAlpha = 1.f;
        if (mRoot != nullptr)
            mRoot->setVisible(false);
        for (Shape& slot : mShapes)
            slot.mBox->setVisible(false);
        for (Turning& piece : mWheel)
            if (piece.mBox != nullptr)
                piece.mBox->setVisible(false);
        // (the menu keeps its picture and tip for a screen that goes on with this one, and is cleared when another begins)
    }
}

#include "falloutloading.hpp"

#include <algorithm>
#include <cmath>

#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_RenderManager.h>

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
            picture.mLocal = !record.mLocations.empty();
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

    bool FalloutLoading::begin(bool mainMenu, bool interior, double now)
    {
        mActive = false;
        mWorldAlpha = 1.f;
        if (mStore == nullptr)
            return false;
        readPool();
        mChosen.clear();
        if (!interior)
        {
            mChosen = LS::choosePictures(mPool, mainMenu, mainMenu ? mostMainMenuPictures : LS::mostPictures,
                [](int low, int high) { return low + Misc::Rng::rollDice(high - low + 1); });
            if (mChosen.empty())
                return false;
        }
        else if (mPool.empty())
        {
            return false;
        }
        if (!makeMenu())
            return false;
        if (mRoot == nullptr)
        {
            mRoot = MyGUI::Gui::getInstance().createWidgetReal<MyGUI::Widget>("Widget", MyGUI::FloatCoord(0, 0, 1, 1),
                MyGUI::Align::Stretch, "LoadingScreenBackground", "FalloutLoading");
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
        // The last frame of the world goes as the menu comes (the executable gives it the inverse of the fade)
        mWorldAlpha = 1.f - shown;
        draw();
    }

    FalloutLoading::Shape& FalloutLoading::shape(std::size_t index)
    {
        while (mShapes.size() <= index)
        {
            Shape made;
            made.mBox
                = mRoot->createWidget<MyGUI::ImageBox>("ImageBox", MyGUI::IntCoord(0, 0, 1, 1), MyGUI::Align::Default);
            made.mBox->setNeedMouseFocus(false);
            made.mBox->setVisible(false);
            mShapes.push_back(std::move(made));
        }
        return mShapes[index];
    }

    void FalloutLoading::draw()
    {
        const std::vector<FalloutMenu::Piece> pieces = mMenu->pieces();
        std::size_t used = 0;
        for (const FalloutMenu::Piece& piece : pieces)
        {
            if (piece.mWidth <= 0 || piece.mHeight <= 0 || piece.mSourceWidth <= 0 || piece.mSourceHeight <= 0
                || piece.mTexture.empty())
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

    void FalloutLoading::end()
    {
        mActive = false;
        mWorldAlpha = 1.f;
        if (mRoot != nullptr)
            mRoot->setVisible(false);
        for (Shape& slot : mShapes)
            slot.mBox->setVisible(false);
        if (mMenu)
            mMenu->clear();
    }
}

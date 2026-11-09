#include "falloutairmeter.hpp"

#include <algorithm>
#include <cmath>

#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_RotatingSkin.h>

#include <components/debug/debuglog.hpp>
#include <components/falloutmenu/openmenu.hpp>

#include "falloutmenuresources.hpp"

namespace OFGui
{
    namespace
    {
        constexpr const char* texturePath = "textures/interface/hud/air_meter.dds";

        // The model (Mac round 12BM D), in its units, from the middle of the ring (x to the right, up is negative y as
        // on the screen). A tick is a quad 12.655 wide and 19.933 high whose middle is 25.86 above the middle of the
        // ring; tick N is turned N sixteenths of a turn clockwise about the ring's middle, so the 16th is at the top.
        // The quad of the "O2" is 32.257 by 35.219 and a little right of and below the middle.
        constexpr float tickLeft = -6.0454f;
        constexpr float tickTop = -35.8263f;
        constexpr float tickWidth = 12.655f;
        constexpr float tickHeight = 19.933f;
        constexpr float labelLeft = -12.5468f;
        constexpr float labelTop = -15.7848f;
        constexpr float labelWidth = 32.257f;
        constexpr float labelHeight = 35.219f;
        // The middle of the ring is a little over and left of the origin of the model (the node of the ring is placed
        // at (0.466, -0.1926) from it and the ticks at (-0.5331, 2.1405) from that)
        constexpr float ringShiftX = -0.0671f;
        constexpr float ringShiftY = -1.9479f;
        // What of the 64 by 64 texture each quad shows (texels, from the quads' texture coordinates)
        constexpr int tickSourceX = 5;
        constexpr int tickSourceY = 5;
        constexpr int tickSourceWidth = 14;
        constexpr int tickSourceHeight = 21;
        constexpr int labelSourceX = 24;
        constexpr int labelSourceY = 4;
        constexpr int labelSourceWidth = 30;
        constexpr int labelSourceHeight = 32;
        constexpr float pi = 3.14159265f;
        constexpr float degreesPerTick = 22.5f;

        int rounded(float value)
        {
            return static_cast<int>(std::lround(value));
        }
    }

    FalloutAirMeter::FalloutAirMeter() = default;

    FalloutAirMeter::~FalloutAirMeter()
    {
        // (its widgets go with the root)
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    void FalloutAirMeter::make()
    {
        if (mRoot != nullptr || mFailed)
            return;
        if (!mResources)
            mResources = makeFalloutMenuResources();
        // A game without the picture has no meter
        if (!mResources->textureSize(texturePath))
        {
            mFailed = true;
            Log(Debug::Warning) << "The air meter is not drawn: the data has no " << texturePath;
            return;
        }
        // The layer of the breath bar of the engine's old display: nothing in it takes the pointer
        mRoot = MyGUI::Gui::getInstance().createWidgetReal<MyGUI::Widget>(
            "Widget", MyGUI::FloatCoord(0, 0, 1, 1), MyGUI::Align::Stretch, "DrowningBar", "FalloutAirMeter");
        mRoot->setNeedMouseFocus(false);
        mRoot->setVisible(false);
        for (std::size_t index = 0; index < mBoxes.size(); ++index)
        {
            MyGUI::ImageBox* box = mRoot->createWidget<MyGUI::ImageBox>(
                "RotatingSkin", MyGUI::IntCoord(0, 0, 1, 1), MyGUI::Align::Default);
            box->setNeedMouseFocus(false);
            const bool label = index == static_cast<std::size_t>(tickCount);
            const MyGUI::IntCoord source = label
                ? MyGUI::IntCoord(labelSourceX, labelSourceY, labelSourceWidth, labelSourceHeight)
                : MyGUI::IntCoord(tickSourceX, tickSourceY, tickSourceWidth, tickSourceHeight);
            box->setImageTexture(texturePath);
            box->setImageTile(source.size());
            box->setImageCoord(source);
            mBoxes[index] = box;
        }
    }

    void FalloutAirMeter::hide()
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
    }

    void FalloutAirMeter::draw(const State& state)
    {
        if (!(state.mAlpha > 0.f) || state.mUnit <= 0.f)
        {
            hide();
            return;
        }
        make();
        if (mRoot == nullptr)
            return;
        mRoot->setVisible(true);
        const float unit = state.mUnit;
        const float ringX = state.mX + ringShiftX * unit;
        const float ringY = state.mY + ringShiftY * unit;
        const MyGUI::Colour color(state.mColor[0], state.mColor[1], state.mColor[2]);
        const int shown = std::clamp(state.mTicks, 0, tickCount);
        for (int index = 0; index <= tickCount; ++index)
        {
            MyGUI::ImageBox* box = mBoxes[static_cast<std::size_t>(index)];
            const bool label = index == tickCount;
            // The ticks that show are the last ones: the tick next to the top goes first, then the ones after it,
            // clockwise
            const int number = index + 1;
            const bool visible = label || number > tickCount - shown;
            box->setVisible(visible);
            if (!visible)
                continue;
            const float left = label ? labelLeft : tickLeft;
            const float top = label ? labelTop : tickTop;
            const float width = label ? labelWidth : tickWidth;
            const float height = label ? labelHeight : tickHeight;
            box->setCoord(rounded(ringX + left * unit), rounded(ringY + top * unit), std::max(1, rounded(width * unit)),
                std::max(1, rounded(height * unit)));
            if (MyGUI::RotatingSkin* skin = box->getSubWidgetMain()->castType<MyGUI::RotatingSkin>(false))
            {
                // The ticks turn about the middle of the ring, which is not in their quad; the "O2" does not turn
                skin->setCenter(MyGUI::IntPoint(rounded(-left * unit), rounded(-top * unit)));
                skin->setAngle(label ? 0.f : number * degreesPerTick * pi / 180.f);
            }
            box->setColour(color);
            box->setAlpha(state.mAlpha);
        }
    }
}

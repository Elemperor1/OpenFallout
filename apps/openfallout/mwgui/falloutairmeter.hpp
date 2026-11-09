#ifndef OPENFALLOUT_MWGUI_FALLOUTAIRMETER_H
#define OPENFALLOUT_MWGUI_FALLOUTAIRMETER_H

#include <array>
#include <memory>

namespace MyGUI
{
    class ImageBox;
    class Widget;
}

namespace FalloutMenu
{
    class Resources;
}

namespace OFGui
{
    /// The air meter of the display of Fallout 3 and New Vegas: a ring of 16 ticks round an "O2" with one tick lost for
    /// each sixteenth of the breath that is gone, drawn where the game's `AirTimer01.NIF` puts them (the model's nodes
    /// were read in Mac round 12BM). The display's menu file only names the model, which the menu engine does not
    /// draw, so the widgets here do: each tick is a picture turned about the middle of the ring.
    class FalloutAirMeter
    {
    public:
        FalloutAirMeter();
        ~FalloutAirMeter();

        /// Where and how the meter is drawn, in the units of the interface
        struct State
        {
            /// The origin of the model (the position of its tile in the display), and the size of a unit of the model
            float mX = 0.f;
            float mY = 0.f;
            float mUnit = 1.f;
            /// 0 to 1
            float mAlpha = 0.f;
            /// How many of the 16 ticks show (the last ones: the first to go is the one next to the top)
            int mTicks = 16;
            /// The colour of the display
            std::array<float, 3> mColor{ 1.f, 1.f, 1.f };
        };

        void draw(const State& state);
        /// Nothing is drawn (the widgets stay for the next time)
        void hide();

        /// The number of ticks of the ring
        static constexpr int tickCount = 16;

    private:
        void make();

        std::shared_ptr<FalloutMenu::Resources> mResources;
        MyGUI::Widget* mRoot = nullptr;
        /// The ticks, then the "O2"
        std::array<MyGUI::ImageBox*, tickCount + 1> mBoxes{};
        bool mFailed = false;
    };
}

#endif

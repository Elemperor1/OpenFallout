#ifndef OPENFALLOUT_COMPONENTS_FALLOUTMENU_OPENMENU_H
#define OPENFALLOUT_COMPONENTS_FALLOUTMENU_OPENMENU_H

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "atlas.hpp"
#include "font.hpp"
#include "listbox.hpp"
#include "menu.hpp"

namespace FalloutMenu
{
    /// What the player's install and the running game give to the menus
    struct Data
    {
        /// A file of the data, from its path in lower case with `/` (`textures/fonts/monofonto_large.fnt`)
        std::function<std::optional<std::string>(const std::string& path)> mRead;
        /// The first bytes of a file, enough to read the size of a picture from
        std::function<std::optional<std::string>(const std::string& path, std::size_t count)> mReadStart;
        /// The text of a game setting (`&-sHowMany;`)
        std::function<std::optional<std::string>(std::string_view name)> mSetting;
    };

    /// The size of a DDS picture, from the first bytes of its file
    std::optional<TextSize> ddsSize(std::string_view start);

    /// The path of a texture in the data from the way a menu file writes it (`Interface\Main\arrow.dds`)
    std::string texturePath(std::string_view written);

    /// What every menu of a game shares: the fonts, the atlas of small pictures, the sizes of the textures
    class Resources
    {
    public:
        /// `fontFiles` says which file each of the fonts 1 to 9 is (as the INI of the game does); the ones it leaves
        /// out are the ones the games ship with
        explicit Resources(Data data, std::map<int, std::string> fontFiles = {});

        const Data& data() const { return mData; }
        const FontSet& fonts() const { return mFonts; }
        const Atlas& atlas() const { return mAtlas; }
        /// The size in pixels of a texture of the data; nothing if the data lacks it
        std::optional<TextSize> textureSize(const std::string& path);
        /// The colour of a system colour (1 to 6), red, green and blue from 0 to 255
        std::optional<std::array<float, 3>> systemColor(int id) const;
        void setSystemColor(int id, std::array<float, 3> color);
        /// The system colours of New Vegas: its executable makes the colours of the head-up display, the Pip-Boy, the
        /// main menu and the system amber (255, 182, 66); the others, and all of Fallout 3's, are the INI's (Mac round
        /// 12BN, FalloutNV.exe 0x718C90 and Fallout3.exe 0x6291B2)
        void useNewVegasColors();

        /// The textures that menus asked for and the data does not have
        const std::set<std::string>& missingTextures() const { return mMissing; }

    private:
        Data mData;
        FontSet mFonts;
        Atlas mAtlas;
        std::unordered_map<std::string, std::optional<TextSize>> mSizes;
        std::map<int, std::array<float, 3>> mColors;
        std::set<std::string> mMissing;
    };

    /// A picture to put on the screen, in pixels of the window: a part of a texture, or one character of a text
    struct Piece
    {
        const Tile* mTile = nullptr;
        int mTileId = 0;
        /// Where it goes. A mirrored picture is drawn from the right to the left.
        int mX = 0;
        int mY = 0;
        int mWidth = 0;
        int mHeight = 0;
        bool mMirrored = false;
        /// Red, green, blue and alpha, 0 to 1
        std::array<float, 4> mColor{ 1.f, 1.f, 1.f, 1.f };
        /// The texture (a path of the data) and the part of it that is drawn, in pixels
        std::string mTexture;
        int mSourceX = 0;
        int mSourceY = 0;
        int mSourceWidth = 0;
        int mSourceHeight = 0;
        /// One character of a text
        bool mGlyph = false;
        /// The window the piece is cut to, if it has one
        bool mClipped = false;
        int mClipX = 0;
        int mClipY = 0;
        int mClipWidth = 0;
        int mClipHeight = 0;
    };

    /// A menu of the game opened for a window of some size: the tiles, what they show as pictures, and what the pointer
    /// does to them
    class OpenMenu
    {
    public:
        /// `file` is the menu file (`message_menu.xml`, in the menus folder; the folder and the ending may be left
        /// out). `pad` is whether a pad is in use, which `&xbox;` stands for in the files. Nothing if the file is not
        /// there, with the reason in `error`.
        static std::unique_ptr<OpenMenu> open(std::shared_ptr<Resources> resources, const std::string& file,
            float pixelWidth, float pixelHeight, bool pad, std::string* error = nullptr);

        Menu& menu() { return *mMenu; }
        /// The fonts the menu is drawn in (to measure a text as the game's code does)
        const FontSet& fonts() const { return mResources->fonts(); }
        const std::vector<std::string>& problems() const { return mLoader->problems(); }

        void resize(float pixelWidth, float pixelHeight);
        float pixelWidth() const { return mPixelWidth; }
        float pixelHeight() const { return mPixelHeight; }
        /// Pixels for one unit of the menu's design space
        float scale() const { return mPixelHeight / mMenu->designHeight(); }

        /// Everything to draw, back to front
        std::vector<Piece> pieces();

        /// The pointer moved to a point of the window in pixels. The tile under it is the one that has `mouseover`; the
        /// tile that has it is returned (the one that lost it is not). A tile that is being dragged follows.
        Tile* pointerMoved(float x, float y);
        /// The button went down. A tile that can be dragged starts to be (`dragstartx`, `dragx`, ...).
        Tile* pointerPressed(float x, float y);
        /// The button went up. The tile that it went down on and goes up on has `clicked` counted up by one, and is
        /// returned; the drag ends.
        Tile* pointerReleased(float x, float y);
        /// The wheel turned over a point: `wheelmoved` of the tile that takes it counts by `notches` (up is positive).
        Tile* wheelMoved(float x, float y, int notches);
        /// A click that did not come from the pointer (the key or button of the pad that stands for a tile)
        void click(Tile& tile);
        Tile* hovered() { return mMenu->byId(mHover); }
        /// The tile that is being dragged, if one is
        Tile* dragged() { return mMenu->byId(mDragged); }

        /// Put a scrollbar (a tile of the file that has a `_current_value`) at a value. The files add the clicks of the
        /// arrows, the turns of the wheel and the drag of the marker; the drag counts only while the button is down, so
        /// the pointer lets go with the value put into the count of the arrow that goes forward, as the arrows would
        /// have it, and the other counts cleared.
        void setScrollbar(Tile& bar, float value);
        /// The tiles that are scrollbars
        const std::vector<int>& scrollbars() const { return mScrollbars; }

        /// Make a list of a tile that the file gives `list_box.xml` and a template for its rows; the number of the
        /// list, -1 if the menu has no such tile or template
        int addList(std::string_view listTile, std::string_view templateName);
        ListBox* list(int number);
        /// The list and the row of it that a tile is (or is in), if it is
        std::optional<std::pair<int, int>> rowOf(const Tile& tile);

        /// Take a tile that was made from a template out of the menu (the pointer lets go of it first)
        void remove(Tile& tile);
        void removeChildren(Tile& tile);

    private:
        OpenMenu() = default;
        void pieceForImage(const DrawItem& item, std::vector<Piece>& out);
        void piecesForText(const DrawItem& item, std::vector<Piece>& out);
        std::array<float, 2> localPoint(const Tile& tile, float designX, float designY);
        void forget(const Tile& tile);
        void findScrollbars();
        void layoutLists();
        std::vector<std::pair<int, float>> scrollbarValues();
        void restoreScrollbars(const std::vector<std::pair<int, float>>& values);

        std::shared_ptr<Resources> mResources;
        std::unique_ptr<Loader> mLoader;
        std::unique_ptr<Menu> mMenu;
        float mPixelWidth = 0;
        float mPixelHeight = 0;
        /// The ids of the tiles the pointer is on, went down on and drags (0 for none)
        int mHover = 0;
        /// The hotrects around the tile the pointer is on, which are over as well
        std::vector<int> mHoverChain;
        int mPressed = 0;
        int mDragged = 0;
        std::vector<int> mScrollbars;
        std::vector<std::unique_ptr<ListBox>> mLists;
    };
}

#endif

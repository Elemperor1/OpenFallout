#ifndef OPENFALLOUT_COMPONENTS_FALLOUTMENU_MENU_H
#define OPENFALLOUT_COMPONENTS_FALLOUTMENU_MENU_H

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "menufile.hpp"
#include "richtext.hpp"
#include "value.hpp"

namespace FalloutMenu
{
    struct TextSize
    {
        float mWidth = 0;
        float mHeight = 0;
    };

    /// What a menu asks of the world around it
    struct Environment
    {
        /// The size in pixels of an image of the game's data, drawn from a file or from a texture atlas
        std::function<std::optional<TextSize>(const std::string& file, const std::string& atlas)> mImageSize;
        /// The size a text takes in a font (1 to 9): `wrapWidth` is zero when the text is not to be broken into lines
        std::function<TextSize(int font, const std::string& text, float wrapWidth)> mTextSize;
        /// The distance from one line to the next in a font (1 to 9); a text with marks (`ishtml`) needs it, and
        /// `mTextSize` for the width of a line
        std::function<float(int font)> mLineHeight;
        /// The size of the tile of a text with no characters in a font (1 to 9): the games lay it out as one space.
        /// Without it, an empty text has no size
        std::function<TextSize(int font)> mEmptyTextSize;
        /// The colour of a system colour: red, green and blue from 0 to 255
        std::function<std::optional<std::array<float, 3>>(int id)> mSystemColor;
    };

    class Menu;

    /// A tile of a menu that is shown: a part of the tree that a file describes. Tiles in a Menu are owned by the Menu.
    /// Pointers and references to a tile are borrowed and become invalid when that tile or an ancestor is removed, when
    /// removeChildren is called on an ancestor, when a different Menu is move-assigned to their owning Menu, or when
    /// their owning Menu is destroyed.
    class Tile
    {
    public:
        TileType type() const { return mType; }
        const std::string& name() const { return mName; }
        int uid() const { return mId; }
        Tile* parent() { return mParent; }
        const Tile* parent() const { return mParent; }
        const std::vector<std::unique_ptr<Tile>>& children() const { return mChildren; }
        const TileDef* definition() const { return mDefinition; }

    private:
        friend class Menu;

        struct Cached
        {
            Value mValue;
            unsigned mGeneration = 0;
            bool mBusy = false;
        };

        TileType mType = TileType::Rect;
        std::string mName;
        std::string mNameLower;
        int mId = 0;
        const TileDef* mDefinition = nullptr;
        std::shared_ptr<const TileDef> mOwnedDefinition;
        Tile* mParent = nullptr;
        std::vector<std::unique_ptr<Tile>> mChildren;
        /// The traits the screen has set; they win over what the file says
        std::unordered_map<std::string, Value> mSet;
        mutable std::unordered_map<std::string, Cached> mCache;
    };

    /// A thing to draw, with everything in design units (the menu is laid out in a tile of 960 high) and the colour of
    /// the end: the system colour or the tile's own, times its brightness
    struct DrawItem
    {
        const Tile* mTile = nullptr;
        TileType mType = TileType::Rect;
        float mX = 0;
        float mY = 0;
        float mWidth = 0;
        float mHeight = 0;
        /// Red, green, blue and alpha, 0 to 1
        std::array<float, 4> mColor{ 1.f, 1.f, 1.f, 1.f };
        /// An image: the file and the texture atlas that holds it, if one does
        std::string mFile;
        std::string mAtlas;
        bool mRepeatX = false;
        bool mRepeatY = false;
        float mCropX = 0;
        float mCropY = 0;
        /// A picture that scrolls (a tile with `_sourcex`): it shows a window of its texture at the natural size, from
        /// this many texels of the left edge on, and goes on from the left edge when it runs past the right one (the
        /// strip of the compass of the head-up display)
        bool mScrolls = false;
        float mScrollX = 0;
        /// A text: the string, the font and how it is placed on `mX` (1 left, 2 center, 4 right)
        std::string mText;
        int mFont = 1;
        int mJustify = 1;
        float mWrapWidth = 0;
        /// The window the tile is clipped to, if it asks for it
        bool mClipped = false;
        float mClipX = 0;
        float mClipY = 0;
        float mClipWidth = 0;
        float mClipHeight = 0;
    };

    class Menu
    {
    public:
        /// `root` is the menu, `globals` the tile that holds the constants every menu uses (it may be null),
        /// `templates` the templates the menu file and its includes declare
        Menu(std::shared_ptr<const TileDef> root, std::shared_ptr<const TileDef> globals,
            std::unordered_map<std::string, std::shared_ptr<const TileDef>> templates, Environment environment);

        Tile& root() { return *mRoot; }
        Tile& screen() { return *mScreen; }
        Tile& globals() { return *mGlobals; }

        /// The size of the screen in pixels, from which the size of the design space follows: a tile 960 high, and as
        /// wide as the screen is for its height
        void setScreen(float pixelWidth, float pixelHeight);
        /// Counts up whenever something that a trait depends on may have changed: a screen that draws again only when
        /// this has moved on
        unsigned generation() const { return mGeneration; }
        float designWidth() const { return mDesignWidth; }
        float designHeight() const { return mDesignHeight; }

        /// The value of a trait of a tile, calculated from what the file says and what the screen has set
        Value get(const Tile& tile, std::string_view trait);
        float number(const Tile& tile, std::string_view trait) { return get(tile, trait).number(); }
        std::string text(const Tile& tile, std::string_view trait) { return get(tile, trait).text(); }
        /// Set a trait of a tile to a value of its own, which wins over the file until it is cleared
        void set(Tile& tile, std::string_view trait, Value value);
        void clear(Tile& tile, std::string_view trait);
        bool isSet(const Tile& tile, std::string_view trait) const;
        bool defines(const Tile& tile, std::string_view trait) const;

        /// The first tile with this name, depth first, in the whole menu
        Tile* find(std::string_view name);
        Tile* find(Tile& under, std::string_view name);
        Tile* byId(int id);
        std::vector<Tile*> tiles();

        bool hasTemplate(std::string_view name) const;
        /// A new tile under `parent` from a template of the menu, named `name` (the template's name if empty)
        Tile* instantiate(Tile& parent, std::string_view templateName, std::string_view name = {});
        /// A new tile of a kind with no traits of its own under `parent`, which a screen gives its traits with `set`: a
        /// screen whose pieces come from the code and not from the file (the lines of the credits)
        Tile* create(Tile& parent, TileType type, std::string_view name);
        /// Take a tile that was made with `instantiate` out of the tree
        void remove(Tile& tile);
        void removeChildren(Tile& tile);

        /// The tile and trait a trait stands for when it is a reference (`<ref src="Button" trait="clicked"/>`, as the
        /// buttons of the pad and the keys of the lists are written)
        std::optional<std::pair<Tile*, std::string>> reference(Tile& tile, std::string_view trait);

        /// Everything to draw, back to front
        std::vector<DrawItem> draw();

        /// The tile the pointer is on at a point of the design space: the topmost visible tile that is a `target`
        Tile* tileAt(float x, float y);
        /// The tile under a point that takes the wheel (`wheelable`)
        Tile* wheelTileAt(float x, float y);
        /// The rectangle a tile takes, from its position and size and those of the tiles it is in
        std::optional<std::array<float, 4>> rectangle(Tile& tile);

        const std::vector<std::string>& problems() const { return mProblems; }

    private:
        struct Frame;

        std::unique_ptr<Tile> build(const std::shared_ptr<const TileDef>& definition, Tile* parent, bool owned);
        Tile* resolve(const Tile& me, Source source, const std::string& name);
        Tile* findIn(Tile& under, const std::string& lowerName, const Tile* skip);
        Value compute(const Tile& tile, const std::string& key);
        Value defaultOf(const Tile& tile, const std::string& key);
        Value evaluate(const Tile& tile, const Expression& expression);
        Value evaluateOperand(const Tile& tile, const Operand& operand, const Value& running);
        void collect(Tile& tile, float originX, float originY, const std::optional<std::array<float, 4>>& clip,
            bool clipped, std::vector<DrawItem>& out);
        /// A text with marks (the tile says `ishtml`): its pages are laid out here, and the page that `pagenum` names
        /// is drawn
        bool isRich(const Tile& tile);
        const std::vector<RichPage>& richPages(const Tile& tile);
        void collectRich(Tile& tile, float x, float y, const std::optional<std::array<float, 4>>& clip, bool clipped,
            std::vector<DrawItem>& out);
        std::array<float, 4> colorOf(const Tile& tile);
        bool visible(const Tile& tile);
        /// A hotrect whose position is its child's (the buttons that `super_text_box` makes): the child already stands
        /// where it should, so the hotrect's position must not be added to it once more
        bool mirrorsChild(const Tile& tile) const;
        /// How far left of its `x` a text starts, for the point it is justified to
        float anchorShift(Tile& tile);
        void assignIds(Tile& tile);
        void unindex(Tile& tile);
        void hitTest(Tile& tile, float originX, float originY, float x, float y, bool wheel, Tile*& best,
            std::optional<std::array<float, 4>> clip, bool clipped);

        Environment mEnvironment;
        std::unordered_map<std::string, std::shared_ptr<const TileDef>> mTemplates;
        std::unique_ptr<Tile> mRoot;
        std::unique_ptr<Tile> mScreen;
        std::unique_ptr<Tile> mGlobals;
        unsigned mGeneration = 1;
        int mNextId = 1;
        std::unordered_map<int, Tile*> mById;
        /// The pages of the texts with marks, by tile, with what they were laid out from
        struct RichCache
        {
            std::string mKey;
            std::vector<RichPage> mPages;
        };
        std::unordered_map<int, RichCache> mRich;
        float mDesignWidth = 1280;
        float mDesignHeight = 960;
        std::vector<std::string> mProblems;
    };
}

#endif

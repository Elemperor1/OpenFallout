#include "openmenu.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "xml.hpp"

namespace FalloutMenu
{
    namespace
    {
        // The fonts the games ship with, by number: the folder is Textures\Fonts
        constexpr const char* defaultFonts[FontSet::count] = {
            "glow_monofonto_large.fnt",
            "monofonto_large.fnt",
            "glow_monofonto_medium.fnt",
            "monofonto_verylarge02_dialogs2.fnt",
            "fixedsys_comp_uniform_width.fnt",
            "glow_monofonto_vl_dialogs.fnt",
            "baked-in_monofonto_large.fnt",
            "glow_futura_caps_large.fnt",
            "nvfont_test.fnt",
        };

        // The system colours as the INI of the games sets them, numbered as the executable numbers them
        constexpr std::array<float, 3> defaultColors[6] = {
            { 26, 255, 128 }, // HUDMain
            { 255, 67, 42 }, // HUDAlt
            { 33, 231, 121 }, // Terminal
            { 26, 255, 128 }, // Pipboy
            { 199, 255, 165 }, // MainMenu
            { 26, 255, 128 }, // System
        };

        std::string lowerSlash(std::string_view text)
        {
            std::string result;
            result.reserve(text.size());
            for (char c : text)
                result.push_back(c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            return result;
        }

        std::string lastPart(const std::string& path)
        {
            const std::size_t slash = path.find_last_of('/');
            return slash == std::string::npos ? path : path.substr(slash + 1);
        }

        int roundToInt(float value)
        {
            return static_cast<int>(std::lround(value));
        }
    }

    std::optional<TextSize> ddsSize(std::string_view start)
    {
        // "DDS ", the size of the header (124), the flags, then the height and the width
        if (start.size() < 20 || std::memcmp(start.data(), "DDS ", 4) != 0)
            return std::nullopt;
        std::uint32_t height = 0;
        std::uint32_t width = 0;
        std::memcpy(&height, start.data() + 12, sizeof(height));
        std::memcpy(&width, start.data() + 16, sizeof(width));
        if (width == 0 || height == 0 || width > 16384 || height > 16384)
            return std::nullopt;
        return TextSize{ static_cast<float>(width), static_cast<float>(height) };
    }

    std::string texturePath(std::string_view written)
    {
        const std::string trimmed(Xml::trim(written));
        std::string path = lowerSlash(trimmed);
        if (path.rfind("data/", 0) == 0)
            path.erase(0, 5);
        if (path.rfind("textures/", 0) != 0)
            path = "textures/" + path;
        return path;
    }

    Resources::Resources(Data data, std::map<int, std::string> fontFiles)
        : mData(std::move(data))
    {
        for (int number = 1; number <= FontSet::count; ++number)
        {
            std::string file = defaultFonts[number - 1];
            const auto given = fontFiles.find(number);
            if (given != fontFiles.end())
                file = lastPart(lowerSlash(given->second));
            const std::string path = "textures/fonts/" + file;
            if (!mData.mRead)
                continue;
            const std::optional<std::string> bytes = mData.mRead(path);
            if (!bytes)
                continue;
            if (std::optional<Font> font = Font::parse(*bytes))
                mFonts.set(number, std::move(*font));
        }
        if (mData.mRead)
            if (const std::optional<std::string> list = mData.mRead("textures/interface/interfaceshared.tai"))
                mAtlas = Atlas::parse(*list);
        for (int id = 1; id <= 6; ++id)
            mColors[id] = defaultColors[id - 1];
    }

    std::optional<TextSize> Resources::textureSize(const std::string& path)
    {
        const auto known = mSizes.find(path);
        if (known != mSizes.end())
            return known->second;
        std::optional<TextSize> size;
        if (mData.mReadStart)
            if (const std::optional<std::string> start = mData.mReadStart(path, 32))
                size = ddsSize(*start);
        mSizes.emplace(path, size);
        if (!size)
            mMissing.insert(path);
        return size;
    }

    std::optional<std::array<float, 3>> Resources::systemColor(int id) const
    {
        const auto found = mColors.find(id);
        if (found == mColors.end())
            return std::nullopt;
        return found->second;
    }

    void Resources::setSystemColor(int id, std::array<float, 3> color)
    {
        mColors[id] = color;
    }

    std::unique_ptr<OpenMenu> OpenMenu::open(std::shared_ptr<Resources> resources, const std::string& file,
        float pixelWidth, float pixelHeight, bool pad, std::string* error)
    {
        std::unique_ptr<OpenMenu> result(new OpenMenu);
        result->mResources = std::move(resources);
        Resources* shared = result->mResources.get();

        LoaderEnvironment env;
        env.mReadFile = [shared](const std::string& path) { return shared->data().mRead(path); };
        env.mSetting = [shared](std::string_view name) -> std::optional<std::string> {
            if (!shared->data().mSetting)
                return std::nullopt;
            return shared->data().mSetting(name);
        };
        env.mEntity = [pad](std::string_view name) -> std::optional<float> {
            if (name == "xbox")
                return pad ? 1.f : 0.f;
            if (name == "xenon")
                return 0.f;
            return std::nullopt;
        };
        result->mLoader = std::make_unique<Loader>(env);

        std::string path = lowerSlash(Xml::trim(file));
        if (path.rfind("menus/", 0) != 0)
            path = "menus/" + path;
        if (path.size() < 4 || path.compare(path.size() - 4, 4, ".xml") != 0)
            path += ".xml";
        std::shared_ptr<const TileDef> definition = result->mLoader->loadMenu(path);
        if (!definition)
        {
            if (error != nullptr)
                *error = "the menu file " + path + " is not in the data or holds no menu";
            return nullptr;
        }
        std::shared_ptr<const TileDef> globals = result->mLoader->loadPlain("menus/globals.xml");

        Environment environment;
        environment.mImageSize
            = [shared](const std::string& written, const std::string& atlas) -> std::optional<TextSize> {
            if (!atlas.empty())
            {
                const std::string name = lastPart(lowerSlash(Xml::trim(written)));
                if (const Atlas::Entry* entry = shared->atlas().find(name))
                    if (const std::optional<TextSize> size
                        = shared->textureSize("textures/interface/" + lowerSlash(entry->mTexture)))
                        return TextSize{ entry->mWidth * size->mWidth, entry->mHeight * size->mHeight };
            }
            return shared->textureSize(texturePath(written));
        };
        environment.mTextSize = [shared](int number, const std::string& text, float wrapWidth) -> TextSize {
            const Font* font = shared->fonts().get(number);
            if (font == nullptr)
                return {};
            const Font::Layout layout = font->layout(text, wrapWidth, 1);
            return { layout.mWidth, layout.mHeight };
        };
        environment.mSystemColor = [shared](int id) { return shared->systemColor(id); };

        result->mMenu = std::make_unique<Menu>(definition, globals, result->mLoader->templates(), environment);
        result->resize(pixelWidth, pixelHeight);
        result->findScrollbars();
        return result;
    }

    void OpenMenu::resize(float pixelWidth, float pixelHeight)
    {
        if (pixelWidth <= 0 || pixelHeight <= 0)
            return;
        mPixelWidth = pixelWidth;
        mPixelHeight = pixelHeight;
        mMenu->setScreen(pixelWidth, pixelHeight);
    }

    void OpenMenu::pieceForImage(const DrawItem& item, std::vector<Piece>& out)
    {
        Resources& resources = *mResources;
        std::string texture;
        float sourceX = 0;
        float sourceY = 0;
        float sourceWidth = 0;
        float sourceHeight = 0;
        bool found = false;
        if (!item.mAtlas.empty())
        {
            const std::string name = lastPart(lowerSlash(Xml::trim(item.mFile)));
            if (const Atlas::Entry* entry = resources.atlas().find(name))
            {
                const std::string path = "textures/interface/" + lowerSlash(entry->mTexture);
                if (const std::optional<TextSize> size = resources.textureSize(path))
                {
                    texture = path;
                    sourceX = entry->mU * size->mWidth;
                    sourceY = entry->mV * size->mHeight;
                    sourceWidth = entry->mWidth * size->mWidth;
                    sourceHeight = entry->mHeight * size->mHeight;
                    found = true;
                }
            }
        }
        if (!found)
        {
            const std::string path = texturePath(item.mFile);
            const std::optional<TextSize> size = resources.textureSize(path);
            if (!size)
                return;
            texture = path;
            sourceWidth = size->mWidth;
            sourceHeight = size->mHeight;
        }

        const float scale = this->scale();
        const bool mirrored = item.mWidth < 0;
        const float width = std::abs(item.mWidth);
        const float height = item.mHeight;
        if (width <= 0 || height <= 0 || sourceWidth <= 0 || sourceHeight <= 0)
            return;
        // A picture drawn with a negative width stands left of its x
        const float left = mirrored ? item.mX - width : item.mX;

        Piece base;
        base.mTile = item.mTile;
        base.mTileId = item.mTile != nullptr ? item.mTile->uid() : 0;
        base.mColor = item.mColor;
        base.mTexture = texture;
        base.mMirrored = mirrored;
        if (item.mClipped)
        {
            base.mClipped = true;
            base.mClipX = roundToInt(item.mClipX * scale);
            base.mClipY = roundToInt(item.mClipY * scale);
            base.mClipWidth = roundToInt((item.mClipX + item.mClipWidth) * scale) - base.mClipX;
            base.mClipHeight = roundToInt((item.mClipY + item.mClipHeight) * scale) - base.mClipY;
        }

        // A picture that repeats is cut into as many as it takes (the last one cut short), each at its natural size
        const float tileWidth = item.mRepeatX ? sourceWidth : width;
        const float tileHeight = item.mRepeatY ? sourceHeight : height;
        const int columns = std::min(256, std::max(1, static_cast<int>(std::ceil(width / tileWidth - 1e-4f))));
        const int rows = std::min(256, std::max(1, static_cast<int>(std::ceil(height / tileHeight - 1e-4f))));
        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                const float partX = static_cast<float>(column) * tileWidth;
                const float partY = static_cast<float>(row) * tileHeight;
                const float partWidth = std::min(tileWidth, width - partX);
                const float partHeight = std::min(tileHeight, height - partY);
                if (partWidth <= 0 || partHeight <= 0)
                    continue;
                Piece piece = base;
                piece.mX = roundToInt((left + partX) * scale);
                piece.mY = roundToInt((item.mY + partY) * scale);
                piece.mWidth = roundToInt((left + partX + partWidth) * scale) - piece.mX;
                piece.mHeight = roundToInt((item.mY + partY + partHeight) * scale) - piece.mY;
                piece.mSourceX = roundToInt(sourceX);
                piece.mSourceY = roundToInt(sourceY);
                piece.mSourceWidth = std::max(1, roundToInt(sourceWidth * partWidth / tileWidth));
                piece.mSourceHeight = std::max(1, roundToInt(sourceHeight * partHeight / tileHeight));
                if (piece.mWidth > 0 && piece.mHeight > 0)
                    out.push_back(std::move(piece));
            }
        }
    }

    void OpenMenu::piecesForText(const DrawItem& item, std::vector<Piece>& out)
    {
        Resources& resources = *mResources;
        const Font* font = resources.fonts().get(item.mFont);
        if (font == nullptr || item.mText.empty())
            return;
        const std::string texture = "textures/fonts/" + lowerSlash(font->textureName()) + ".dds";
        const std::optional<TextSize> size = resources.textureSize(texture);
        if (!size)
            return;
        const float scale = this->scale();
        const Font::Layout layout = font->layout(item.mText, item.mWrapWidth, item.mJustify, item.mWidth);

        Piece base;
        base.mTile = item.mTile;
        base.mTileId = item.mTile != nullptr ? item.mTile->uid() : 0;
        base.mColor = item.mColor;
        base.mTexture = texture;
        base.mGlyph = true;
        if (item.mClipped)
        {
            base.mClipped = true;
            base.mClipX = roundToInt(item.mClipX * scale);
            base.mClipY = roundToInt(item.mClipY * scale);
            base.mClipWidth = roundToInt((item.mClipX + item.mClipWidth) * scale) - base.mClipX;
            base.mClipHeight = roundToInt((item.mClipY + item.mClipHeight) * scale) - base.mClipY;
        }
        for (const Font::Quad& quad : layout.mQuads)
        {
            if (quad.mWidth <= 0 || quad.mHeight <= 0)
                continue;
            Piece piece = base;
            piece.mX = roundToInt((item.mX + quad.mX) * scale);
            piece.mY = roundToInt((item.mY + quad.mY) * scale);
            piece.mWidth = roundToInt((item.mX + quad.mX + quad.mWidth) * scale) - piece.mX;
            piece.mHeight = roundToInt((item.mY + quad.mY + quad.mHeight) * scale) - piece.mY;
            piece.mSourceX = roundToInt(quad.mU0 * size->mWidth);
            piece.mSourceY = roundToInt(quad.mV0 * size->mHeight);
            piece.mSourceWidth = roundToInt((quad.mU1 - quad.mU0) * size->mWidth);
            piece.mSourceHeight = roundToInt((quad.mV1 - quad.mV0) * size->mHeight);
            if (piece.mWidth > 0 && piece.mHeight > 0 && piece.mSourceWidth > 0 && piece.mSourceHeight > 0)
                out.push_back(std::move(piece));
        }
    }

    int OpenMenu::addList(std::string_view listTile, std::string_view templateName)
    {
        // The list is the tile of that name that has a scrollbar in it: a menu may name a heading the same
        Tile* tile = nullptr;
        const std::string wanted = Xml::lower(listTile);
        for (const Tile* candidate : mMenu->tiles())
        {
            if (Xml::lower(candidate->name()) != wanted)
                continue;
            Tile* found = mMenu->byId(candidate->uid());
            if (found != nullptr && mMenu->find(*found, "lb_scrollbar") != nullptr)
            {
                tile = found;
                break;
            }
        }
        if (tile == nullptr)
            tile = mMenu->find(listTile);
        if (tile == nullptr || !mMenu->hasTemplate(templateName))
            return -1;
        mLists.push_back(std::make_unique<ListBox>(*this, tile->uid(), std::string(templateName)));
        return static_cast<int>(mLists.size()) - 1;
    }

    ListBox* OpenMenu::list(int number)
    {
        if (number < 0 || number >= static_cast<int>(mLists.size()))
            return nullptr;
        return mLists[number].get();
    }

    std::optional<std::pair<int, int>> OpenMenu::rowOf(const Tile& tile)
    {
        for (int number = 0; number < static_cast<int>(mLists.size()); ++number)
        {
            const int row = mLists[number]->rowOf(tile);
            if (row >= 0)
                return std::make_pair(number, row);
        }
        return std::nullopt;
    }

    void OpenMenu::layoutLists()
    {
        for (const std::unique_ptr<ListBox>& list : mLists)
            list->layout();
    }

    std::vector<Piece> OpenMenu::pieces()
    {
        layoutLists();
        std::vector<Piece> result;
        for (const DrawItem& item : mMenu->draw())
        {
            if (item.mColor[3] <= 0)
                continue;
            if (item.mType == TileType::Text)
                piecesForText(item, result);
            else
                pieceForImage(item, result);
        }
        return result;
    }

    std::array<float, 2> OpenMenu::localPoint(const Tile& tile, float designX, float designY)
    {
        float originX = 0;
        float originY = 0;
        if (Tile* parent = const_cast<Tile&>(tile).parent())
            if (const std::optional<std::array<float, 4>> rect = mMenu->rectangle(*parent))
            {
                originX = (*rect)[0];
                originY = (*rect)[1];
            }
        return { designX - originX, designY - originY };
    }

    Tile* OpenMenu::pointerMoved(float x, float y)
    {
        const float scale = this->scale();
        const float designX = x / scale;
        const float designY = y / scale;
        Tile* over = mMenu->tileAt(designX, designY);
        const int overId = over != nullptr ? over->uid() : 0;
        if (overId != mHover)
        {
            if (Tile* old = mMenu->byId(mHover))
                mMenu->set(*old, "mouseover", 0.f);
            if (over != nullptr)
            {
                mMenu->set(*over, "mouseover", 1.f);
                // The highlight of a list goes to the row the pointer comes to
                if (const std::optional<std::pair<int, int>> row = rowOf(*over))
                    mLists[row->first]->select(row->second);
            }
            mHover = overId;
        }
        if (Tile* dragged = mMenu->byId(mDragged))
        {
            const std::array<float, 2> point = localPoint(*dragged, designX, designY);
            mMenu->set(*dragged, "dragx", point[0]);
            mMenu->set(*dragged, "dragy", point[1]);
        }
        return over;
    }

    Tile* OpenMenu::pointerPressed(float x, float y)
    {
        const float scale = this->scale();
        const float designX = x / scale;
        const float designY = y / scale;
        pointerMoved(x, y);
        Tile* tile = mMenu->tileAt(designX, designY);
        mPressed = tile != nullptr ? tile->uid() : 0;
        if (tile != nullptr && mMenu->number(*tile, "draggable") != 0)
        {
            mDragged = tile->uid();
            const std::array<float, 2> point = localPoint(*tile, designX, designY);
            const std::optional<std::array<float, 4>> rect = mMenu->rectangle(*tile);
            mMenu->set(*tile, "dragstartx", point[0]);
            mMenu->set(*tile, "dragstarty", point[1]);
            mMenu->set(*tile, "dragx", point[0]);
            mMenu->set(*tile, "dragy", point[1]);
            mMenu->set(*tile, "dragoffsetx", rect ? designX - (*rect)[0] : 0.f);
            mMenu->set(*tile, "dragoffsety", rect ? designY - (*rect)[1] : 0.f);
        }
        return tile;
    }

    Tile* OpenMenu::pointerReleased(float x, float y)
    {
        const float scale = this->scale();
        Tile* tile = mMenu->tileAt(x / scale, y / scale);
        Tile* pressed = mMenu->byId(mPressed);
        if (Tile* dragged = mMenu->byId(mDragged))
        {
            const std::vector<std::pair<int, float>> values = scrollbarValues();
            mMenu->set(*dragged, "dragstartx", -1.f);
            mMenu->set(*dragged, "dragstarty", -1.f);
            restoreScrollbars(values);
        }
        mDragged = 0;
        mPressed = 0;
        if (tile != nullptr && tile == pressed)
        {
            click(*tile);
            return tile;
        }
        return nullptr;
    }

    Tile* OpenMenu::wheelMoved(float x, float y, int notches)
    {
        const float scale = this->scale();
        Tile* tile = mMenu->wheelTileAt(x / scale, y / scale);
        if (tile != nullptr)
        {
            mMenu->set(*tile, "wheelmoved", mMenu->number(*tile, "wheelmoved") + static_cast<float>(notches));
            // The scrollbars have counted the turn in their value; it is kept there and not in the count of the wheel
            if (!mScrollbars.empty())
            {
                const std::vector<std::pair<int, float>> values = scrollbarValues();
                mMenu->set(*tile, "wheelmoved", 0.f);
                restoreScrollbars(values);
            }
        }
        return tile;
    }

    void OpenMenu::click(Tile& tile)
    {
        mMenu->set(tile, "clicked", mMenu->number(tile, "clicked") + 1.f);
    }

    void OpenMenu::findScrollbars()
    {
        mScrollbars.clear();
        for (Tile* tile : mMenu->tiles())
            if (mMenu->defines(*tile, "_current_value") && mMenu->defines(*tile, "_number_of_items"))
                mScrollbars.push_back(tile->uid());
    }

    std::vector<std::pair<int, float>> OpenMenu::scrollbarValues()
    {
        std::vector<std::pair<int, float>> values;
        for (int id : mScrollbars)
            if (Tile* bar = mMenu->byId(id))
                values.emplace_back(id, mMenu->number(*bar, "_current_value"));
        return values;
    }

    void OpenMenu::restoreScrollbars(const std::vector<std::pair<int, float>>& values)
    {
        for (const auto& [id, value] : values)
            if (Tile* bar = mMenu->byId(id))
                setScrollbar(*bar, value);
    }

    void OpenMenu::setScrollbar(Tile& bar, float value)
    {
        auto endsWith = [](const std::string& text, std::string_view ending) {
            return text.size() >= ending.size()
                && text.compare(text.size() - ending.size(), ending.size(), ending) == 0;
        };
        Tile* forward = nullptr;
        Tile* back = nullptr;
        for (const std::unique_ptr<Tile>& child : bar.children())
        {
            const std::string name = Xml::lower(child->name());
            if (endsWith(name, "_right") || endsWith(name, "_down"))
                forward = child.get();
            else if (endsWith(name, "_left") || endsWith(name, "_up"))
                back = child.get();
            else if (name.find("_page_") != std::string::npos)
                mMenu->set(*child, "clicked", 0.f);
        }
        if (forward == nullptr || back == nullptr)
            return;
        float step = mMenu->number(bar, "_step_size");
        if (step == 0)
            step = 1;
        mMenu->set(*forward, "clicked", std::max(0.f, std::floor(value / step + 0.5f)));
        mMenu->set(*back, "clicked", 0.f);
        // The marker stands by the value, not where it was let go, once something has been poked
        mMenu->set(bar, "_SetInCode", mMenu->number(bar, "_SetInCode") + 1.f);
    }

    void OpenMenu::forget(const Tile& tile)
    {
        // The pointer lets go of a tile that is about to be gone, and of the ones in it
        std::vector<const Tile*> stack{ &tile };
        while (!stack.empty())
        {
            const Tile* current = stack.back();
            stack.pop_back();
            if (current->uid() == mHover)
                mHover = 0;
            if (current->uid() == mPressed)
                mPressed = 0;
            if (current->uid() == mDragged)
                mDragged = 0;
            for (const std::unique_ptr<Tile>& child : current->children())
                stack.push_back(child.get());
        }
    }

    void OpenMenu::remove(Tile& tile)
    {
        forget(tile);
        mMenu->remove(tile);
    }

    void OpenMenu::removeChildren(Tile& tile)
    {
        for (const std::unique_ptr<Tile>& child : tile.children())
            forget(*child);
        mMenu->removeChildren(tile);
    }
}

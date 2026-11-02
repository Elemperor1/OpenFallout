#include "menu.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutMenu
{
    namespace
    {
        std::array<float, 4> intersect(const std::array<float, 4>& a, const std::array<float, 4>& b)
        {
            const float x0 = std::max(a[0], b[0]);
            const float y0 = std::max(a[1], b[1]);
            const float x1 = std::min(a[0] + a[2], b[0] + b[2]);
            const float y1 = std::min(a[1] + a[3], b[1] + b[3]);
            return { x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0) };
        }

        bool contains(const std::array<float, 4>& rect, float x, float y)
        {
            return x >= rect[0] && y >= rect[1] && x < rect[0] + rect[2] && y < rect[1] + rect[3];
        }
    }

    Menu::Menu(std::shared_ptr<const TileDef> root, std::shared_ptr<const TileDef> globals,
        std::unordered_map<std::string, std::shared_ptr<const TileDef>> templates, Environment environment)
        : mEnvironment(std::move(environment))
        , mTemplates(std::move(templates))
    {
        if (!root)
            root = std::make_shared<TileDef>();
        mRoot = build(root, nullptr, true);
        mScreen = std::make_unique<Tile>();
        mScreen->mType = TileType::Plain;
        mScreen->mName = "screen";
        mScreen->mNameLower = "screen";
        mGlobals = globals ? build(globals, nullptr, true) : std::make_unique<Tile>();
        mGlobals->mType = TileType::Plain;
        setScreen(1280, 960);
    }

    void Menu::setScreen(float pixelWidth, float pixelHeight)
    {
        if (pixelWidth <= 0 || pixelHeight <= 0)
            return;
        if (pixelWidth >= pixelHeight)
        {
            mDesignHeight = 960;
            mDesignWidth = 960 * pixelWidth / pixelHeight;
        }
        else
        {
            mDesignWidth = 1280;
            mDesignHeight = 1280 * pixelHeight / pixelWidth;
        }
        set(*mScreen, "width", mDesignWidth);
        set(*mScreen, "height", mDesignHeight);
        set(*mScreen, "resolutionconverter", 960.f / pixelHeight);
        set(*mScreen, "cropx", 15);
        set(*mScreen, "cropy", 15);
    }

    std::unique_ptr<Tile> Menu::build(const std::shared_ptr<const TileDef>& definition, Tile* parent, bool owned)
    {
        auto tile = std::make_unique<Tile>();
        tile->mType = definition->mType;
        tile->mName = definition->mName;
        tile->mNameLower = Xml::lower(definition->mName);
        tile->mId = mNextId++;
        mById[tile->mId] = tile.get();
        tile->mDefinition = definition.get();
        if (owned)
            tile->mOwnedDefinition = definition;
        tile->mParent = parent;
        for (const std::shared_ptr<const TileDef>& child : definition->mChildren)
        {
            // Templates are not tiles of the menu: the screen makes tiles from them when it needs them
            if (child->mType == TileType::Template)
                continue;
            std::unique_ptr<Tile> built = build(child, tile.get(), false);
            tile->mChildren.push_back(std::move(built));
        }
        return tile;
    }

    Value Menu::get(const Tile& tile, std::string_view trait)
    {
        const std::string key = Xml::lower(trait);
        Tile::Cached& entry = tile.mCache[key];
        if (entry.mGeneration == mGeneration)
            return entry.mValue;
        if (entry.mBusy)
            return Value();
        entry.mBusy = true;
        Value value = compute(tile, key);
        // The entry may have moved if the calculation put more entries in the map, but a node of the map stays where it
        // is
        entry.mBusy = false;
        entry.mValue = value;
        entry.mGeneration = mGeneration;
        return value;
    }

    void Menu::set(Tile& tile, std::string_view trait, Value value)
    {
        const std::string key = Xml::lower(trait);
        const auto found = tile.mSet.find(key);
        // Setting what is already set changes nothing, so a screen that draws when the generation moves does not
        if (found != tile.mSet.end() && found->second == value)
            return;
        tile.mSet[key] = std::move(value);
        ++mGeneration;
    }

    void Menu::clear(Tile& tile, std::string_view trait)
    {
        if (tile.mSet.erase(Xml::lower(trait)) > 0)
            ++mGeneration;
    }

    bool Menu::isSet(const Tile& tile, std::string_view trait) const
    {
        return tile.mSet.count(Xml::lower(trait)) > 0;
    }

    bool Menu::defines(const Tile& tile, std::string_view trait) const
    {
        const std::string key = Xml::lower(trait);
        if (tile.mSet.count(key) > 0)
            return true;
        return tile.mDefinition != nullptr && tile.mDefinition->mTraits.count(key) > 0;
    }

    Value Menu::compute(const Tile& tile, const std::string& key)
    {
        const auto set = tile.mSet.find(key);
        if (set != tile.mSet.end())
            return set->second;
        if (tile.mDefinition != nullptr)
        {
            const auto found = tile.mDefinition->mTraits.find(key);
            if (found != tile.mDefinition->mTraits.end())
            {
                if (found->second.mExpression)
                    return evaluate(tile, *found->second.mExpression);
                return found->second.mLiteral;
            }
        }
        return defaultOf(tile, key);
    }

    Value Menu::defaultOf(const Tile& tile, const std::string& key)
    {
        if (key == "visible")
            return 1.f;
        if (key == "alpha" || key == "brightness" || key == "red" || key == "green" || key == "blue")
            return 255.f;
        if (key == "font" || key == "justify")
            return 1.f;
        if (key == "zoom")
            return 100.f;
        if (key == "string" || key == "filename" || key == "texatlas")
            return Value::emptyText();
        if (key == "childcount" || key == "child_count")
            return static_cast<float>(tile.mChildren.size());
        if (key == "width" || key == "height")
        {
            const bool isWidth = key == "width";
            if (tile.mType == TileType::Text)
            {
                const std::string text = get(tile, "string").text();
                if (text.empty())
                    return 0.f;
                const int font = static_cast<int>(get(tile, "font").number());
                const float wrap = get(tile, "wrapwidth").number();
                TextSize size{ 8.f * static_cast<float>(text.size()), 16.f };
                if (mEnvironment.mTextSize)
                    size = mEnvironment.mTextSize(font, text, wrap > 0 ? wrap : 0.f);
                return isWidth ? size.mWidth : size.mHeight;
            }
            if (tile.mType == TileType::Image)
            {
                const std::string file = get(tile, "filename").text();
                if (file.empty() || !mEnvironment.mImageSize)
                    return 0.f;
                const std::optional<TextSize> size = mEnvironment.mImageSize(file, get(tile, "texatlas").text());
                if (!size)
                    return 0.f;
                float zoom = get(tile, "zoom").number();
                if (zoom < 0)
                    zoom = 100.f;
                return (isWidth ? size->mWidth : size->mHeight) * zoom / 100.f;
            }
            return 0.f;
        }
        if (key == "filewidth" || key == "fileheight")
        {
            const std::string file = get(tile, "filename").text();
            if (file.empty() || !mEnvironment.mImageSize)
                return 0.f;
            const std::optional<TextSize> size = mEnvironment.mImageSize(file, get(tile, "texatlas").text());
            if (!size)
                return 0.f;
            return key == "filewidth" ? size->mWidth : size->mHeight;
        }
        return Value();
    }

    Value Menu::evaluateOperand(const Tile& tile, const Operand& operand, const Value& running)
    {
        switch (operand.mKind)
        {
            case Operand::Kind::None:
                return Value();
            case Operand::Kind::Literal:
                return operand.mLiteral;
            case Operand::Kind::Nested:
                return evaluate(tile, *operand.mNested);
            case Operand::Kind::Trait:
            {
                const Tile* target = resolve(tile, operand.mSource, operand.mSourceName);
                if (target == nullptr)
                    return Value();
                std::string name = operand.mTrait;
                // A name that ends with `_` is completed with the value so far (`_filename_` and 3 name `_filename_3`)
                if (!name.empty() && name.back() == '_' && !running.isText())
                    name += std::to_string(static_cast<long long>(std::lround(running.number())));
                return get(*target, name);
            }
        }
        return Value();
    }

    Value Menu::evaluate(const Tile& tile, const Expression& expression)
    {
        Value acc;
        for (const Step& step : expression.mSteps)
        {
            const Value operand = evaluateOperand(tile, step.mOperand, acc);
            const float a = acc.number();
            const float b = operand.number();
            switch (step.mOp)
            {
                case Op::Copy:
                case Op::Ref:
                    acc = operand;
                    break;
                case Op::Add:
                    acc = a + b;
                    break;
                case Op::Sub:
                    acc = a - b;
                    break;
                case Op::Mul:
                    acc = a * b;
                    break;
                case Op::Div:
                    acc = b == 0.f ? 0.f : a / b;
                    break;
                case Op::Mod:
                    acc = b == 0.f ? 0.f : std::fmod(a, b);
                    break;
                case Op::Min:
                    acc = std::min(a, b);
                    break;
                case Op::Max:
                    acc = std::max(a, b);
                    break;
                case Op::Abs:
                    acc = std::fabs(a);
                    break;
                case Op::Floor:
                    acc = std::floor(a);
                    break;
                case Op::Ceil:
                    acc = std::ceil(a);
                    break;
                case Op::Round:
                    acc = std::round(a);
                    break;
                case Op::Trunc:
                    acc = std::trunc(a);
                    break;
                case Op::Eq:
                case Op::Neq:
                {
                    bool same;
                    if (acc.isText() || operand.isText())
                        same = acc.text() == operand.text();
                    else
                        same = a == b;
                    acc = (same == (step.mOp == Op::Eq)) ? 1.f : 0.f;
                    break;
                }
                case Op::Gt:
                    acc = a > b ? 1.f : 0.f;
                    break;
                case Op::Gte:
                    acc = a >= b ? 1.f : 0.f;
                    break;
                case Op::Lt:
                    acc = a < b ? 1.f : 0.f;
                    break;
                case Op::Lte:
                    acc = a <= b ? 1.f : 0.f;
                    break;
                case Op::And:
                    acc = (acc.truth() && operand.truth()) ? 1.f : 0.f;
                    break;
                case Op::Or:
                    acc = (acc.truth() || operand.truth()) ? 1.f : 0.f;
                    break;
                case Op::Not:
                    acc = (step.mOperand.mKind == Operand::Kind::None ? !acc.truth() : !operand.truth()) ? 1.f : 0.f;
                    break;
                case Op::OnlyIf:
                    if (!operand.truth())
                        acc = Value(0.f);
                    break;
                case Op::OnlyIfNot:
                    if (operand.truth())
                        acc = Value(0.f);
                    break;
            }
        }
        return acc;
    }

    Tile* Menu::findIn(Tile& under, const std::string& lowerName, const Tile* skip)
    {
        if (&under == skip)
            return nullptr;
        if (under.mNameLower == lowerName)
            return &under;
        for (const std::unique_ptr<Tile>& child : under.mChildren)
            if (Tile* found = findIn(*child, lowerName, skip))
                return found;
        return nullptr;
    }

    Tile* Menu::resolve(const Tile& me, Source source, const std::string& name)
    {
        Tile& self = const_cast<Tile&>(me);
        const std::string wanted = Xml::lower(name);
        switch (source)
        {
            case Source::Me:
                return &self;
            case Source::Parent:
                return self.mParent;
            case Source::GrandParent:
                return self.mParent != nullptr ? self.mParent->mParent : nullptr;
            case Source::Child:
            {
                for (const std::unique_ptr<Tile>& child : self.mChildren)
                    if (child->mNameLower == wanted)
                        return child.get();
                // The files name a tile further down now and then
                for (const std::unique_ptr<Tile>& child : self.mChildren)
                    if (Tile* found = findIn(*child, wanted, nullptr))
                        return found;
                return nullptr;
            }
            case Source::Sibling:
            {
                if (self.mParent == nullptr)
                    return nullptr;
                for (const std::unique_ptr<Tile>& sibling : self.mParent->mChildren)
                    if (sibling->mNameLower == wanted)
                        return sibling.get();
                return nullptr;
            }
            case Source::Screen:
                return mScreen.get();
            case Source::Globals:
                return mGlobals.get();
            case Source::Io:
                return mRoot.get();
            case Source::Named:
            {
                // The nearest one: the tile and what is under it, then the tile it is in and what is under that, ...
                const Tile* skip = nullptr;
                for (Tile* scope = &self; scope != nullptr; scope = scope->mParent)
                {
                    if (Tile* found = findIn(*scope, wanted, skip))
                        return found;
                    skip = scope;
                }
                return nullptr;
            }
        }
        return nullptr;
    }

    Tile* Menu::find(std::string_view name)
    {
        return findIn(*mRoot, Xml::lower(name), nullptr);
    }

    Tile* Menu::find(Tile& under, std::string_view name)
    {
        return findIn(under, Xml::lower(name), nullptr);
    }

    Tile* Menu::byId(int id)
    {
        const auto found = mById.find(id);
        return found == mById.end() ? nullptr : found->second;
    }

    void Menu::unindex(Tile& tile)
    {
        mById.erase(tile.mId);
        for (const std::unique_ptr<Tile>& child : tile.mChildren)
            unindex(*child);
    }

    std::vector<Tile*> Menu::tiles()
    {
        std::vector<Tile*> result;
        std::vector<Tile*> stack{ mRoot.get() };
        while (!stack.empty())
        {
            Tile* tile = stack.back();
            stack.pop_back();
            result.push_back(tile);
            for (auto it = tile->mChildren.rbegin(); it != tile->mChildren.rend(); ++it)
                stack.push_back(it->get());
        }
        return result;
    }

    Tile* Menu::instantiate(Tile& parent, std::string_view templateName, std::string_view name)
    {
        const auto found = mTemplates.find(Xml::lower(templateName));
        if (found == mTemplates.end())
        {
            mProblems.push_back("no template " + std::string(templateName));
            return nullptr;
        }
        // The template is a tile of its own kind that the menu does not show; what is made from it is a rect
        // with the same traits and children
        auto copy = std::make_shared<TileDef>(*found->second);
        copy->mType = TileType::Rect;
        copy->mName = name.empty() ? found->second->mName : std::string(name);
        std::unique_ptr<Tile> tile = build(copy, &parent, true);
        Tile* result = tile.get();
        parent.mChildren.push_back(std::move(tile));
        ++mGeneration;
        return result;
    }

    void Menu::remove(Tile& tile)
    {
        if (tile.mParent == nullptr)
            return;
        unindex(tile);
        auto& siblings = tile.mParent->mChildren;
        siblings.erase(std::remove_if(siblings.begin(), siblings.end(),
                           [&tile](const std::unique_ptr<Tile>& other) { return other.get() == &tile; }),
            siblings.end());
        ++mGeneration;
    }

    void Menu::removeChildren(Tile& tile)
    {
        for (const std::unique_ptr<Tile>& child : tile.mChildren)
            unindex(*child);
        tile.mChildren.clear();
        ++mGeneration;
    }

    std::optional<std::pair<Tile*, std::string>> Menu::reference(Tile& tile, std::string_view trait)
    {
        const std::string key = Xml::lower(trait);
        const TileDef* definition = tile.mDefinition;
        if (definition == nullptr)
            return std::nullopt;
        const auto found = definition->mTraits.find(key);
        if (found == definition->mTraits.end() || !found->second.mExpression)
            return std::nullopt;
        for (const Step& step : found->second.mExpression->mSteps)
        {
            if (step.mOp == Op::Ref && step.mOperand.mKind == Operand::Kind::Trait)
            {
                Tile* target = resolve(tile, step.mOperand.mSource, step.mOperand.mSourceName);
                if (target != nullptr)
                    return std::make_pair(target, step.mOperand.mTrait);
            }
        }
        return std::nullopt;
    }

    bool Menu::visible(const Tile& tile)
    {
        return get(tile, "visible").truth();
    }

    bool Menu::mirrorsChild(const Tile& tile) const
    {
        if (tile.mType != TileType::HotRect || tile.mDefinition == nullptr)
            return false;
        // `x` and `y` that are nothing but the same trait of one of its children (what `super_text_box` does)
        const auto mirrors = [&](const char* trait) {
            if (tile.mSet.count(trait) > 0)
                return false;
            const auto found = tile.mDefinition->mTraits.find(trait);
            if (found == tile.mDefinition->mTraits.end() || !found->second.mExpression)
                return false;
            const std::vector<Step>& steps = found->second.mExpression->mSteps;
            return steps.size() == 1 && steps[0].mOp == Op::Copy && steps[0].mOperand.mKind == Operand::Kind::Trait
                && steps[0].mOperand.mSource == Source::Child && Xml::lower(steps[0].mOperand.mTrait) == trait;
        };
        return mirrors("x") && mirrors("y");
    }

    float Menu::anchorShift(Tile& tile)
    {
        if (tile.mType != TileType::Text)
            return 0;
        // A text is placed by the point it is justified to: its left end, its middle or its right end
        const int justify = static_cast<int>(std::lround(number(tile, "justify")));
        const float width = number(tile, "width");
        return justify == 2 ? width / 2 : justify == 4 ? width : 0.f;
    }

    std::optional<std::array<float, 4>> Menu::rectangle(Tile& tile)
    {
        float x = 0;
        float y = 0;
        for (Tile* t = &tile; t != nullptr; t = t->mParent)
        {
            // A tile that mirrors its child does not move the child's frame, so the child stands where it says
            if (t != &tile && t->mParent != nullptr && mirrorsChild(*t))
                continue;
            // A text puts its x at the point it is justified to, so what is in it moves with it
            x += number(*t, "x") - anchorShift(*t);
            y += number(*t, "y");
        }
        return std::array<float, 4>{ x, y, number(tile, "width"), number(tile, "height") };
    }

    std::array<float, 4> Menu::colorOf(const Tile& tile)
    {
        float r = number(tile, "red");
        float g = number(tile, "green");
        float b = number(tile, "blue");
        for (const Tile* t = &tile; t != nullptr; t = t->mParent)
        {
            if (!defines(*t, "systemcolor"))
                continue;
            const int id = static_cast<int>(std::lround(number(*t, "systemcolor")));
            if (id > 0 && mEnvironment.mSystemColor)
            {
                if (const auto color = mEnvironment.mSystemColor(id))
                {
                    r = (*color)[0];
                    g = (*color)[1];
                    b = (*color)[2];
                }
            }
            break;
        }
        const float brightness = number(tile, "brightness") / 255.f;
        const float alpha = number(tile, "alpha") / 255.f;
        const auto clamp = [](float v) { return std::clamp(v, 0.f, 1.f); };
        return { clamp(r / 255.f * brightness), clamp(g / 255.f * brightness), clamp(b / 255.f * brightness),
            clamp(alpha) };
    }

    void Menu::collect(Tile& tile, float originX, float originY, const std::optional<std::array<float, 4>>& clip,
        std::vector<DrawItem>& out)
    {
        if (!visible(tile))
            return;
        const float x = originX + number(tile, "x") - anchorShift(tile);
        const float y = originY + number(tile, "y");
        const float width = number(tile, "width");
        const float height = number(tile, "height");
        // Where the tile's children start: a tile that mirrors its child does not move the frame
        const float frameX = mirrorsChild(tile) ? originX : x;
        const float frameY = mirrorsChild(tile) ? originY : y;

        const bool isImage = tile.mType == TileType::Image;
        const bool isText = tile.mType == TileType::Text;
        if (isImage || isText)
        {
            DrawItem item;
            item.mTile = &tile;
            item.mType = tile.mType;
            item.mX = x;
            item.mY = y;
            item.mWidth = width;
            item.mHeight = height;
            item.mColor = colorOf(tile);
            if (isImage)
            {
                item.mFile = text(tile, "filename");
                item.mAtlas = text(tile, "texatlas");
                item.mRepeatX = number(tile, "repeathorizontal") != 0 || get(tile, "tile").number() > 0;
                item.mRepeatY = number(tile, "repeatvertical") != 0 || get(tile, "tile").number() > 0;
                item.mCropX = number(tile, "cropx");
                item.mCropY = number(tile, "cropy");
            }
            else
            {
                item.mText = text(tile, "string");
                item.mFont = static_cast<int>(number(tile, "font"));
                item.mJustify = static_cast<int>(number(tile, "justify"));
                item.mWrapWidth = number(tile, "wrapwidth");
            }
            if (number(tile, "clips") != 0 && clip)
            {
                item.mClipped = true;
                item.mClipX = (*clip)[0];
                item.mClipY = (*clip)[1];
                item.mClipWidth = (*clip)[2];
                item.mClipHeight = (*clip)[3];
            }
            const bool empty = isImage ? item.mFile.empty() : item.mText.empty();
            if (!empty)
                out.push_back(std::move(item));
        }

        std::optional<std::array<float, 4>> childClip = clip;
        if (number(tile, "clipwindow") != 0)
        {
            const std::array<float, 4> own{ x, y, width, height };
            childClip = clip ? intersect(*clip, own) : own;
        }

        std::vector<std::pair<float, Tile*>> ordered;
        ordered.reserve(tile.mChildren.size());
        for (const std::unique_ptr<Tile>& child : tile.mChildren)
            ordered.emplace_back(number(*child, "depth"), child.get());
        std::stable_sort(ordered.begin(), ordered.end(),
            [](const std::pair<float, Tile*>& a, const std::pair<float, Tile*>& b) { return a.first < b.first; });
        for (const auto& entry : ordered)
            collect(*entry.second, frameX, frameY, childClip, out);
    }

    std::vector<DrawItem> Menu::draw()
    {
        std::vector<DrawItem> out;
        collect(*mRoot, 0, 0, std::nullopt, out);
        return out;
    }

    void Menu::hitTest(Tile& tile, float originX, float originY, float px, float py, bool wheel, Tile*& best,
        std::optional<std::array<float, 4>> clip)
    {
        if (!visible(tile))
            return;
        const float x = originX + number(tile, "x") - anchorShift(tile);
        const float y = originY + number(tile, "y");
        const float width = number(tile, "width");
        const float height = number(tile, "height");
        const float frameX = mirrorsChild(tile) ? originX : x;
        const float frameY = mirrorsChild(tile) ? originY : y;
        // A tile that is a `target` takes the pointer (a marker that is dragged has only that; `locus` is on tiles that
        // are no buttons at all)
        const bool wanted = wheel ? number(tile, "wheelable") != 0 : number(tile, "target") != 0;
        if (wanted && width > 0 && height > 0)
        {
            const std::array<float, 4> rect{ x, y, width, height };
            const bool clipped = number(tile, "clips") != 0 && clip;
            if (contains(rect, px, py) && (!clipped || contains(*clip, px, py)))
                best = &tile;
        }
        std::optional<std::array<float, 4>> childClip = clip;
        if (number(tile, "clipwindow") != 0)
        {
            const std::array<float, 4> own{ x, y, width, height };
            childClip = clip ? intersect(*clip, own) : own;
        }
        std::vector<std::pair<float, Tile*>> ordered;
        for (const std::unique_ptr<Tile>& child : tile.mChildren)
            ordered.emplace_back(number(*child, "depth"), child.get());
        std::stable_sort(ordered.begin(), ordered.end(),
            [](const std::pair<float, Tile*>& a, const std::pair<float, Tile*>& b) { return a.first < b.first; });
        for (const auto& entry : ordered)
            hitTest(*entry.second, frameX, frameY, px, py, wheel, best, childClip);
    }

    Tile* Menu::tileAt(float x, float y)
    {
        Tile* best = nullptr;
        hitTest(*mRoot, 0, 0, x, y, false, best, std::nullopt);
        return best;
    }

    Tile* Menu::wheelTileAt(float x, float y)
    {
        Tile* best = nullptr;
        hitTest(*mRoot, 0, 0, x, y, true, best, std::nullopt);
        return best;
    }
}

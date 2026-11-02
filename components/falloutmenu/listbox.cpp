#include "listbox.hpp"

#include <algorithm>
#include <cmath>

#include "openmenu.hpp"
#include "xml.hpp"

namespace FalloutMenu
{
    ListBox::ListBox(OpenMenu& open, int listTile, std::string templateName)
        : mOpen(open)
        , mList(listTile)
        , mTemplate(std::move(templateName))
    {
        Menu& menu = mOpen.menu();
        if (Tile* tile = list())
        {
            mFileDelta = menu.number(*tile, "_scroll_delta");
            if (mFileDelta <= 0)
                mFileDelta = 26.f;
            mFileHeight = menu.number(*tile, "height");
        }
    }

    Tile* ListBox::list()
    {
        return mOpen.menu().byId(mList);
    }

    void ListBox::setRows(const std::vector<std::map<std::string, Value>>& rows)
    {
        Menu& menu = mOpen.menu();
        Tile* tile = list();
        if (tile == nullptr)
            return;
        for (int id : mItems)
            if (Tile* old = menu.byId(id))
                mOpen.remove(*old);
        mItems.clear();
        mSelected = -1;
        int index = 0;
        for (const auto& row : rows)
        {
            // A row may name the template it is made from (`_template`): the save list of the start menu has a row for
            // a new save, which the file makes from another template than the rows for the saves
            const auto named = row.find("_template");
            const std::string& templateName
                = named != row.end() && named->second.isText() ? named->second.text() : mTemplate;
            Tile* item = menu.instantiate(*tile, templateName, {});
            if (item == nullptr)
                break;
            // What the screen does not say, the row has lit: the files leave the alpha of a row's text for the code to
            // set
            menu.set(*item, "_line_alpha", 255.f);
            setTraits(*item, row);
            menu.set(*item, "listindex", static_cast<float>(index++));
            mItems.push_back(item->uid());
        }
        layout();
    }

    void ListBox::setTraits(Tile& item, const std::map<std::string, Value>& traits)
    {
        Menu& menu = mOpen.menu();
        // A trait written `Tile.trait` is set on the tile of that name inside the row (the file's template names the
        // text of a row, and the marker that shows an equipped item)
        for (const auto& [trait, value] : traits)
        {
            if (trait == "_template")
                continue;
            const std::size_t dot = trait.find('.');
            if (dot == std::string::npos)
                menu.set(item, trait, value);
            else if (Tile* inside = menu.find(item, std::string_view(trait).substr(0, dot)))
                menu.set(*inside, std::string_view(trait).substr(dot + 1), value);
        }
    }

    void ListBox::updateRow(int row, const std::map<std::string, Value>& traits)
    {
        Tile* target = item(row);
        if (target == nullptr)
            return;
        setTraits(*target, traits);
        // The row can be higher or lower with its new text: it is measured again, and the rows below it move (the
        // scrolling is left where it is)
        mOpen.menu().clear(*target, "_fitted");
        layout();
    }

    Tile* ListBox::item(int row)
    {
        if (row < 0 || row >= rowCount())
            return nullptr;
        return mOpen.menu().byId(mItems[row]);
    }

    int ListBox::rowOf(const Tile& tile) const
    {
        for (const Tile* current = &tile; current != nullptr; current = current->parent())
            for (int row = 0; row < rowCount(); ++row)
                if (mItems[row] == current->uid())
                    return row;
        return -1;
    }

    void ListBox::select(int row, bool show)
    {
        if (row < -1 || row >= rowCount())
            row = -1;
        mSelected = row;
        if (show && row >= 0)
            reveal(row);
        layout();
    }

    void ListBox::reveal(int row)
    {
        Menu& menu = mOpen.menu();
        Tile* tile = list();
        Tile* bar = tile != nullptr ? menu.find(*tile, "lb_scrollbar") : nullptr;
        Tile* target = item(row);
        if (bar == nullptr || target == nullptr)
            return;
        const float delta = std::max(1.f, menu.number(*tile, "_scroll_delta"));
        const float top = menu.number(*target, "_y");
        const float bottom = top + menu.number(*target, "height");
        const float visible = std::max(1.f, menu.number(*bar, "_number_of_visible_items"));
        float scroll = menu.number(*bar, "_current_value");
        if (top < scroll * delta)
            scroll = std::floor(top / delta);
        else if (bottom > (scroll + visible) * delta)
            scroll = std::ceil(bottom / delta) - visible;
        scroll = std::max(0.f, scroll);
        if (scroll != menu.number(*bar, "_current_value"))
            mOpen.setScrollbar(*bar, scroll);
    }

    void ListBox::layout()
    {
        Menu& menu = mOpen.menu();
        Tile* tile = list();
        if (tile == nullptr)
            return;
        Tile* bar = menu.find(*tile, "lb_scrollbar");

        // A list whose file says how many rows show has rows as high as the first, and is as high as they are
        const float fixedRows = menu.number(*tile, "_number_of_visible_items");
        float delta = mFileDelta;
        float y = 0;
        for (int row = 0; row < rowCount(); ++row)
        {
            Tile* target = item(row);
            if (target == nullptr)
                continue;
            // The height: what the template gives, or the height of its text and the spacing around it
            float height = 0;
            if (menu.isSet(*target, "_fitted"))
            {
                height = menu.number(*target, "height");
            }
            else
            {
                menu.clear(*target, "height");
                height = menu.number(*target, "height");
                if (height <= 0)
                {
                    for (const std::unique_ptr<Tile>& child : target->children())
                    {
                        if (child->type() == TileType::Text)
                        {
                            height = menu.number(*child, "height") + menu.number(*target, "_verticalspacing");
                            break;
                        }
                    }
                }
                menu.set(*target, "height", height);
                menu.set(*target, "_fitted", 1.f);
            }
            if (row == 0 && fixedRows > 0)
                delta = std::max(1.f, height);
            menu.set(*target, "_y", y);
            y += height;
        }
        const float total = y;
        if (fixedRows > 0)
        {
            menu.set(*tile, "_scroll_delta", delta);
            menu.set(*tile, "height", fixedRows * delta);
        }
        const float listHeight = menu.number(*tile, "height");
        if (bar != nullptr)
        {
            menu.set(*bar, "_number_of_items", std::max(1.f, std::ceil(total / delta)));
            menu.set(*bar, "_number_of_visible_items", std::max(1.f, std::floor(listHeight / delta)));
        }
        menu.set(*tile, "_num_filtered", static_cast<float>(rowCount()));

        // The highlight box stands on the picked row, wherever the scrolling has put it
        if (Tile* picked = item(mSelected))
        {
            menu.set(*tile, "_highlight_y", menu.number(*picked, "y"));
            menu.set(*tile, "_selected_height", menu.number(*picked, "height"));
        }
        else
        {
            menu.set(*tile, "_highlight_y", -1.f);
            menu.set(*tile, "_selected_height", 0.f);
        }
    }
}

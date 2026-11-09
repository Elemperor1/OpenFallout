#include "menubindings.hpp"

#include <map>
#include <memory>
#include <stdexcept>

#include <components/falloutmenu/openmenu.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwgui/falloutmenuresources.hpp"

#include "context.hpp"

namespace OFLua
{
    namespace
    {
        struct LuaMenu
        {
            std::unique_ptr<FalloutMenu::OpenMenu> mMenu;
        };

        // A tile is named by its name in the menu file or by the number `find` gave
        FalloutMenu::Tile& tileOf(LuaMenu& self, const sol::object& which)
        {
            FalloutMenu::Tile* tile = nullptr;
            if (which.is<std::string>())
                tile = self.mMenu->menu().find(which.as<std::string>());
            else if (which.is<double>())
                tile = self.mMenu->menu().byId(static_cast<int>(which.as<double>()));
            if (tile == nullptr)
            {
                std::string what = which.is<std::string>() ? which.as<std::string>() : "(not a name or a number)";
                throw std::runtime_error("The menu has no tile " + what);
            }
            return *tile;
        }

        sol::object tileId(sol::this_state lua, const FalloutMenu::Tile* tile)
        {
            if (tile == nullptr)
                return sol::nil;
            return sol::make_object<int>(lua, tile->uid());
        }

        // The traits of a row of a list, from a table of Lua (texts, numbers and booleans; the rest is left out)
        std::map<std::string, FalloutMenu::Value> traitsOf(const sol::table& table)
        {
            std::map<std::string, FalloutMenu::Value> traits;
            for (const auto& [key, value] : table)
            {
                if (!key.is<std::string>())
                    continue;
                if (value.is<std::string>())
                    traits[key.as<std::string>()] = FalloutMenu::Value::fromText(value.as<std::string>());
                else if (value.is<bool>())
                    traits[key.as<std::string>()] = FalloutMenu::Value(value.as<bool>() ? 1.f : 0.f);
                else if (value.is<double>())
                    traits[key.as<std::string>()] = FalloutMenu::Value(static_cast<float>(value.as<double>()));
            }
            return traits;
        }

        sol::object valueObject(sol::this_state lua, const FalloutMenu::Value& value)
        {
            if (value.isText())
                return sol::make_object<std::string>(lua, value.text());
            return sol::make_object<double>(lua, value.number());
        }
    }

    sol::table initCoreMenuBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        sol::usertype<LuaMenu> menuT = lua.new_usertype<LuaMenu>("FalloutMenu");
        menuT[sol::meta_function::to_string] = [](const LuaMenu&) { return "FalloutMenu"; };

        // Everything to draw, back to front, as pictures in pixels of the window: `tile` (the number of the tile),
        // `x`, `y`, `width`, `height`, `mirrored`, `texture` (a path of the data) and the part of it that is drawn
        // (`sourceX`, `sourceY`, `sourceWidth`, `sourceHeight`), `red`, `green`, `blue`, `alpha` (0 to 1), `glyph`
        // (a character of a text) and, for a picture that is cut to a window, `clip` (`x`, `y`, `width`, `height`)
        menuT["pieces"] = [](sol::this_state state, LuaMenu& self) {
            sol::state_view view(state);
            sol::table rows(view, sol::create);
            int index = 1;
            for (const FalloutMenu::Piece& piece : self.mMenu->pieces())
            {
                sol::table row
                    = view.create_table_with("tile", piece.mTileId, "x", piece.mX, "y", piece.mY, "width", piece.mWidth,
                        "height", piece.mHeight, "texture", piece.mTexture, "sourceX", piece.mSourceX, "sourceY",
                        piece.mSourceY, "sourceWidth", piece.mSourceWidth, "sourceHeight", piece.mSourceHeight, "red",
                        piece.mColor[0], "green", piece.mColor[1], "blue", piece.mColor[2], "alpha", piece.mColor[3]);
                if (piece.mMirrored)
                    row["mirrored"] = true;
                if (piece.mGlyph)
                    row["glyph"] = true;
                if (piece.mClipped)
                {
                    sol::table clip = view.create_table_with(
                        "x", piece.mClipX, "y", piece.mClipY, "width", piece.mClipWidth, "height", piece.mClipHeight);
                    row["clip"] = clip;
                }
                rows[index++] = row;
            }
            return rows;
        };

        // A number that goes up whenever something that the picture depends on may have changed
        menuT["version"] = [](LuaMenu& self) { return self.mMenu->menu().generation(); };
        menuT["resize"]
            = [](LuaMenu& self, double width, double height) { self.mMenu->resize(float(width), float(height)); };
        menuT["problems"] = [](sol::this_state state, LuaMenu& self) {
            sol::state_view view(state);
            sol::table rows(view, sol::create);
            int index = 1;
            for (const std::string& problem : self.mMenu->problems())
                rows[index++] = problem;
            return rows;
        };

        menuT["find"] = [](sol::this_state state, LuaMenu& self, std::string_view name) -> sol::object {
            return tileId(state, self.mMenu->menu().find(name));
        };
        menuT["name"] = [](LuaMenu& self, const sol::object& which) { return tileOf(self, which).name(); };
        menuT["parent"] = [](sol::this_state state, LuaMenu& self, const sol::object& which) {
            return tileId(state, tileOf(self, which).parent());
        };
        menuT["children"] = [](sol::this_state state, LuaMenu& self, const sol::object& which) {
            sol::state_view view(state);
            sol::table rows(view, sol::create);
            int index = 1;
            for (const auto& child : tileOf(self, which).children())
                rows[index++] = child->uid();
            return rows;
        };

        // The value of a trait: a number, or the text if it is one
        menuT["get"] = [](sol::this_state state, LuaMenu& self, const sol::object& which, std::string_view trait) {
            FalloutMenu::Tile& tile = tileOf(self, which);
            return valueObject(state, self.mMenu->menu().get(tile, trait));
        };
        menuT["getNumber"] = [](LuaMenu& self, const sol::object& which, std::string_view trait) {
            return self.mMenu->menu().number(tileOf(self, which), trait);
        };
        menuT["getText"] = [](LuaMenu& self, const sol::object& which, std::string_view trait) {
            return self.mMenu->menu().text(tileOf(self, which), trait);
        };
        // Set a trait to a number or a text; nil lets the menu file decide again
        menuT["set"] = [](LuaMenu& self, const sol::object& which, std::string_view trait, const sol::object& value) {
            FalloutMenu::Tile& tile = tileOf(self, which);
            FalloutMenu::Menu& menu = self.mMenu->menu();
            if (value.is<std::string>())
                menu.set(tile, trait, FalloutMenu::Value::fromText(value.as<std::string>()));
            else if (value.is<bool>())
                menu.set(tile, trait, FalloutMenu::Value(value.as<bool>() ? 1.f : 0.f));
            else if (value.is<double>())
                menu.set(tile, trait, FalloutMenu::Value(static_cast<float>(value.as<double>())));
            else
                menu.clear(tile, trait);
        };

        // The rectangle a tile takes in pixels of the window: x, y, width, height
        menuT["rect"] = [](sol::this_state state, LuaMenu& self, const sol::object& which) -> sol::object {
            const auto rect = self.mMenu->menu().rectangle(tileOf(self, which));
            if (!rect)
                return sol::nil;
            const float scale = self.mMenu->scale();
            sol::state_view view(state);
            return sol::make_object(view,
                view.create_table_with("x", (*rect)[0] * scale, "y", (*rect)[1] * scale, "width", (*rect)[2] * scale,
                    "height", (*rect)[3] * scale));
        };

        // The colour a tile is drawn in: red, green, blue and alpha, 0 to 1
        menuT["color"] = [](sol::this_state state, LuaMenu& self, const sol::object& which) {
            const std::array<float, 4> color = self.mMenu->menu().color(tileOf(self, which));
            sol::state_view view(state);
            return view.create_table_with("red", color[0], "green", color[1], "blue", color[2], "alpha", color[3]);
        };

        // A tile made from a template of the menu under a tile, with a name of its own; the number of the new tile
        menuT["instantiate"] = [](sol::this_state state, LuaMenu& self, const sol::object& parent,
                                   std::string_view templateName, sol::optional<std::string_view> name) -> sol::object {
            FalloutMenu::Tile& under = tileOf(self, parent);
            return tileId(
                state, self.mMenu->menu().instantiate(under, templateName, name.value_or(std::string_view())));
        };
        // A tile with no traits under a tile (kind "text", "image", "rect" or "hotrect"), which the script gives its
        // traits with `set`: the number of the new tile, nothing for another kind
        menuT["create"] = [](sol::this_state state, LuaMenu& self, const sol::object& parent, std::string_view kind,
                              sol::optional<std::string_view> name) -> sol::object {
            const std::optional<FalloutMenu::TileType> type = FalloutMenu::tileTypeOf(kind);
            if (!type || *type == FalloutMenu::TileType::Template || *type == FalloutMenu::TileType::Menu)
                return sol::nil;
            FalloutMenu::Tile& under = tileOf(self, parent);
            return tileId(state, self.mMenu->menu().create(under, *type, name.value_or(std::string_view())));
        };
        menuT["remove"] = [](LuaMenu& self, const sol::object& which) { self.mMenu->remove(tileOf(self, which)); };
        menuT["removeChildren"]
            = [](LuaMenu& self, const sol::object& which) { self.mMenu->removeChildren(tileOf(self, which)); };

        // The tile and trait that a trait stands for when the menu file writes it as a reference to another (the button
        // of the pad that does what a tile does when it is clicked): the number of the tile and the name of the trait
        menuT["reference"] = [](sol::this_state state, LuaMenu& self, const sol::object& which,
                                 std::string_view trait) -> std::tuple<sol::object, sol::object> {
            const auto target = self.mMenu->menu().reference(tileOf(self, which), trait);
            if (!target)
                return { sol::nil, sol::nil };
            return { tileId(state, target->first), sol::make_object<std::string>(state, target->second) };
        };

        // The pointer, in pixels of the window. Each returns the number of the tile it concerns, if one.
        menuT["pointerMoved"] = [](sol::this_state state, LuaMenu& self, double x, double y) {
            return tileId(state, self.mMenu->pointerMoved(float(x), float(y)));
        };
        menuT["pointerPressed"] = [](sol::this_state state, LuaMenu& self, double x, double y) {
            return tileId(state, self.mMenu->pointerPressed(float(x), float(y)));
        };
        menuT["pointerReleased"] = [](sol::this_state state, LuaMenu& self, double x, double y) {
            return tileId(state, self.mMenu->pointerReleased(float(x), float(y)));
        };
        menuT["wheel"] = [](sol::this_state state, LuaMenu& self, double x, double y, int notches) {
            return tileId(state, self.mMenu->wheelMoved(float(x), float(y), notches));
        };
        // Count a click on a tile that did not come from the pointer
        menuT["click"] = [](LuaMenu& self, const sol::object& which) { self.mMenu->click(tileOf(self, which)); };
        menuT["hovered"] = [](sol::this_state state, LuaMenu& self) { return tileId(state, self.mMenu->hovered()); };
        // Put a scrollbar (a tile with a `_current_value`) at a value, as clicks of its arrows would
        menuT["setScrollbar"] = [](LuaMenu& self, const sol::object& which, double value) {
            self.mMenu->setScrollbar(tileOf(self, which), float(value));
        };
        menuT["dragged"] = [](sol::this_state state, LuaMenu& self) { return tileId(state, self.mMenu->dragged()); };

        // Whether the file (or the script, with `set`) gives the tile this trait, and all the tiles of the menu
        menuT["defines"] = [](LuaMenu& self, const sol::object& which, std::string_view trait) {
            return self.mMenu->menu().defines(tileOf(self, which), trait);
        };
        menuT["tiles"] = [](sol::this_state state, LuaMenu& self) {
            sol::state_view view(state);
            sol::table rows(view, sol::create);
            int index = 1;
            for (const FalloutMenu::Tile* tile : self.mMenu->menu().tiles())
                rows[index++] = tile->uid();
            return rows;
        };

        // Lists (`list_box.xml`): make one of a tile and a template for its rows (the number of the list, from 1; the
        // third argument is how many of its first rows the list is as high as), give it rows (tables of traits to set
        // on each: `string` and the like), pick a row, and find which row a tile is
        menuT["addList"] = [](sol::this_state state, LuaMenu& self, std::string_view listTile,
                               std::string_view templateName, sol::optional<int> fitRows) -> sol::object {
            const int number = self.mMenu->addList(listTile, templateName, std::max(0, fitRows.value_or(0)));
            if (number < 0)
                return sol::nil;
            return sol::make_object<int>(state, number + 1);
        };
        menuT["setRows"] = [](LuaMenu& self, int number, const sol::table& rows) {
            FalloutMenu::ListBox* list = self.mMenu->list(number - 1);
            if (list == nullptr)
                throw std::runtime_error("The menu has no list " + std::to_string(number));
            std::vector<std::map<std::string, FalloutMenu::Value>> made;
            for (std::size_t index = 1; index <= rows.size(); ++index)
            {
                const sol::object entry = rows[index];
                made.push_back(entry.is<sol::table>() ? traitsOf(entry.as<sol::table>())
                                                      : std::map<std::string, FalloutMenu::Value>());
            }
            list->setRows(made);
        };
        // Set traits on a row that is there (the row from 1), without making the rows again
        menuT["updateRow"] = [](LuaMenu& self, int number, int row, const sol::table& traits) {
            FalloutMenu::ListBox* list = self.mMenu->list(number - 1);
            if (list == nullptr)
                throw std::runtime_error("The menu has no list " + std::to_string(number));
            list->updateRow(row - 1, traitsOf(traits));
        };
        menuT["selectRow"] = [](LuaMenu& self, int number, int row, sol::optional<bool> reveal) {
            if (FalloutMenu::ListBox* list = self.mMenu->list(number - 1))
                list->select(row - 1, reveal.value_or(false));
        };
        menuT["selectedRow"] = [](LuaMenu& self, int number) {
            FalloutMenu::ListBox* list = self.mMenu->list(number - 1);
            return list == nullptr ? 0 : list->selected() + 1;
        };
        menuT["rowCount"] = [](LuaMenu& self, int number) {
            FalloutMenu::ListBox* list = self.mMenu->list(number - 1);
            return list == nullptr ? 0 : list->rowCount();
        };
        menuT["rowOf"] = [](LuaMenu& self, const sol::object& which) -> std::tuple<sol::object, sol::object> {
            const auto found = self.mMenu->rowOf(tileOf(self, which));
            if (!found)
                return { sol::nil, sol::nil };
            return { sol::make_object<int>(which.lua_state(), found->first + 1),
                sol::make_object<int>(which.lua_state(), found->second + 1) };
        };
        menuT["root"] = [](sol::this_state state, LuaMenu& self) { return tileId(state, &self.mMenu->menu().root()); };
        // What a text takes in one of the fonts of the games (1 to 9) when it is wrapped at `wrapWidth` (0 for no
        // wrapping): the width of its longest line, the height and the number of lines. The code of the games measures
        // the text of a message box this way, where the width of a text tile is at least the room it is wrapped in.
        menuT["measure"] = [](sol::this_state state, LuaMenu& self, int font, std::string_view text,
                               sol::optional<double> wrapWidth) {
            sol::state_view view(state);
            sol::table result(view, sol::create);
            const FalloutMenu::Font* face = self.mMenu->fonts().get(font);
            if (face == nullptr)
                return result;
            const FalloutMenu::Font::Layout layout = face->layout(text, static_cast<float>(wrapWidth.value_or(0.0)), 1);
            result["width"] = layout.mLongest;
            result["height"] = layout.mHeight;
            result["lines"] = layout.mLines;
            return result;
        };

        // Open a menu file (`message_menu.xml`) for a window of some size. `pad` says whether a pad is in use.
        // Nothing, and the reason, if the data lacks the file.
        auto resources = std::make_shared<std::shared_ptr<FalloutMenu::Resources>>();
        api["open"] = [resources](sol::this_state state, std::string_view file, double width, double height,
                          sol::optional<bool> pad) -> std::tuple<sol::object, sol::object> {
            if (!*resources)
                *resources = OFGui::makeFalloutMenuResources();
            std::string error;
            std::unique_ptr<FalloutMenu::OpenMenu> menu = FalloutMenu::OpenMenu::open(
                *resources, std::string(file), float(width), float(height), pad.value_or(false), &error);
            if (!menu)
                return { sol::nil, sol::make_object<std::string>(state, error) };
            return { sol::make_object<LuaMenu>(state, LuaMenu{ std::move(menu) }), sol::nil };
        };

        // The textures that menus asked for and the data does not have, so far
        api["missingTextures"] = [resources](sol::this_state state) {
            sol::state_view view(state);
            sol::table rows(view, sol::create);
            if (*resources)
            {
                int index = 1;
                for (const std::string& path : (*resources)->missingTextures())
                    rows[index++] = path;
            }
            return rows;
        };

        return api;
    }
}

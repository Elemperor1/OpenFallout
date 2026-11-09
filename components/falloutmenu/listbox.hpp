#ifndef OPENFALLOUT_COMPONENTS_FALLOUTMENU_LISTBOX_H
#define OPENFALLOUT_COMPONENTS_FALLOUTMENU_LISTBOX_H

#include <map>
#include <string>
#include <vector>

#include "menu.hpp"

namespace FalloutMenu
{
    class OpenMenu;

    /// What the games' own code does for a menu's list (`list_box.xml`): a row of the list is a tile made from a
    /// template under the list's tile; the code gives the row its traits, its height and where it is when the list is
    /// not scrolled (`_y`), tells the scrollbar how many items there are and how many show, and moves the highlight
    /// box to the row the pointer is on.
    class ListBox
    {
    public:
        /// `fitRows` is for a list that the code of the games makes as high as its first rows (the topics of a
        /// conversation: the first three) and scrolls in the steps that the rest of the rows need: 0 for the others
        ListBox(OpenMenu& open, int listTile, std::string templateName, int fitRows = 0);

        /// Make the rows again: each is a tile of the template with these traits set (`string`, `_line_alpha`, ...); a
        /// trait named `Tile.trait` is set on the tile of that name inside the row, and `_template` (a text) names
        /// another template for that row
        void setRows(const std::vector<std::map<std::string, Value>>& rows);
        /// Set traits on a row that is there (a value that changed: a toggle, a meter), without making the rows again:
        /// the scrolling of the list stays where it is. The traits are named as for `setRows`.
        void updateRow(int row, const std::map<std::string, Value>& traits);
        int rowCount() const { return static_cast<int>(mItems.size()); }
        /// The row that is picked out (the one the pointer was last on, or the screen picked), -1 for none
        int selected() const { return mSelected; }
        /// Pick a row out, and scroll to it if `reveal`
        void select(int row, bool reveal = false);
        Tile* item(int row);
        /// The row whose tile is `tile` or has it inside; -1 if none
        int rowOf(const Tile& tile) const;
        int listTile() const { return mList; }

        /// Work out again where the rows are, how far the list scrolls and where the highlight box goes
        void layout();

    private:
        Tile* list();
        void reveal(int row);
        void setTraits(Tile& item, const std::map<std::string, Value>& traits);

        OpenMenu& mOpen;
        int mList;
        std::string mTemplate;
        int mFitRows = 0;
        std::vector<int> mItems;
        int mSelected = -1;
        /// What the file gives for the height of one scroll step, before the code changes it
        float mFileDelta = 26.f;
        float mFileHeight = 0.f;
    };
}

#endif

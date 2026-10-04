#ifndef OPENFALLOUT_GAME_MWGUI_ITEMSELECTION_H
#define OPENFALLOUT_GAME_MWGUI_ITEMSELECTION_H

#include <MyGUI_Delegate.h>

#include "windowbase.hpp"

namespace OFWorld
{
    class Ptr;
}

namespace OFGui
{
    class ItemView;
    class SortFilterItemModel;

    class ItemSelectionDialog : public WindowModal
    {
    public:
        ItemSelectionDialog(const std::string& label);

        bool exit() override;

        typedef MyGUI::delegates::MultiDelegate<> EventHandle_Void;
        typedef MyGUI::delegates::MultiDelegate<OFWorld::Ptr> EventHandle_Item;

        EventHandle_Item eventItemSelected;
        EventHandle_Void eventDialogCanceled;

        void openContainer(const OFWorld::Ptr& container);
        void setCategory(int category);
        void setFilter(int filter);

        SortFilterItemModel* getSortModel() { return mSortModel; }

    private:
        ItemView* mItemView;
        SortFilterItemModel* mSortModel;

        void onSelectedItem(int index);

        void onCancelButtonClicked(MyGUI::Widget* sender);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
    };

}

#endif

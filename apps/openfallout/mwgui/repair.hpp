#ifndef OPENFALLOUT_MWGUI_REPAIR_H
#define OPENFALLOUT_MWGUI_REPAIR_H

#include <memory>

#include "windowbase.hpp"

#include "../mwmechanics/repair.hpp"

namespace OFGui
{

    class ItemSelectionDialog;
    class ItemWidget;
    class ItemChargeView;

    class Repair : public WindowBase
    {
    public:
        Repair();

        void onOpen() override;

        void setPtr(const OFWorld::Ptr& item) override;

        std::string_view getWindowIdForLua() const override { return "Repair"; }

    protected:
        ItemChargeView* mRepairBox;

        MyGUI::Widget* mToolBox;

        ItemWidget* mToolIcon;

        std::unique_ptr<ItemSelectionDialog> mItemSelectionDialog;

        MyGUI::TextBox* mUsesLabel;
        MyGUI::TextBox* mQualityLabel;

        MyGUI::Button* mCancelButton;

        OFMechanics::Repair mRepair;

        void updateRepairView();

        void onSelectItem(MyGUI::Widget* sender);

        void onItemSelected(OFWorld::Ptr item);
        void onItemCancel();

        void onRepairItem(MyGUI::Widget* sender, const OFWorld::Ptr& ptr);
        void onCancel(MyGUI::Widget* sender);

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
    };

}

#endif

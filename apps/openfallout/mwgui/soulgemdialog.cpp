#include "soulgemdialog.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "messagebox.hpp"

namespace OFGui
{

    void SoulgemDialog::show(const OFWorld::Ptr& soulgem)
    {
        mSoulgem = soulgem;
        std::vector<std::string> buttons;
        buttons.emplace_back("#{sRechargeEnchantment}");
        buttons.emplace_back("#{sMake Enchantment}");
        mManager->createInteractiveMessageBox("#{sDoYouWantTo}", buttons);
        mManager->eventButtonPressed += MyGUI::newDelegate(this, &SoulgemDialog::onButtonPressed);
    }

    void SoulgemDialog::onButtonPressed(int button)
    {
        if (button == 0)
        {
            OFBase::Environment::get().getWindowManager()->pushGuiMode(GM_Recharge, mSoulgem);
        }
        else
        {
            OFBase::Environment::get().getWindowManager()->pushGuiMode(GM_Enchanting, mSoulgem);
        }
    }

}

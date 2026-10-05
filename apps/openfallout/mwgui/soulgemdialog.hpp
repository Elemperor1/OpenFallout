#ifndef OPENFALLOUT_MWGUI_SOULGEMDIALOG_H
#define OPENFALLOUT_MWGUI_SOULGEMDIALOG_H

#include "../mwworld/ptr.hpp"

namespace OFGui
{

    class MessageBoxManager;

    class SoulgemDialog
    {
    public:
        SoulgemDialog(MessageBoxManager* manager)
            : mManager(manager)
        {
        }

        void show(const OFWorld::Ptr& soulgem);

        void onButtonPressed(int button);

    private:
        MessageBoxManager* mManager;
        OFWorld::Ptr mSoulgem;
    };

}

#endif

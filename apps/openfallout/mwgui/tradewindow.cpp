#include "tradewindow.hpp"

#include <MyGUI_Button.h>
#include <MyGUI_ControllerManager.h>
#include <MyGUI_ControllerRepeatClick.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_InputManager.h>

#include <components/misc/rng.hpp>
#include <components/misc/strings/format.hpp>
#include <components/widgets/numericeditbox.hpp>

#include "../mwbase/dialoguemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"

#include "containeritemmodel.hpp"
#include "countdialog.hpp"
#include "inventorywindow.hpp"
#include "itemview.hpp"
#include "sortfilteritemmodel.hpp"
#include "tooltips.hpp"
#include "tradeitemmodel.hpp"

namespace
{

    int getEffectiveValue(OFWorld::Ptr item, int count)
    {
        float price = static_cast<float>(item.getClass().getValue(item));
        if (item.getClass().hasItemHealth(item))
        {
            price *= item.getClass().getItemNormalizedHealth(item);
        }
        return static_cast<int>(price * count);
    }

    bool haggle(const OFWorld::Ptr& player, const OFWorld::Ptr& merchant, int playerOffer, int merchantOffer)
    {
        // accept if merchant offer is better than player offer
        if (playerOffer <= merchantOffer)
        {
            return true;
        }

        // reject if npc is a creature
        if (merchant.getType() != ESM::NPC::sRecordId)
        {
            return false;
        }

        const OFWorld::Store<ESM::GameSetting>& gmst
            = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

        // Is the player buying?
        bool buying = (merchantOffer < 0);
        int a = std::abs(merchantOffer);
        int b = std::abs(playerOffer);
        int d = (buying) ? int(100 * (a - b) / a) : int(100 * (b - a) / b);

        int clampedDisposition = OFBase::Environment::get().getMechanicsManager()->getDerivedDisposition(merchant);

        const OFMechanics::CreatureStats& merchantStats = merchant.getClass().getCreatureStats(merchant);
        const OFMechanics::CreatureStats& playerStats = player.getClass().getCreatureStats(player);

        float a1 = static_cast<float>(player.getClass().getSkill(player, ESM::Skill::Mercantile));
        float b1 = 0.1f * playerStats.getAttribute(ESM::Attribute::Luck).getModified();
        float c1 = 0.2f * playerStats.getAttribute(ESM::Attribute::Personality).getModified();
        float d1 = static_cast<float>(merchant.getClass().getSkill(merchant, ESM::Skill::Mercantile));
        float e1 = 0.1f * merchantStats.getAttribute(ESM::Attribute::Luck).getModified();
        float f1 = 0.2f * merchantStats.getAttribute(ESM::Attribute::Personality).getModified();

        float dispositionTerm = gmst.find("fDispositionMod")->mValue.getFloat() * (clampedDisposition - 50);
        float pcTerm = (dispositionTerm + a1 + b1 + c1) * playerStats.getFatigueTerm();
        float npcTerm = (d1 + e1 + f1) * merchantStats.getFatigueTerm();
        float x = gmst.find("fBargainOfferMulti")->mValue.getFloat() * d
            + gmst.find("fBargainOfferBase")->mValue.getFloat() + int(pcTerm - npcTerm);

        auto& prng = OFBase::Environment::get().getWorld()->getPrng();
        int roll = Misc::Rng::rollDice(100, prng) + 1;

        // reject if roll fails
        // (or if player tries to buy things and get money)
        if (roll > x || (merchantOffer < 0 && 0 < playerOffer))
        {
            return false;
        }

        // apply skill gain on successful barter
        float skillGain = 0.f;
        int finalPrice = std::abs(playerOffer);
        int initialMerchantOffer = std::abs(merchantOffer);

        if (!buying && (finalPrice > initialMerchantOffer))
        {
            skillGain = std::floor(100.f * (finalPrice - initialMerchantOffer) / finalPrice);
        }
        else if (buying && (finalPrice < initialMerchantOffer))
        {
            skillGain = std::floor(100.f * (initialMerchantOffer - finalPrice) / initialMerchantOffer);
        }
        player.getClass().skillUsageSucceeded(
            player, ESM::Skill::Mercantile, ESM::Skill::Mercantile_Success, skillGain);

        return true;
    }
}

namespace OFGui
{
    TradeWindow::TradeWindow()
        : WindowBase("openmw_trade_window.layout")
        , mSortModel(nullptr)
        , mTradeModel(nullptr)
        , mItemToSell(-1)
        , mCurrentBalance(0)
        , mCurrentMerchantOffer(0)
        , mUpdateNextFrame(false)
    {
        getWidget(mFilterAll, "AllButton");
        getWidget(mFilterWeapon, "WeaponButton");
        getWidget(mFilterApparel, "ApparelButton");
        getWidget(mFilterMagic, "MagicButton");
        getWidget(mFilterMisc, "MiscButton");

        getWidget(mMaxSaleButton, "MaxSaleButton");
        getWidget(mCancelButton, "CancelButton");
        getWidget(mOfferButton, "OfferButton");
        getWidget(mPlayerGold, "PlayerGold");
        getWidget(mMerchantGold, "MerchantGold");
        getWidget(mIncreaseButton, "IncreaseButton");
        getWidget(mDecreaseButton, "DecreaseButton");
        getWidget(mTotalBalance, "TotalBalance");
        getWidget(mTotalBalanceLabel, "TotalBalanceLabel");
        getWidget(mBottomPane, "BottomPane");
        getWidget(mFilterEdit, "FilterEdit");

        getWidget(mItemView, "ItemView");
        mItemView->eventItemClicked += MyGUI::newDelegate(this, &TradeWindow::onItemSelected);

        mFilterAll->setStateSelected(true);

        mFilterAll->eventMouseButtonClick += MyGUI::newDelegate(this, &TradeWindow::onFilterChanged);
        mFilterWeapon->eventMouseButtonClick += MyGUI::newDelegate(this, &TradeWindow::onFilterChanged);
        mFilterApparel->eventMouseButtonClick += MyGUI::newDelegate(this, &TradeWindow::onFilterChanged);
        mFilterMagic->eventMouseButtonClick += MyGUI::newDelegate(this, &TradeWindow::onFilterChanged);
        mFilterMisc->eventMouseButtonClick += MyGUI::newDelegate(this, &TradeWindow::onFilterChanged);
        mFilterEdit->eventEditTextChange += MyGUI::newDelegate(this, &TradeWindow::onNameFilterChanged);

        mCancelButton->eventMouseButtonClick += MyGUI::newDelegate(this, &TradeWindow::onCancelButtonClicked);
        mOfferButton->eventMouseButtonClick += MyGUI::newDelegate(this, &TradeWindow::onOfferButtonClicked);
        mMaxSaleButton->eventMouseButtonClick += MyGUI::newDelegate(this, &TradeWindow::onMaxSaleButtonClicked);
        mIncreaseButton->eventMouseButtonPressed += MyGUI::newDelegate(this, &TradeWindow::onIncreaseButtonPressed);
        mIncreaseButton->eventMouseButtonReleased += MyGUI::newDelegate(this, &TradeWindow::onBalanceButtonReleased);
        mDecreaseButton->eventMouseButtonPressed += MyGUI::newDelegate(this, &TradeWindow::onDecreaseButtonPressed);
        mDecreaseButton->eventMouseButtonReleased += MyGUI::newDelegate(this, &TradeWindow::onBalanceButtonReleased);

        mTotalBalance->eventValueChanged += MyGUI::newDelegate(this, &TradeWindow::onBalanceValueChanged);
        mTotalBalance->eventEditSelectAccept += MyGUI::newDelegate(this, &TradeWindow::onAccept);
        mTotalBalance->setMinValue(
            std::numeric_limits<int>::min() + 1); // disallow INT_MIN since abs(INT_MIN) is undefined

        setCoord(400, 0, 400, 300);

        if (Settings::gui().mControllerMenus)
        {
            // Show L1 and R1 buttons next to tabs
            MyGUI::ImageBox* image;
            getWidget(image, "BtnL1Image");
            image->setVisible(true);
            image->setUserString("Hidden", "false");
            image->setImageTexture(OFBase::Environment::get().getInputManager()->getControllerButtonIcon(
                SDL_CONTROLLER_BUTTON_LEFTSHOULDER));

            getWidget(image, "BtnR1Image");
            image->setVisible(true);
            image->setUserString("Hidden", "false");
            image->setImageTexture(OFBase::Environment::get().getInputManager()->getControllerButtonIcon(
                SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));

            mControllerButtons.mA = "#{Interface:Buy}";
            mControllerButtons.mB = "#{Interface:Cancel}";
            mControllerButtons.mX = "#{Interface:Offer}";
            mControllerButtons.mR3 = "#{Interface:Info}";
            mControllerButtons.mL2 = "#{Interface:Inventory}";
        }
    }

    void TradeWindow::setPtr(const OFWorld::Ptr& actor)
    {
        if (actor.isEmpty() || !actor.getClass().isActor())
            throw std::runtime_error("Invalid argument in TradeWindow::setPtr");
        mPtr = actor;

        mCurrentBalance = 0;
        mCurrentMerchantOffer = 0;

        std::vector<OFWorld::Ptr> itemSources;
        // Important: actor goes first, so purchased items come out of the actor's pocket first
        itemSources.push_back(actor);
        OFBase::Environment::get().getWorld()->getContainersOwnedBy(actor, itemSources);

        std::vector<OFWorld::Ptr> worldItems;
        OFBase::Environment::get().getWorld()->getItemsOwnedBy(actor, worldItems);

        auto tradeModel
            = std::make_unique<TradeItemModel>(std::make_unique<ContainerItemModel>(itemSources, worldItems), mPtr);
        mTradeModel = tradeModel.get();
        auto sortModel = std::make_unique<SortFilterItemModel>(std::move(tradeModel));
        mSortModel = sortModel.get();
        mItemView->setModel(std::move(sortModel));
        mItemView->resetScrollBars();

        updateLabels();

        setTitle(actor.getClass().getName(actor));

        onFilterChanged(mFilterAll);
        mFilterEdit->setCaption({});

        // Cycle to the buy window if it's not active.
        if (Settings::gui().mControllerMenus && !mActiveControllerWindow)
            OFBase::Environment::get().getWindowManager()->cycleActiveControllerWindow(true);
    }

    void TradeWindow::onFrame(float dt)
    {
        checkReferenceAvailable();

        if (isVisible() && mUpdateNextFrame)
        {
            mTradeModel->updateBorrowed();
            OFBase::Environment::get().getWindowManager()->getInventoryWindow()->getTradeModel()->updateBorrowed();
            OFBase::Environment::get().getWindowManager()->getInventoryWindow()->updateItemView();
            mItemView->update();
            updateOffer();
            mUpdateNextFrame = false;
        }
    }

    void TradeWindow::onNameFilterChanged(MyGUI::EditBox* sender)
    {
        mSortModel->setNameFilter(sender->getCaption());
        mItemView->update();
    }

    void TradeWindow::onFilterChanged(MyGUI::Widget* sender)
    {
        if (sender == mFilterAll)
            mSortModel->setCategory(SortFilterItemModel::Category_All);
        else if (sender == mFilterWeapon)
            mSortModel->setCategory(SortFilterItemModel::Category_Weapon);
        else if (sender == mFilterApparel)
            mSortModel->setCategory(SortFilterItemModel::Category_Apparel);
        else if (sender == mFilterMagic)
            mSortModel->setCategory(SortFilterItemModel::Category_Magic);
        else if (sender == mFilterMisc)
            mSortModel->setCategory(SortFilterItemModel::Category_Misc);

        mFilterAll->setStateSelected(false);
        mFilterWeapon->setStateSelected(false);
        mFilterApparel->setStateSelected(false);
        mFilterMagic->setStateSelected(false);
        mFilterMisc->setStateSelected(false);

        sender->castType<MyGUI::Button>()->setStateSelected(true);

        mItemView->update();
    }

    int TradeWindow::getMerchantServices()
    {
        return mPtr.getClass().getServices(mPtr);
    }

    bool TradeWindow::exit()
    {
        mTradeModel->abort();
        OFBase::Environment::get().getWindowManager()->getInventoryWindow()->getTradeModel()->abort();
        return true;
    }

    void TradeWindow::onItemSelected(int index)
    {
        const ItemStack& item = mSortModel->getItem(index);

        OFWorld::Ptr object = item.mBase;
        size_t count = item.mCount;
        bool shift = MyGUI::InputManager::getInstance().isShiftPressed();
        if (MyGUI::InputManager::getInstance().isControlPressed())
            count = 1;

        if (count > 1 && !shift)
        {
            CountDialog* dialog = OFBase::Environment::get().getWindowManager()->getCountDialog();
            std::string message = "#{sQuanityMenuMessage02}";
            std::string name{ object.getClass().getName(object) };
            name += OFGui::ToolTips::getSoulString(object.getCellRef());
            dialog->openCountDialog(name, message, static_cast<int>(count));
            dialog->eventOkClicked.clear();
            dialog->eventOkClicked += MyGUI::newDelegate(this, &TradeWindow::sellItem);
            mItemToSell = mSortModel->mapToSource(index);
        }
        else
        {
            mItemToSell = mSortModel->mapToSource(index);
            sellItem(nullptr, count);
        }
    }

    void TradeWindow::sellItem(MyGUI::Widget* /*sender*/, std::size_t count)
    {
        const ItemStack& item = mTradeModel->getItem(mItemToSell);
        const ESM::RefId& sound = item.mBase.getClass().getUpSoundId(item.mBase);
        OFBase::Environment::get().getWindowManager()->playSound(sound);

        TradeItemModel* playerTradeModel
            = OFBase::Environment::get().getWindowManager()->getInventoryWindow()->getTradeModel();

        if (item.mType == ItemStack::Type_Barter)
        {
            // this was an item borrowed to us by the player
            mTradeModel->returnItemBorrowedToUs(mItemToSell, count);
            playerTradeModel->returnItemBorrowedFromUs(mItemToSell, mTradeModel, count);
            updateOffer();
        }
        else
        {
            // borrow item to player
            playerTradeModel->borrowItemToUs(mItemToSell, mTradeModel, count);
            mTradeModel->borrowItemFromUs(mItemToSell, count);
            updateOffer();
        }

        OFBase::Environment::get().getWindowManager()->getInventoryWindow()->updateItemView();
        mItemView->update();
    }

    void TradeWindow::borrowItem(int index, size_t count)
    {
        TradeItemModel* playerTradeModel
            = OFBase::Environment::get().getWindowManager()->getInventoryWindow()->getTradeModel();
        mTradeModel->borrowItemToUs(index, playerTradeModel, count);
        mItemView->update();
        updateOffer();
    }

    void TradeWindow::returnItem(int index, size_t count)
    {
        TradeItemModel* playerTradeModel
            = OFBase::Environment::get().getWindowManager()->getInventoryWindow()->getTradeModel();
        mTradeModel->returnItemBorrowedFromUs(index, playerTradeModel, count);
        mItemView->update();
        updateOffer();
    }

    void TradeWindow::addOrRemoveGold(int amount, const OFWorld::Ptr& actor)
    {
        OFWorld::ContainerStore& store = actor.getClass().getContainerStore(actor);

        if (amount > 0)
        {
            store.add(OFWorld::ContainerStore::sGoldId, amount);
        }
        else
        {
            store.remove(OFWorld::ContainerStore::sGoldId, -amount);
        }
    }

    void TradeWindow::onOfferSubmitted(MyGUI::Widget* /*sender*/, size_t offerAmount)
    {
        mCurrentBalance = static_cast<int>(offerAmount) * (mCurrentBalance < 0 ? -1 : 1);
        updateLabels();
        onOfferButtonClicked(mOfferButton);
    }

    void TradeWindow::onOfferButtonClicked(MyGUI::Widget* /*sender*/)
    {
        TradeItemModel* playerItemModel
            = OFBase::Environment::get().getWindowManager()->getInventoryWindow()->getTradeModel();

        const OFWorld::Store<ESM::GameSetting>& gmst
            = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

        if (mTotalBalance->getValue() == 0)
            mCurrentBalance = 0;

        // were there any items traded at all?
        const std::vector<ItemStack>& playerBought = playerItemModel->getItemsBorrowedToUs();
        const std::vector<ItemStack>& merchantBought = mTradeModel->getItemsBorrowedToUs();
        if (playerBought.empty() && merchantBought.empty())
        {
            // user notification
            OFBase::Environment::get().getWindowManager()->messageBox("#{sBarterDialog11}");
            return;
        }

        OFWorld::Ptr player = OFMechanics::getPlayer();
        int playerGold = player.getClass().getContainerStore(player).count(OFWorld::ContainerStore::sGoldId);

        // check if the player can afford this
        if (mCurrentBalance < 0 && playerGold < std::abs(mCurrentBalance))
        {
            // user notification
            OFBase::Environment::get().getWindowManager()->messageBox("#{sBarterDialog1}");
            return;
        }

        // check if the merchant can afford this
        if (mCurrentBalance > 0 && getMerchantGold() < mCurrentBalance)
        {
            // user notification
            OFBase::Environment::get().getWindowManager()->messageBox("#{sBarterDialog2}");
            return;
        }

        // check if the player is attempting to sell back an item stolen from this actor
        for (const ItemStack& itemStack : merchantBought)
        {
            if (OFBase::Environment::get().getMechanicsManager()->isItemStolenFrom(
                    itemStack.mBase.getCellRef().getRefId(), mPtr))
            {
                std::string msg = gmst.find("sNotifyMessage49")->mValue.getString();
                msg = Misc::StringUtils::format(msg, itemStack.mBase.getClass().getName(itemStack.mBase));
                OFBase::Environment::get().getWindowManager()->messageBox(msg);

                OFBase::Environment::get().getMechanicsManager()->confiscateStolenItemToOwner(
                    player, itemStack.mBase, mPtr, static_cast<int>(itemStack.mCount));

                onCancelButtonClicked(mCancelButton);
                OFBase::Environment::get().getWindowManager()->exitCurrentGuiMode();
                return;
            }
        }

        bool offerAccepted = haggle(player, mPtr, mCurrentBalance, mCurrentMerchantOffer);

        // apply disposition change if merchant is NPC
        if (mPtr.getClass().isNpc())
        {
            int dispositionDelta = offerAccepted ? gmst.find("iBarterSuccessDisposition")->mValue.getInteger()
                                                 : gmst.find("iBarterFailDisposition")->mValue.getInteger();

            OFBase::Environment::get().getDialogueManager()->applyBarterDispositionChange(dispositionDelta);
        }

        // display message on haggle failure
        if (!offerAccepted)
        {
            OFBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage9}");
            return;
        }

        // make the item transfer
        mTradeModel->transferItems();
        playerItemModel->transferItems();

        // transfer the gold
        if (mCurrentBalance != 0)
        {
            addOrRemoveGold(mCurrentBalance, player);
            mPtr.getClass().getCreatureStats(mPtr).setGoldPool(
                mPtr.getClass().getCreatureStats(mPtr).getGoldPool() - mCurrentBalance);
        }

        eventTradeDone();

        OFBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Item Gold Up"));
        OFBase::Environment::get().getWindowManager()->removeGuiMode(GM_Barter);
    }

    void TradeWindow::onAccept(MyGUI::EditBox* sender)
    {
        onOfferButtonClicked(sender);

        // To do not spam onAccept() again and again
        OFBase::Environment::get().getWindowManager()->injectKeyRelease(MyGUI::KeyCode::None);
    }

    void TradeWindow::onCancelButtonClicked(MyGUI::Widget* /*sender*/)
    {
        exit();
        OFBase::Environment::get().getWindowManager()->removeGuiMode(GM_Barter);
    }

    void TradeWindow::onMaxSaleButtonClicked(MyGUI::Widget* /*sender*/)
    {
        mCurrentBalance = getMerchantGold();
        updateLabels();
    }

    void TradeWindow::addRepeatController(MyGUI::Widget* widget)
    {
        MyGUI::ControllerItem* item
            = MyGUI::ControllerManager::getInstance().createItem(MyGUI::ControllerRepeatClick::getClassTypeName());
        MyGUI::ControllerRepeatClick* controller = static_cast<MyGUI::ControllerRepeatClick*>(item);
        controller->eventRepeatClick += newDelegate(this, &TradeWindow::onRepeatClick);
        MyGUI::ControllerManager::getInstance().addItem(widget, controller);
    }

    void TradeWindow::onIncreaseButtonPressed(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id)
    {
        addRepeatController(sender);
        onIncreaseButtonTriggered();
    }

    void TradeWindow::onDecreaseButtonPressed(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id)
    {
        addRepeatController(sender);
        onDecreaseButtonTriggered();
    }

    void TradeWindow::onRepeatClick(MyGUI::Widget* widget, MyGUI::ControllerItem* controller)
    {
        if (widget == mIncreaseButton)
            onIncreaseButtonTriggered();
        else if (widget == mDecreaseButton)
            onDecreaseButtonTriggered();
    }

    void TradeWindow::onBalanceButtonReleased(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id)
    {
        MyGUI::ControllerManager::getInstance().removeItem(sender);
    }

    void TradeWindow::onBalanceValueChanged(int value)
    {
        int previousBalance = mCurrentBalance;

        // Entering a "-" sign inverts the buying/selling state
        mCurrentBalance = (mCurrentBalance >= 0 ? 1 : -1) * value;
        updateLabels();

        if (mCurrentBalance == 0)
            mCurrentBalance = previousBalance;

        if (value != std::abs(value))
            mTotalBalance->setValue(std::abs(value));
    }

    void TradeWindow::onIncreaseButtonTriggered()
    {
        // prevent overflows, and prevent entering INT_MIN since abs(INT_MIN) is undefined
        if (mCurrentBalance == std::numeric_limits<int>::max()
            || mCurrentBalance == std::numeric_limits<int>::min() + 1)
            return;
        if (mTotalBalance->getValue() == 0)
            mCurrentBalance = 0;
        if (mCurrentBalance < 0)
            mCurrentBalance -= 1;
        else
            mCurrentBalance += 1;
        updateLabels();
    }

    void TradeWindow::onDecreaseButtonTriggered()
    {
        if (mTotalBalance->getValue() == 0)
            mCurrentBalance = 0;
        if (mCurrentBalance < 0)
            mCurrentBalance += 1;
        else
            mCurrentBalance -= 1;
        updateLabels();
    }

    void TradeWindow::updateLabels()
    {
        OFWorld::Ptr player = OFMechanics::getPlayer();
        int playerGold = player.getClass().getContainerStore(player).count(OFWorld::ContainerStore::sGoldId);
        mPlayerGold->setCaptionWithReplacing("#{sYourGold} " + MyGUI::utility::toString(playerGold));

        TradeItemModel* playerTradeModel
            = OFBase::Environment::get().getWindowManager()->getInventoryWindow()->getTradeModel();
        const std::vector<ItemStack>& playerBorrowed = playerTradeModel->getItemsBorrowedToUs();
        const std::vector<ItemStack>& merchantBorrowed = mTradeModel->getItemsBorrowedToUs();

        if (playerBorrowed.empty() && merchantBorrowed.empty())
        {
            mCurrentBalance = 0;
        }

        if (mCurrentBalance < 0)
        {
            mTotalBalanceLabel->setCaptionWithReplacing("#{sTotalCost}");
        }
        else
        {
            mTotalBalanceLabel->setCaptionWithReplacing("#{sTotalSold}");
        }

        mTotalBalance->setValue(std::abs(mCurrentBalance));

        mMerchantGold->setCaptionWithReplacing("#{sSellerGold} " + MyGUI::utility::toString(getMerchantGold()));
    }

    void TradeWindow::updateOffer()
    {
        TradeItemModel* playerTradeModel
            = OFBase::Environment::get().getWindowManager()->getInventoryWindow()->getTradeModel();

        int merchantOffer = 0;

        // The offered price must be capped at 75% of the base price to avoid exploits
        // connected to buying and selling the same item.
        // This value has been determined by researching the limitations of the vanilla formula
        // and may not be sufficient if getBarterOffer behavior has been changed.
        const std::vector<ItemStack>& playerBorrowed = playerTradeModel->getItemsBorrowedToUs();
        for (const ItemStack& itemStack : playerBorrowed)
        {
            const int basePrice = getEffectiveValue(itemStack.mBase, static_cast<int>(itemStack.mCount));
            const int cap
                = static_cast<int>(std::max(1.f, 0.75f * basePrice)); // Minimum buying price -- 75% of the base
            const int buyingPrice
                = OFBase::Environment::get().getMechanicsManager()->getBarterOffer(mPtr, basePrice, true);
            merchantOffer -= std::max(cap, buyingPrice);
        }

        const std::vector<ItemStack>& merchantBorrowed = mTradeModel->getItemsBorrowedToUs();
        for (const ItemStack& itemStack : merchantBorrowed)
        {
            const int basePrice = getEffectiveValue(itemStack.mBase, static_cast<int>(itemStack.mCount));
            const int cap
                = static_cast<int>(std::max(1.f, 0.75f * basePrice)); // Maximum selling price -- 75% of the base
            const int sellingPrice
                = OFBase::Environment::get().getMechanicsManager()->getBarterOffer(mPtr, basePrice, false);
            merchantOffer += mPtr.getClass().isNpc() ? std::min(cap, sellingPrice) : sellingPrice;
        }

        int diff = merchantOffer - mCurrentMerchantOffer;
        mCurrentMerchantOffer = merchantOffer;
        mCurrentBalance += diff;
        updateLabels();
    }

    void TradeWindow::onReferenceUnavailable()
    {
        // remove both Trade and Dialogue (since you always trade with the NPC/creature that you have previously talked
        // to)
        OFBase::Environment::get().getWindowManager()->removeGuiMode(GM_Barter);
        OFBase::Environment::get().getWindowManager()->exitCurrentGuiMode();
    }

    int TradeWindow::getMerchantGold()
    {
        int merchantGold = mPtr.getClass().getCreatureStats(mPtr).getGoldPool();
        return merchantGold;
    }

    void TradeWindow::resetReference()
    {
        ReferenceInterface::resetReference();
        mItemView->setModel(nullptr);
        mTradeModel = nullptr;
        mSortModel = nullptr;
    }

    void TradeWindow::onClose()
    {
        // Make sure the window was actually closed and not temporarily hidden.
        if (OFBase::Environment::get().getWindowManager()->containsMode(GM_Barter))
            return;
        resetReference();
    }

    void TradeWindow::onDeleteCustomData(const OFWorld::Ptr& ptr)
    {
        if (mTradeModel && mTradeModel->usesContainer(ptr))
            OFBase::Environment::get().getWindowManager()->removeGuiMode(GM_Barter);
    }

    bool TradeWindow::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            int index = mItemView->getControllerFocus();
            if (index >= 0 && index < mItemView->getItemCount())
                onItemSelected(index);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_B)
        {
            onCancelButtonClicked(mCancelButton);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_X)
        {
            if (mCurrentBalance == 0)
                return true;
            // Show a count dialog to allow for bartering.
            CountDialog* dialog = OFBase::Environment::get().getWindowManager()->getCountDialog();
            if (mCurrentBalance < 0)
            {
                // Buying from the merchant
                dialog->openCountDialog("#{sTotalcost}:", "#{sOffer}", -mCurrentMerchantOffer);
                dialog->setCount(-mCurrentBalance);
            }
            else
            {
                // Selling to the merchant
                dialog->openCountDialog("#{sTotalsold}:", "#{sOffer}", getMerchantGold());
                dialog->setCount(mCurrentBalance);
            }
            dialog->eventOkClicked.clear();
            dialog->eventOkClicked += MyGUI::newDelegate(this, &TradeWindow::onOfferSubmitted);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
        {
            if (mFilterAll->getStateSelected())
                onFilterChanged(mFilterMisc);
            else if (mFilterWeapon->getStateSelected())
                onFilterChanged(mFilterAll);
            else if (mFilterApparel->getStateSelected())
                onFilterChanged(mFilterWeapon);
            else if (mFilterMagic->getStateSelected())
                onFilterChanged(mFilterApparel);
            else if (mFilterMisc->getStateSelected())
                onFilterChanged(mFilterMagic);
            OFBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
        {
            if (mFilterAll->getStateSelected())
                onFilterChanged(mFilterWeapon);
            else if (mFilterWeapon->getStateSelected())
                onFilterChanged(mFilterApparel);
            else if (mFilterApparel->getStateSelected())
                onFilterChanged(mFilterMagic);
            else if (mFilterMagic->getStateSelected())
                onFilterChanged(mFilterMisc);
            else if (mFilterMisc->getStateSelected())
                onFilterChanged(mFilterAll);
            OFBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_RIGHTSTICK || arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP
            || arg.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN || arg.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT
            || arg.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
        {
            mItemView->onControllerButton(arg.button);
        }

        return true;
    }

    void TradeWindow::setActiveControllerWindow(bool active)
    {
        // Show L1 and R1 buttons next to tabs
        MyGUI::Widget* image;
        getWidget(image, "BtnL1Image");
        image->setVisible(active);

        getWidget(image, "BtnR1Image");
        image->setVisible(active);

        mItemView->setActiveControllerWindow(active);
        WindowBase::setActiveControllerWindow(active);
    }

    void TradeWindow::updateItemView()
    {
        mItemView->update();
    }

    void TradeWindow::onInventoryUpdate(const OFWorld::Ptr& ptr)
    {
        if (mTradeModel && mTradeModel->usesContainer(ptr))
            mUpdateNextFrame = true;
    }
}

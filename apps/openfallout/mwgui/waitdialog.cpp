#include "waitdialog.hpp"

#include <MyGUI_InputManager.h>
#include <MyGUI_ProgressBar.h>
#include <MyGUI_ScrollBar.h>

#include <components/misc/rng.hpp>

#include <components/esm3/loadregn.hpp>
#include <components/settings/values.hpp>
#include <components/widgets/box.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/datetimemanager.hpp"
#include "../mwworld/esmstore.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/npcstats.hpp"

#include <format>

namespace OFGui
{

    WaitDialogProgressBar::WaitDialogProgressBar()
        : WindowBase("openmw_wait_dialog_progressbar.layout")
    {
        getWidget(mProgressBar, "ProgressBar");
        getWidget(mProgressText, "ProgressText");
    }

    void WaitDialogProgressBar::onOpen()
    {
        center();
    }

    void WaitDialogProgressBar::setProgress(int cur, int total)
    {
        mProgressBar->setProgressRange(total);
        mProgressBar->setProgressPosition(cur);
        mProgressText->setCaption(std::format("{}/{}", cur, total));
    }

    // ---------------------------------------------------------------------------------------------------------

    WaitDialog::WaitDialog()
        : WindowBase("openmw_wait_dialog.layout")
        , mSleeping(false)
        , mHours(1)
        , mManualHours(1)
        , mFadeTimeRemaining(0)
        , mInterruptAt(-1)
        , mProgressBar()
    {
        getWidget(mDateTimeText, "DateTimeText");
        getWidget(mRestText, "RestText");
        getWidget(mHourText, "HourText");
        getWidget(mUntilHealedButton, "UntilHealedButton");
        getWidget(mWaitButton, "WaitButton");
        getWidget(mCancelButton, "CancelButton");
        getWidget(mHourSlider, "HourSlider");

        mCancelButton->eventMouseButtonClick += MyGUI::newDelegate(this, &WaitDialog::onCancelButtonClicked);
        mUntilHealedButton->eventMouseButtonClick += MyGUI::newDelegate(this, &WaitDialog::onUntilHealedButtonClicked);
        mWaitButton->eventMouseButtonClick += MyGUI::newDelegate(this, &WaitDialog::onWaitButtonClicked);
        mHourSlider->eventScrollChangePosition += MyGUI::newDelegate(this, &WaitDialog::onHourSliderChangedPosition);

        mCancelButton->eventKeyButtonPressed += MyGUI::newDelegate(this, &WaitDialog::onKeyButtonPressed);
        mWaitButton->eventKeyButtonPressed += MyGUI::newDelegate(this, &WaitDialog::onKeyButtonPressed);
        mUntilHealedButton->eventKeyButtonPressed += MyGUI::newDelegate(this, &WaitDialog::onKeyButtonPressed);

        mTimeAdvancer.eventProgressChanged += MyGUI::newDelegate(this, &WaitDialog::onWaitingProgressChanged);
        mTimeAdvancer.eventInterrupted += MyGUI::newDelegate(this, &WaitDialog::onWaitingInterrupted);
        mTimeAdvancer.eventFinished += MyGUI::newDelegate(this, &WaitDialog::onWaitingFinished);

        mControllerButtons.mB = "#{Interface:Cancel}";
        mDisableGamepadCursor = Settings::gui().mControllerMenus;
    }

    void WaitDialog::setPtr(const OFWorld::Ptr& ptr)
    {
        const int restFlags = OFBase::Environment::get().getWorld()->canRest();

        const bool underwater = (restFlags & OFBase::World::Rest_PlayerIsUnderwater) != 0;
        // Resting in air is allowed if you're using a bed
        const bool inAir = ptr.isEmpty() && (restFlags & OFBase::World::Rest_PlayerIsInAir) != 0;
        const bool enemiesNearby = (restFlags & OFBase::World::Rest_EnemiesAreNearby) != 0;
        const bool solidGround = !underwater && !inAir;

        if (!solidGround || enemiesNearby)
        {
            if (!solidGround)
                OFBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage1}");

            if (enemiesNearby)
                OFBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage2}");

            OFBase::Environment::get().getWindowManager()->popGuiMode();
            return;
        }

        const bool canSleep = !ptr.isEmpty() || (restFlags & OFBase::World::Rest_CanSleep) != 0;
        setCanRest(canSleep);

        if (mUntilHealedButton->getVisible())
            OFBase::Environment::get().getWindowManager()->setKeyFocusWidget(mUntilHealedButton);
        else
            OFBase::Environment::get().getWindowManager()->setKeyFocusWidget(mWaitButton);
    }

    bool WaitDialog::exit()
    {
        bool canExit = !mTimeAdvancer.isRunning(); // Only exit if not currently waiting
        if (canExit)
        {
            clear();
            stopWaiting();
        }
        return canExit;
    }

    void WaitDialog::clear()
    {
        mSleeping = false;
        mHours = 1;
        mManualHours = 1;
        mFadeTimeRemaining = 0;
        mInterruptAt = -1;
        mTimeAdvancer.stop();
    }

    void WaitDialog::onOpen()
    {
        if (mTimeAdvancer.isRunning())
        {
            mProgressBar.setVisible(true);
            setVisible(false);
            return;
        }
        else
        {
            mProgressBar.setVisible(false);
        }

        if (!OFBase::Environment::get().getWindowManager()->getRestEnabled())
        {
            OFBase::Environment::get().getWindowManager()->popGuiMode();
        }

        onHourSliderChangedPosition(mHourSlider, 0);
        mHourSlider->setScrollPosition(0);

        const OFWorld::DateTimeManager& timeManager = *OFBase::Environment::get().getWorld()->getTimeManager();
        std::string_view month = timeManager.getMonthName();
        int hour = static_cast<int>(timeManager.getTimeStamp().getHour());
        bool pm = hour >= 12;
        if (hour >= 13)
            hour -= 12;
        if (hour == 0)
            hour = 12;

        ESM::EpochTimeStamp currentDate = timeManager.getEpochTimeStamp();
        std::string_view formattedHour(pm ? "#{Calendar:pm}" : "#{Calendar:am}");
        std::string dateTimeText = std::format("{} {} (#{{Calendar:day}} {}) {} {}", currentDate.mDay, month,
            timeManager.getTimeStamp().getDay(), hour, formattedHour);
        mDateTimeText->setCaptionWithReplacing(dateTimeText);
    }

    void WaitDialog::onUntilHealedButtonClicked(MyGUI::Widget* /*sender*/)
    {
        int autoHours = OFBase::Environment::get().getMechanicsManager()->getHoursToRest();

        startWaiting(autoHours);
    }

    void WaitDialog::onWaitButtonClicked(MyGUI::Widget* /*sender*/)
    {
        startWaiting(mManualHours);
    }

    void WaitDialog::startWaiting(int hoursToWait)
    {
        if (Settings::saves().mAutosave) // autosaves when enabled
            OFBase::Environment::get().getStateManager()->quickSave("Autosave");

        OFBase::World* world = OFBase::Environment::get().getWorld();
        OFBase::Environment::get().getWindowManager()->fadeScreenOut(0.2f);
        mFadeTimeRemaining = 0.4f;
        setVisible(false);

        mHours = hoursToWait;

        // FIXME: move this somewhere else?
        mInterruptAt = -1;
        OFWorld::Ptr player = world->getPlayerPtr();
        if (mSleeping && player.getCell()->isExterior())
        {
            const ESM::RefId& regionstr = player.getCell()->getCell()->getRegion();
            if (!regionstr.empty())
            {
                const ESM::Region* region = world->getStore().get<ESM::Region>().find(regionstr);
                if (!region->mSleepList.empty())
                {
                    // figure out if player will be woken while sleeping
                    int x = Misc::Rng::rollDice(hoursToWait, world->getPrng());
                    float fSleepRandMod
                        = world->getStore().get<ESM::GameSetting>().find("fSleepRandMod")->mValue.getFloat();
                    if (x < fSleepRandMod * hoursToWait)
                    {
                        float fSleepRestMod
                            = world->getStore().get<ESM::GameSetting>().find("fSleepRestMod")->mValue.getFloat();
                        int interruptAtHoursRemaining = int(fSleepRestMod * hoursToWait);
                        if (interruptAtHoursRemaining != 0)
                        {
                            mInterruptAt = hoursToWait - interruptAtHoursRemaining;
                            mInterruptCreatureList = region->mSleepList;
                        }
                    }
                }
            }
        }

        mProgressBar.setProgress(0, hoursToWait);
    }

    void WaitDialog::onCancelButtonClicked(MyGUI::Widget* /*sender*/)
    {
        OFBase::Environment::get().getWindowManager()->removeGuiMode(GM_Rest);
    }

    void WaitDialog::onHourSliderChangedPosition(MyGUI::ScrollBar* sender, size_t position)
    {
        mHourText->setCaptionWithReplacing(std::format("{} #{{sRestMenu2}}", position + 1));
        mManualHours = static_cast<int>(position + 1);
        OFBase::Environment::get().getWindowManager()->setKeyFocusWidget(mWaitButton);
    }

    void WaitDialog::onKeyButtonPressed(MyGUI::Widget* /*sender*/, MyGUI::KeyCode key, MyGUI::Char character)
    {
        if (key == MyGUI::KeyCode::ArrowUp)
            mHourSlider->setScrollPosition(
                std::min(mHourSlider->getScrollPosition() + 1, mHourSlider->getScrollRange() - 1));
        else if (key == MyGUI::KeyCode::ArrowDown)
            mHourSlider->setScrollPosition(std::max(static_cast<int>(mHourSlider->getScrollPosition()) - 1, 0));
        else
            return;
        onHourSliderChangedPosition(mHourSlider, mHourSlider->getScrollPosition());
    }

    void WaitDialog::onWaitingProgressChanged(int cur, int total)
    {
        mProgressBar.setProgress(cur, total);
        OFBase::Environment::get().getMechanicsManager()->rest(1, mSleeping);
        OFBase::Environment::get().getWorld()->advanceTime(1);

        OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        if (player.getClass().getCreatureStats(player).isDead())
            stopWaiting();
    }

    void WaitDialog::onWaitingInterrupted()
    {
        OFBase::Environment::get().getWindowManager()->messageBox("#{sSleepInterrupt}");
        OFBase::Environment::get().getWorld()->spawnRandomCreature(mInterruptCreatureList);
        stopWaiting();
    }

    void WaitDialog::onWaitingFinished()
    {
        stopWaiting();

        OFWorld::Ptr player = OFMechanics::getPlayer();
        const OFMechanics::NpcStats& pcstats = player.getClass().getNpcStats(player);

        // trigger levelup if possible
        const OFWorld::Store<ESM::GameSetting>& gmst
            = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
        if (mSleeping && pcstats.getLevelProgress() >= gmst.find("iLevelUpTotal")->mValue.getInteger())
        {
            OFBase::Environment::get().getWindowManager()->pushGuiMode(GM_Levelup);
        }
    }

    void WaitDialog::setCanRest(bool canRest)
    {
        OFWorld::Ptr player = OFMechanics::getPlayer();
        OFMechanics::CreatureStats& stats = player.getClass().getCreatureStats(player);
        bool full = (stats.getHealth().getCurrent() >= stats.getHealth().getModified())
            && (stats.getMagicka().getCurrent() >= stats.getMagicka().getModified());
        OFMechanics::NpcStats& npcstats = player.getClass().getNpcStats(player);
        bool werewolf = npcstats.isWerewolf();

        mUntilHealedButton->setVisible(canRest && !full);
        mWaitButton->setCaptionWithReplacing(canRest ? "#{sRest}" : "#{sWait}");
        mRestText->setCaptionWithReplacing(
            canRest ? "#{sRestMenu3}" : (werewolf ? "#{sWerewolfRestMessage}" : "#{sRestIllegal}"));

        mSleeping = canRest;

        Gui::Box* box = dynamic_cast<Gui::Box*>(mMainWidget);
        if (box == nullptr)
            throw std::runtime_error("main widget must be a box");
        box->notifyChildrenSizeChanged();
        center();
    }

    void WaitDialog::onFrame(float dt)
    {
        mTimeAdvancer.onFrame(dt);

        if (mFadeTimeRemaining <= 0)
            return;

        mFadeTimeRemaining -= dt;

        if (mFadeTimeRemaining <= 0)
        {
            mProgressBar.setVisible(true);
            mTimeAdvancer.run(mHours, mInterruptAt);
        }
    }

    ControllerButtons* WaitDialog::getControllerButtons()
    {
        mControllerButtons.mX.clear();
        if (mSleeping)
        {
            mControllerButtons.mA = "#{Interface:Rest}";
            if (mUntilHealedButton->getVisible())
                mControllerButtons.mX = "#{Interface:UntilHealed}";
        }
        else
        {
            mControllerButtons.mA = "#{Interface:Wait}";
        }
        return &mControllerButtons;
    }

    bool WaitDialog::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            onWaitButtonClicked(mWaitButton);
            OFBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_B)
            onCancelButtonClicked(mCancelButton);
        else if (arg.button == SDL_CONTROLLER_BUTTON_X && mUntilHealedButton->getVisible())
        {
            onUntilHealedButtonClicked(mUntilHealedButton);
            OFBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
            OFBase::Environment::get().getWindowManager()->injectKeyPress(MyGUI::KeyCode::ArrowDown, 0, false);
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
            OFBase::Environment::get().getWindowManager()->injectKeyPress(MyGUI::KeyCode::ArrowUp, 0, false);
        else if (arg.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
        {
            mHourSlider->setScrollPosition(0);
            onHourSliderChangedPosition(mHourSlider, mHourSlider->getScrollPosition());
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
        {
            mHourSlider->setScrollPosition(mHourSlider->getScrollRange() - 1);
            onHourSliderChangedPosition(mHourSlider, mHourSlider->getScrollPosition());
        }

        return true;
    }

    void WaitDialog::stopWaiting()
    {
        OFBase::Environment::get().getWindowManager()->fadeScreenIn(0.2f);
        mProgressBar.setVisible(false);
        OFBase::Environment::get().getWindowManager()->removeGuiMode(GM_Rest);
        mTimeAdvancer.stop();
    }

    void WaitDialog::wakeUp()
    {
        mSleeping = false;
        if (mInterruptAt != -1)
            onWaitingInterrupted();
        else
            stopWaiting();
    }

}

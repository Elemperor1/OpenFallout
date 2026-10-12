#ifndef MWGUI_MESSAGE_BOX_H
#define MWGUI_MESSAGE_BOX_H

#include <cstdint>
#include <memory>

#include "windowbase.hpp"

namespace MyGUI
{
    class Widget;
    class Button;
    class EditBox;
}

namespace OFGui
{
    class InteractiveMessageBox;
    class MessageBoxManager;
    class MessageBox;
    class MessageBoxManager
    {
    public:
        MessageBoxManager(float timePerChar);
        ~MessageBoxManager();
        void onFrame(float frameDuration);
        void createMessageBox(std::string_view message, bool stat = false);
        void removeStaticMessageBox();
        bool createInteractiveMessageBox(std::string_view message, const std::vector<std::string>& buttons,
            bool immediate = false, int defaultFocus = -1);
        bool isInteractiveMessageBox();
        /// The identifier of the interactive message box that is open, 0 when there is none. A box that replaces
        /// another one has an identifier of its own.
        std::uint64_t getInteractiveMessageBoxId() const { return mInterMessageBoxe != nullptr ? mInteractiveId : 0; }
        /// Presses a button of the interactive message box that is open; false when there is none, or it has no such
        /// button
        bool pressInteractiveMessageBoxButton(std::size_t button);

        std::size_t getMessagesCount();

        const InteractiveMessageBox* getInteractiveMessageBox() const { return mInterMessageBoxe.get(); }

        /// Remove all message boxes
        void clear();

        bool removeMessageBox(MessageBox* msgbox);

        /// @param reset Reset the pressed button to -1 after reading it.
        int readPressedButton(bool reset = true);

        void resetInteractiveMessageBox();

        void setLastButtonPressed(int index);

        typedef MyGUI::delegates::MultiDelegate<int> EventHandle_Int;

        // Note: this delegate unassigns itself after it was fired, i.e. works once.
        EventHandle_Int eventButtonPressed;

        void onButtonPressed(int button)
        {
            eventButtonPressed(button);
            eventButtonPressed.clear();
        }

        void setVisible(bool value);

        const std::vector<std::unique_ptr<MessageBox>>& getActiveMessageBoxes() const;

    private:
        std::vector<std::unique_ptr<MessageBox>> mMessageBoxes;
        std::unique_ptr<InteractiveMessageBox> mInterMessageBoxe;
        std::uint64_t mInteractiveId = 0;
        MessageBox* mStaticMessageBox;
        float mMessageBoxSpeed;
        int mLastButtonPressed;
        bool mVisible = true;
    };

    class MessageBox : public Layout
    {
    public:
        MessageBox(MessageBoxManager& parMessageBoxManager, std::string_view message);
        const std::string& getMessage() { return mMessage; }
        int getHeight();
        void update(int height);
        void setVisible(bool value);

        float mCurrentTime;
        float mMaxTime;

    protected:
        MessageBoxManager& mMessageBoxManager;
        std::string mMessage;
        MyGUI::EditBox* mMessageWidget;
        int mBottomPadding;
        int mNextBoxPadding;
    };

    class InteractiveMessageBox : public WindowModal
    {
    public:
        InteractiveMessageBox(MessageBoxManager& parMessageBoxManager, const std::string& message,
            const std::vector<std::string>& buttons, bool immediate, size_t defaultFocus);
        void mousePressed(MyGUI::Widget* widget);
        int readPressedButton();
        /// A click on the button (counted from 0); false when the box has no such button or is already answered
        bool press(std::size_t button);

        MyGUI::Widget* getDefaultKeyFocus() override;

        bool exit() override { return false; }

        bool mMarkedToDelete;

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;

    private:
        void buttonActivated(MyGUI::Widget* widget);

        MessageBoxManager& mMessageBoxManager;
        MyGUI::EditBox* mMessageWidget;
        MyGUI::Widget* mButtonsWidget;
        std::vector<MyGUI::Button*> mButtons;

        int mButtonPressed;
        size_t mDefaultFocus;
        bool mImmediate;
        size_t mControllerFocus = 0;
    };

}

#endif

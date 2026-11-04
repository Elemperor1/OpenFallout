#ifndef COMPONENTS_LOADINGLISTENER_H
#define COMPONENTS_LOADINGLISTENER_H

#include <cstdint>
#include <memory>
#include <string>

namespace Loading
{
    class Listener
    {
    public:
        /// Set a text label to show on the loading screen.
        /// @param label The label
        /// @param important Is the label considered important to show?
        /// @note "non-important" labels may not show on screen if the loading process went so fast
        /// that the implementation decided not to show a loading screen at all. "important" labels
        /// will show in a separate message-box if the loading screen was not shown.
        virtual void setLabel(const std::string& label, bool important = false) {}

        /// Say where the next loading sequence goes, for a load screen with pictures of places: the record number of
        /// the cell (an interior) or of the world (an exterior), the world and, for an exterior, the grid cell the
        /// player arrives in. Call it before loadingOn(); the next loadingOn() takes it, and a sequence that follows
        /// with no destination of its own goes nowhere in particular.
        virtual void setDestination(
            std::uint32_t object, std::uint32_t world, bool exterior, int gridX, int gridY)
        {
        }

        /// Start a loading sequence. Must call loadingOff() when done.
        /// @note To get the loading screen to actually update, you must call setProgress / increaseProgress
        /// periodically.
        /// @note It is best to use the ScopedLoad object instead of using loadingOn()/loadingOff() directly,
        ///  so that the loading is exception safe.
        virtual void loadingOn() {}
        virtual void loadingOff() {}

        /// Set the total range of progress (e.g. the number of objects to load).
        virtual void setProgressRange(size_t range) {}
        /// Set current progress. Valid range is [0, progressRange)
        virtual void setProgress(size_t value) {}
        /// Increase current progress, default by 1.
        virtual void increaseProgress(size_t increase = 1) {}

        virtual ~Listener() = default;
    };

    struct LoadingOff
    {
        void operator()(Listener* listener) const
        {
            if (listener != nullptr)
                listener->loadingOff();
        }
    };

    /// @brief Used for stopping a loading sequence when the object goes out of scope
    struct ScopedLoad
    {
        std::unique_ptr<Listener, LoadingOff> mListener;

        explicit ScopedLoad(Listener* listener)
            : mListener(listener)
        {
            if (mListener != nullptr)
                mListener->loadingOn();
        }
    };
}

#endif

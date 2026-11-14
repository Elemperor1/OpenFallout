#include <components/misc/thread.hpp>

#include <gtest/gtest.h>

#include <thread>

#ifdef __APPLE__
#include <pthread/qos.h>
#endif

namespace
{
    // Every test runs its calls on a fresh thread, so the priority of the test runner's own thread is never changed.
    template <class F>
    void runOnNewThread(F&& f)
    {
        std::thread(std::forward<F>(f)).join();
    }

    TEST(MiscThreadTest, frameCriticalPriorityCanBeSetOnAnyThread)
    {
        runOnNewThread([] { Misc::setCurrentThreadFrameCriticalPriority(); });
    }

    TEST(MiscThreadTest, frameCriticalPriorityCanBeSetTwice)
    {
        runOnNewThread([] {
            Misc::setCurrentThreadFrameCriticalPriority();
            Misc::setCurrentThreadFrameCriticalPriority();
        });
    }

#ifdef __APPLE__
    TEST(MiscThreadTest, frameCriticalPriorityIsUserInteractiveQosOnMac)
    {
        qos_class_t before = QOS_CLASS_UNSPECIFIED;
        qos_class_t after = QOS_CLASS_UNSPECIFIED;
        runOnNewThread([&] {
            before = qos_class_self();
            Misc::setCurrentThreadFrameCriticalPriority();
            after = qos_class_self();
        });
        EXPECT_EQ(before, QOS_CLASS_DEFAULT);
        EXPECT_EQ(after, QOS_CLASS_USER_INTERACTIVE);
    }

    // Threads started by a raised thread (loading, sound, physics, Lua workers) must keep the default class: macOS
    // gives a new pthread the default class, not its creator's, and the engine relies on that.
    TEST(MiscThreadTest, threadsStartedFromAFrameCriticalThreadKeepTheDefaultQos)
    {
        qos_class_t child = QOS_CLASS_UNSPECIFIED;
        runOnNewThread([&] {
            Misc::setCurrentThreadFrameCriticalPriority();
            std::thread([&] { child = qos_class_self(); }).join();
        });
        EXPECT_EQ(child, QOS_CLASS_DEFAULT);
    }
#endif
}

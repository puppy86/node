#include <lib/system/utils.hpp>
#include "gtest/gtest.h"
#include "lib/system/concurrent.hpp"

#include <atomic>
#include <iostream>
#include <string>

using ThreadId = std::thread::id;

template <typename T>
using Ref = std::reference_wrapper<T>;

static const uint64_t sleepTimeMs = 2500;

#define GENERATE_THREAD_VALUES()                        \
    std::atomic<bool> isRunningFinished = false;        \
    ThreadId mainId = std::this_thread::get_id();       \
    ThreadId concurrentId;                              \
    static std::atomic<bool> called = false;            \
    cs::Console::writeLine("Main thread id: ", mainId); \
    (void)called

TEST(Concurrent, SimpleRunWithBinding) {
    GENERATE_THREAD_VALUES();

    class Demo {
    public:
        void method(const std::string& message, Ref<ThreadId> wrapper, Ref<std::atomic<bool>> finished) {
            wrapper.get() = std::this_thread::get_id();

            cs::Console::writeLine("Concurrent thread id: ", wrapper);
            cs::Console::writeLine(message);

            finished.get() = true;
        }
    };

    Demo demo;
    cs::Concurrent::run(std::bind(&Demo::method, &demo, "Finished", std::ref(concurrentId), std::ref(isRunningFinished)));

    while (!isRunningFinished);

    ASSERT_NE(mainId, concurrentId);
}

TEST(Concurrent, SimpleRunWithoutBinding) {
    GENERATE_THREAD_VALUES();

    class Demo {
    public:
        void method(const std::string& message, Ref<ThreadId> wrapper, Ref<std::atomic<bool>> finished) {
            wrapper.get() = std::this_thread::get_id();

            cs::Console::writeLine("Concurrent thread id: ", wrapper);
            cs::Console::writeLine(message);

            finished.get() = true;
        }
    };

    Demo demo;
    cs::Concurrent::run(&Demo::method, &demo, "Finished", std::ref(concurrentId), std::ref(isRunningFinished));

    while (!isRunningFinished);

    ASSERT_NE(mainId, concurrentId);
}

TEST(Concurrent, SimpleRunLambda) {
    GENERATE_THREAD_VALUES();

    auto lambda = [&] {
        concurrentId = std::this_thread::get_id();

        cs::Console::writeLine("Concurrent thread id: ", concurrentId);
        cs::Console::writeLine("Finished");

        isRunningFinished = true;
    };

    cs::Concurrent::run(lambda);
    while (!isRunningFinished);

    ASSERT_NE(mainId, concurrentId);
}

TEST(Concurrent, VoidFutureWatcherBindedRun) {
    GENERATE_THREAD_VALUES();

    class Demo {
    public:
        void method(const std::string& message, Ref<ThreadId> wrapper, Ref<std::atomic<bool>> finished) {
            wrapper.get() = std::this_thread::get_id();

            cs::Console::writeLine("Concurrent thread id: ", wrapper);
            cs::Console::writeLine(message);

            finished.get() = true;
        }

    public slots:
        void onWatcherFinished() {
            cs::Console::writeLine("Watcher finished slot activated");
            called = true;
        }

        void onFailed() {
            cs::Console::writeLine("Execution failed");
        }
    };

    Demo demo;
    std::string message = "Finished";
    auto binder = std::bind(&Demo::method, &demo, message, std::ref(concurrentId), std::ref(isRunningFinished));

    cs::FutureWatcherPtr<void> watcher = cs::Concurrent::run(cs::RunPolicy::ThreadPolicy, std::move(binder));
    cs::Console::writeLine("Not connected yet");

    cs::Connector::connect(&watcher->finished, &demo, &Demo::onWatcherFinished);
    cs::Connector::connect(&watcher->failed, &demo, &Demo::onFailed);

    while (!isRunningFinished);

    if (!called) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeMs));
    }

    if (!called && isRunningFinished) {
        cs::Console::writeLine("Method executed, but does not generate finished signal");
    }

    cs::Console::writeLine("Called value is ", called);
    cs::Console::writeLine("isRunnings value is ", isRunningFinished);

    ASSERT_NE(mainId, concurrentId);
    ASSERT_EQ(called, true);
}

TEST(Concurrent, VoidFutureWatcherNonBindedRun) {
    GENERATE_THREAD_VALUES();

    class Demo {
    public:
        void method(const std::string& message, Ref<ThreadId> wrapper, Ref<std::atomic<bool>> finished) {
            wrapper.get() = std::this_thread::get_id();

            cs::Console::writeLine("Concurrent thread id: ", wrapper);
            cs::Console::writeLine(message);

            finished.get() = true;
        }

    public slots:
        void onWatcherFinished() {
            cs::Console::writeLine("Watcher finished slot activated");
            called = true;
        }

        void onFailed() {
            cs::Console::writeLine("Execution failed");
        }
    };

    Demo demo;
    std::string message = "Finished";

    cs::FutureWatcherPtr<void> watcher = cs::Concurrent::run(cs::RunPolicy::ThreadPolicy, &Demo::method, &demo, message, std::ref(concurrentId), std::ref(isRunningFinished));
    cs::Console::writeLine("Not connected yet");

    // look at watcher
    cs::Connector::connect(&watcher->finished, &demo, &Demo::onWatcherFinished);
    cs::Connector::connect(&watcher->failed, &demo, &Demo::onFailed);

    while (!isRunningFinished);

    if (!called) {
        cs::Console::writeLine("Not called, sleeping...");
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeMs));
    }

    cs::Console::writeLine("Called value is ", called);
    cs::Console::writeLine("isRunnings value is ", isRunningFinished);

    if (!called && isRunningFinished) {
        cs::Console::writeLine("Method executed, but does not generate finished signal");
    }

    ASSERT_NE(mainId, concurrentId);
    ASSERT_EQ(called, true);
}

#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <thread_safe_queue.h>
#include <vector>

TEST(ThreadSafeQueue, SafeQueueTest) {
    ThreadSafeQueue<int> tqueue;
    constexpr int kWriters = 4;
    constexpr int kPerWriters = 10000;

    std::atomic<bool> start{false};
    std::vector<std::thread> writers;

    for (int i = 0; i < kWriters; ++i) {
        writers.emplace_back([&] {
            while (!start.load()) {
            }
            for (int j = 0; j < kPerWriters; ++j) {
                tqueue.push(1);
            }
        });
    }

    long long counter = 0;

    std::thread reader([&] {
        while (!start.load()) {
        }
        while (counter < kWriters * kPerWriters) {
            auto batch = tqueue.drain();
            for (size_t i = 0; i < batch.size(); ++i) {
                ++counter;
            }
            batch.clear();
        }
    });

    start.store(true);
    for (auto& t : writers) {
        t.join();
    }
    reader.join();

    EXPECT_EQ(counter, kWriters * kPerWriters);
}

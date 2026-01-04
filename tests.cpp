// This is made by ChatGPT to help us debug our WSD
#include "wsd.hpp"
#include <atomic>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <unordered_set>
#include <vector>

static constexpr int NUM_TASKS = 200000;
static constexpr int NUM_STEALERS = 8;

struct Task {
    int id;
};

// Helper barrier (C++17 compatible)
class SpinBarrier {
public:
    explicit SpinBarrier(int count) : count(count) {}

    void arrive_and_wait() {
        int old = count.fetch_sub(1);
        while (count.load() > 0) {
            std::this_thread::yield();
        }
    }

private:
    std::atomic<int> count;
};

TEST(CircularWSDequeTest, SingleThreadPushPopFIFOish) {
    CircularWSDeque<Task> dq;

    Task a{1}, b{2}, c{3};

    dq.pushBottom(&a);
    dq.pushBottom(&b);
    dq.pushBottom(&c);

    EXPECT_EQ(dq.popBottom()->id, 3);
    EXPECT_EQ(dq.popBottom()->id, 2);
    EXPECT_EQ(dq.popBottom()->id, 1);
    EXPECT_EQ(dq.popBottom(), CircularWSDeque<Task>::Empty);
}

TEST(CircularWSDequeTest, StealFromOtherThread) {
    CircularWSDeque<Task> dq;
    Task t{42};

    dq.pushBottom(&t);

    std::atomic<Task*> stolen{nullptr};
    std::thread thief([&] {
        stolen.store(dq.steal(), std::memory_order_relaxed);
    });

    thief.join();
    ASSERT_NE(stolen.load(), nullptr);
    EXPECT_EQ(stolen.load()->id, 42);
}

TEST(CircularWSDequeTest, HeavyConcurrentPushPopSteal) {
    CircularWSDeque<Task> dq;

    std::vector<Task> tasks(NUM_TASKS);
    for (int i = 0; i < NUM_TASKS; i++)
        tasks[i].id = i;

    std::vector<std::atomic<int>> seen(NUM_TASKS);
    for (auto& s: seen) s.store(0);

    std::atomic<int> pushed{0};
    std::atomic<int> completed{0};

    SpinBarrier barrier(NUM_STEALERS + 1);

    // Owner thread
    std::thread owner([&] {
        barrier.arrive_and_wait();

        std::mt19937 rng(123);
        std::uniform_int_distribution<int> dist(0, 1);

        while (completed.load() < NUM_TASKS) {
            // push
            int idx = pushed.fetch_add(1);
            if (idx < NUM_TASKS) {
                dq.pushBottom(&tasks[idx]);
            }

            // occasionally pop
            if (dist(rng)) {
                Task* t = dq.popBottom();
                if (t && t != CircularWSDeque<Task>::Empty) {
                    int old = seen[t->id].fetch_add(1);
                    EXPECT_EQ(old, 0);
                    completed.fetch_add(1);
                }
            }
        }
    });

    // Stealer threads
    std::vector<std::thread> stealers;
    for (int i = 0; i < NUM_STEALERS; i++) {
        stealers.emplace_back([&] {
            barrier.arrive_and_wait();
            while (completed.load() < NUM_TASKS) {
                Task* t = dq.steal();
                if (!t || t == CircularWSDeque<Task>::Empty || t == CircularWSDeque<Task>::Abort)
                    continue;

                int old = seen[t->id].fetch_add(1);
                EXPECT_EQ(old, 0);
                completed.fetch_add(1);
            }
        });
    }

    owner.join();
    for (auto& s: stealers) s.join();

    // Validate all tasks seen exactly once
    for (int i = 0; i < NUM_TASKS; i++) {
        EXPECT_EQ(seen[i].load(), 1) << "Task " << i << " seen wrong number of times";
    }
}

TEST(CircularWSDequeTest, ResizeStressTest) {
    CircularWSDeque<Task> dq;

    constexpr int N = 50000;
    std::vector<Task> tasks(N);
    for (int i = 0; i < N; i++) tasks[i].id = i;

    // Force multiple resizes
    for (int i = 0; i < N; i++)
        dq.pushBottom(&tasks[i]);

    std::unordered_set<int> results;
    results.reserve(N);

    for (int i = 0; i < N; i++) {
        Task* t = dq.popBottom();
        ASSERT_NE(t, CircularWSDeque<Task>::Empty);
        results.insert(t->id);
    }

    EXPECT_EQ(results.size(), N);
    EXPECT_EQ(dq.popBottom(), CircularWSDeque<Task>::Empty);
}

//
// Created by fail on 29.11.25.
//

#ifndef PARALLEL_RUNNER_HPP
#define PARALLEL_RUNNER_HPP

#include "LockFreeStack.hpp"

#include "tsptask.hpp"

#include <atomic>
#include <thread>
#include <vector>

class ParallelTaskRunner : public TaskRunner {

private:
    int _num_threads;
    LockFreeStack<Task> _worklist;
    std::atomic<int> _active_tasks;
    std::vector<std::thread> _threads;

    void worker_loop() {
        while (true) {
            Task* task = _worklist.pop();
            if (!task) {
                if (_active_tasks.load(std::memory_order_acquire) == 0)
                    break;  // plus de travail
                std::this_thread::yield();
                continue;
            }


            TaskStack children(TSPPath::MAX_GRAPH);
            int n = task->split(&children);
            if (n > 0) {
                // push enfants dans la pile
                for (int i = 0; i < n; i++) {
                    Task* child = new TSPTask(dynamic_cast<TSPTask*>(task), i);
                    _worklist.push(child);
                    _active_tasks.fetch_add(1, std::memory_order_relaxed);
                }
                delete task;
            } else {
                task->solve();
                delete task;
            }
            _active_tasks.fetch_sub(1, std::memory_order_release);
        }
    }

public:
    ParallelTaskRunner(int num_threads)
        : _num_threads(num_threads), _active_tasks(0) {}

    ~ParallelTaskRunner() {
        for (auto& th : _threads)
            if (th.joinable()) th.join();
    }

    void run(Task* root) override {
        TaskRunner::startTimer();
        std::cout << "ok 1: " << std::endl;
        _worklist.push(root);
        _active_tasks.store(1, std::memory_order_release);
        // créer les threads
        std::cout << "ok 2: " << std::endl;
        for (int i = 0; i < _num_threads; i++) {
            std::cout << "ok 3: " << i << std::endl;
            _threads.emplace_back([this] { this->worker_loop(); });
        }

        std::cout << "ok 4: " << std::endl;
        // attendre la fin
        for (auto& th : _threads)
            th.join();
        std::cout << "ok final: " << std::endl;
        //stopTimer();
    }
};

#endif//PARALLEL_RUNNER_HPP

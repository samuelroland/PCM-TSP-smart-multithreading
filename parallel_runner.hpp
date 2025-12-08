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
                    break; // plus de travail
                std::this_thread::yield();
                continue;
            }
            // debug purpose
            //std::cout << "worker " << std::this_thread::get_id() << " got task " << task << "\n";

            // Créer un conteneur local pour les enfants
            TaskStack children(TSPPath::MAX_GRAPH);
            int n = task->split(&children);

            // Push les enfants dans la pile de travail
            for (int i = 0; i < n; i++) {
                Task* child = children.pop();
                _worklist.push(child);
                _active_tasks.fetch_add(1, std::memory_order_relaxed);
            }

            // Si pas d'enfants, résoudre la tâche
            if (n == 0) {
                task->solve();
            }

            // Supprimer la tâche actuelle
            //delete task;
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
        _worklist.push(root);
        _active_tasks.store(1, std::memory_order_release);

        // Créer les threads
        for (int i = 0; i < _num_threads; i++) {
            _threads.emplace_back([this] { this->worker_loop(); });
        }

        // Attendre la fin
        for (auto& th : _threads)
            th.join();

        stopTimer();
    }
};

#endif // PARALLEL_RUNNER_HPP

#ifndef PARALLEL_WORK
#define PARALLEL_WORK

#include "task.hpp"
#include "tsptask.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

// TODO: transform this collection with CAS using atomic_stamped
// TODO: implement a first queue structure SAMUEL
class FastTaskDS : public TaskCollection {
private:
    std::vector<Task*> _tab;

public:
    FastTaskDS(int cap) { _tab.reserve(cap); }
    int size() const override { return _tab.size(); }
    Task* operator[](int i) override { return _tab[i]; }
    void push(Task* t) override {
        _tab.push_back(t);
    }
    Task* pop() override {
        if (_tab.size() <= 0)
            throw std::runtime_error("TaskStack empty!");
        Task* ret = _tab.back();
        _tab.pop_back();
        return ret;
    }
    void clear() override { _tab.clear(); }
};


/*****************************************************************
  TSPTask class, extends Task
  Should call (static) TSPPath::setup() using a TSPGraph before
  creating TSPPath objects.
  Methods:
    reusealloc()/reusefree() replace new/delete (reuse tasks)
    cutoff(c) sets a cutoff size (from the end of a full path)
    result() gets the result after solve() or merge()
 *****************************************************************/

// TODO: make TSPparatask inherit from TspTask to get the default impl and only change reusealloc, reusefree and split

class TSPParraTask : public Task {

private:
    static TSPPath _shortest;
    static FastTaskDS* _free_list;

    TSPParraTask* reusealloc(int node) {
        return new TSPParraTask(this, node);// TODO: fix that
    }

    void reusefree(TSPParraTask* p) {
        delete p;// TODO: fix that
    }

    TSPPath _path;
    int _cutoff_size;

    TSPParraTask(TSPParraTask* task, int node) : _path(task->_path), _cutoff_size(task->_cutoff_size) {
        _path.push(node);
    }

public:
    TSPParraTask() { _cutoff_size = TSPPath::full(); }
    ~TSPParraTask() override = default;

    // cutoff set, expressed as a distance from full path
    void cutoff(int c) { _cutoff_size = TSPPath::full() - c; }
    TSPPath& result() { return _shortest; }

    // Task interface implementation: split, merge, solve, write

    int split(TaskCollection* collection) override {
        collection->clear();
        if (_path.size() >= _cutoff_size) return 0;
        if (_path.distance() >= _shortest.distance()) return -1;// the branch must be cut
        int count = 0;
        for (int i = 0; i < TSPPath::full(); i++) {
            if (!_path.contains(i)) {
                //				TSPParraTask* t  = new TSPParraTask(this, i);
                TSPParraTask* t = reusealloc(i);
                collection->push(t);
                count++;
            }
        }
        return count;
    }

    void merge(TaskCollection* collection) override {
        for (int p = 0; p < collection->size(); p++) {
            TSPParraTask* t = (TSPParraTask*) collection->pop();
            //			delete t;
            reusefree(t);
        }
    }

    void solve() override {
        //std::cout << "solving " << _path << "\n";
        if (_path.size() == TSPPath::full()) {
            _path.push(TSPPath::FIRST_NODE);// last node = first node
            if (_path.distance() < _shortest.distance())
                _shortest = _path;
            _path.pop();
        } else {
            for (int i = 0; i < TSPPath::full(); i++) {
                if (!_path.contains(i)) {
                    _path.push(i);
                    if (_path.distance() < _shortest.distance())
                        solve();
                    _path.pop();
                }
            }
        }
    }

    void write(std::ostream& os) const override {
        std::cout << "Task(c=" << _cutoff_size << ')' << _path;
    }
};


// TODO: change the implementation to start threads
// TODO: OLIVIA
class ParallelTaskRunner : public TaskRunner {
private:
    int _size;
    int _splits;
    int _solves;

    // variables for thread pool
    std::vector<std::thread> workers;// list with all threads ready to work
    std::condition_variable _cv;     // condition variable to signal changes in the state of the tasks queue
    std::mutex _queue_mutex;         // Mutex to synchronize access to shared data
    std::queue<std::function<void()>> _tasks;
    std::atomic<bool> _stop{false};        // Flag to indicate whether the thread pool should stop or not
    std::atomic<int> _tasks_in_progress{0};// to check the finalization of computing

    void recurse(Task* t) {
        // 		Task* space[_size];
        //		FixedTaskStack coll(space, _size);
        FastTaskDS coll(_size);
        int n = t->split(&coll);
        if (n < 0) return;// the split has defined we are over the shortest path on this branch so we cut the branch
        // TODO: should we free the task somehow ?
        if (n > 0) {
            _splits++;
            for (int i = 0; i < n; i++) {
                Task* sub = coll[i];
                enqueue([this, sub]() {
                    recurse(sub);
                });
            }
            // ici merge n'attend pas les sous taches. voulu? > core dump!
            //t->merge(&coll);
        } else {
            _solves++;
            t->solve();
        }
        // TODO: implement the glouton approach by giving an empty FastTaskDS to split(), then keeping a task to continue, and pushing other in the global FastTaskDS
    }

    ParallelTaskRunner() {}// cannot use default constructor

    // manage work for thread
    void worker() {
        while (true) {
            std::function<void()> cur_task;
            {
                // Locking the queue so that data
                // can be shared safely
                std::unique_lock<std::mutex> lock(_queue_mutex);


                // Waiting until there is a task to
                // execute or the pool is stopped
                _cv.wait(lock, [this] {
                    return !_tasks.empty() || _stop.load();
                });

                // exit the thread in case the pool
                // is stopped and there are no tasks
                if (_stop.load() && _tasks.empty()) {
                    return;
                }

                // Get the next task from the queue
                cur_task = move(_tasks.front());
                _tasks.pop();
            }
            cur_task();
        }
    }
    // spin wait until all tasks are done
    void wait_until_done() {
        while (_tasks_in_progress.load() > 0) {
            std::this_thread::yield();// CPU can do something else
        }
    }

public:
    ParallelTaskRunner(int size, unsigned int nbThreads) : _size(size), _splits(0), _solves(0) {
        // create thread pool
        for (unsigned int i = 0; i < nbThreads; ++i)
            workers.emplace_back(&ParallelTaskRunner::worker, this);
    }
    ~ParallelTaskRunner() {
        _stop.store(true);
        // Notify all threads
        _cv.notify_all();

        // Joining all worker threads to ensure they have
        // completed their tasks
        for (auto& thread: workers) {
            thread.join();
        }
    }
    virtual void run(Task* t) override {
        TaskRunner::startTimer();
        // To "start" a thread, let's enqueue
        enqueue([this, t]() {
            recurse(t);
        });

        wait_until_done();
        TaskRunner::stopTimer();
    }
    int solves() { return _solves; }
    int splits() { return _splits; }

    // Enqueue task for execution by the thread pool
    void enqueue(std::function<void()> task) {
        _tasks_in_progress++;
        {
            std::unique_lock<std::mutex> lock(_queue_mutex);
            _tasks.emplace([this, task]() {
                task();
                _tasks_in_progress--;
            });
        }
        _cv.notify_one();
    }
};


TSPPath TSPParraTask::_shortest = [] { TSPPath s; s.maximise(); return s; }();
#endif

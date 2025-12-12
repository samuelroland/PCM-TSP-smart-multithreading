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

/// --------------------------------------------------------
/// Queue lock-free pour Task*
// Given in course, chap5 in java, inspired from Michael & Scott,
/// --------------------------------------------------------
class LockFreeQueue {
private:
    struct Node {
        Task* value;
        std::atomic<Node*> next;
        Node(Task* t) : value(t), next(nullptr) {}
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

public:
    LockFreeQueue() {
        Node* dummy = new Node(nullptr); // dummy node
        head.store(dummy);
        tail.store(dummy);
    }

    ~LockFreeQueue() {
        // delete remaining nodes
        Node* node = head.load();
        while (node) {
            Node* next = node->next.load();
            delete node;
            node = next;
        }
    }

    void enqueue(Task* t) {
        Node* node = new Node(t);
        while (true) {
            Node* last = tail.load(std::memory_order_acquire);
            Node* next = last->next.load(std::memory_order_acquire);

            if (last == tail.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (last->next.compare_exchange_weak(next, node,
                            std::memory_order_release,
                            std::memory_order_relaxed)) {
                        tail.compare_exchange_weak(last, node,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        return;
                    }
                } else {
                    tail.compare_exchange_weak(last, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                }
            }
        }
    }

    bool dequeue(Task*& result) {
        while (true) {
            Node* first = head.load(std::memory_order_acquire);
            Node* last = tail.load(std::memory_order_acquire);
            Node* next = first->next.load(std::memory_order_acquire);

            if (first == head.load(std::memory_order_acquire)) {
                if (first == last) {
                    if (next == nullptr)
                        return false; // queue vide
                    tail.compare_exchange_weak(last, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                } else {
                    result = next->value;
                    if (head.compare_exchange_weak(first, next,
                            std::memory_order_release,
                            std::memory_order_relaxed)) {
                        delete first; // supprimer ancien dummy
                        return true;
                    }
                }
            }
        }
    }
};

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
    /*
     * param:
     *  collection : get a collection to full (will be overridden)
     * Return:
     *  return the number of item in TaskCollection
     * return 0 : must be resolve
     * return < 0: cut the branch, distance is too long
     * return > 0: continue to split
     */
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

    /*
     * delete all tasks in the collection
     * be sure that nobody use the task after this point and no reference exists for these tasks!
     */
    void merge(TaskCollection* collection) override {
        for (int p = 0; p < collection->size(); p++) {
            TSPParraTask* t = (TSPParraTask*) collection->pop();
            delete t;
            //reusefree(t);
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


/*
 * Thread pool
 * - manages fixed threads
 * - holds LockFreeQueue _tasks (global, shared)
 * - enqueue task in _tasks pool after split
 */

class ParallelTaskRunner : public TaskRunner {
private:
    std::atomic<int> _size;
    std::atomic<int> _splits;
    std::atomic<int> _solves;

    // variables for thread pool
    std::vector<std::thread> workers;// list with all threads ready to work
    //std::condition_variable _cv;     // condition variable to signal changes in the state of the tasks queue
    //std::mutex _queue_mutex;         // Mutex to synchronize access to shared data
    LockFreeQueue _tasks;
    std::atomic<bool> _stop{false};        // Flag to indicate whether the thread pool should stop or not
    std::atomic<int> _tasks_in_progress{0};// to check the finalization of computing

    void recurse(Task* t) {
        // 		Task* space[_size];
        //		FixedTaskStack coll(space, _size);
        FastTaskDS coll(_size);
        int n = t->split(&coll);
        if (n < 0) {
            //std::cout << "cut the branch " << "\n";
            // TODO: delete task in this collection?
            return;// the split has defined we are over the shortest path on this branch so we cut the branch
        }
        if (n > 0) {
            _splits++;
            for (int i = 0; i < n; i++) {
                Task* sub = coll[i];
                enqueue(sub);
            }
            // ici merge n'attend pas les sous taches. voulu? > core dump!

        } else {
            _solves++;
            t->solve();
            // TODO: delete task after done (leaf task). Should we manage it in another way?

        }
        // TODO: implement the glouton approach by giving an empty FastTaskDS to split(), then keeping a task to continue, and pushing other in the global FastTaskDS
    }

    ParallelTaskRunner() {}// cannot use default constructor

    // manage work for thread
    void worker() {
        while (true) {
            Task* t = nullptr;
            if (!_tasks.dequeue(t)) {
                if (_stop.load() && _tasks_in_progress.load() == 0)
                    return;
                std::this_thread::yield();
                continue;
            }
            recurse(t);
            _tasks_in_progress--;
            //delete t; // TODO: need to be done but create core dump!
        }
            /*{
                std::unique_lock<std::mutex> lock(_queue_mutex);
                _cv.wait(lock, [this] {
                        return !_tasks.empty() || _stop.load();
                    });
                if (_stop.load() && _tasks.empty())
                    return;
                t = _tasks.front(); // take the task
                _tasks.pop();       // remove from the queue
            }
            recurse(t);
            _tasks_in_progress--;
            // TODO: suggestion: remove merge and delete task here, when it's out of the queue
            //delete t;   // remove the task here, when it's done
        }*/
    }

public:
    ParallelTaskRunner(int size, unsigned int nbThreads) : _size(size), _splits(0), _solves(0) {
        // create thread pool, put at the end of the queue
        for (unsigned int i = 0; i < nbThreads; ++i)
            workers.emplace_back(&ParallelTaskRunner::worker, this); // emplace_back, like push_back but create objet in the call
    }
    // never called ;-)
    ~ParallelTaskRunner() {
        _stop.store(true);
        // Notify all threads
        //_cv.notify_all();

        // Joining all worker threads to ensure they have completed their tasks
        for (auto& thread: workers) {
            thread.join();
        }
    }
    virtual void run(Task* rootTask) override {
        TaskRunner::startTimer();
        // give the first task to be consumed by the thread pool
        enqueue(rootTask);
        // wait until all threads finished
        while (_tasks_in_progress.load() > 0)
            std::this_thread::yield();

        TaskRunner::stopTimer();
    }
    int solves() { return _solves; }
    int splits() { return _splits; }

    // Enqueue task for execution by the thread pool
    void enqueue(Task* t) {
        _tasks_in_progress++;
        _tasks.enqueue(t);
        /*{
                std::unique_lock<std::mutex> lock(_queue_mutex);
                _tasks.push(t);
        }*/
        //_cv.notify_one();
    }
};


TSPPath TSPParraTask::_shortest = [] { TSPPath s; s.maximise(); return s; }();
#endif

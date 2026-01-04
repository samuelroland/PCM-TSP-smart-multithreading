#ifndef PARALLEL_WORK
#define PARALLEL_WORK

#include "LockFreeQueue.hpp"
#include "task.hpp"
#include "tsptask.hpp"
#include "util.hpp"
#include "wsd.hpp"

#include <array>
#include <atomic>
#include <thread>


/// Pre calculation of subtree sizes based on the tree height.
/// This is done at compile time only to avoid any runtime cost
/// We are using long double to store 16 bytes integers, instead of 8 bytes long long that are overflowing
///
/// Here are the first 10 results as a preview
/// 0 => 0 -> no tree, zero node
/// 1 => 1
/// 2 => 2
/// 3 => 5 -> height of 3 -> level 0: 1, level 1: 2, level 2: 2. sum=1+2+2=5
/// 4 => 16
/// 5 => 65
/// 6 => 326
/// 7 => 1957
/// 8 => 13700
/// 9 => 109601
constexpr int MAX_CITIES = 25;
constexpr long double subtree_nodes_count(int tree_height) {
    long double nodes_count = 0.0L;
    long double branches_at_level_i = 1.0L;// a virtual branch holding the root node

    // Example of execution: for a 4 cities tour (tree_height = 4)
    // we use the city 1 as the start and the 3 remaining cities 2,3,4
    // are used to create 3 branches on second level,
    // then 3*2 branches on third level,
    // and finally 3*2*1 branches on fourth level.
    // The sum of all branches is 1 + 3 + 6 + 6 = 16 nodes in total

    // remaining_nodes is the number of "other cities" in a tour as the first city is chosen, thus the height -1
    for (int remaining_nodes = tree_height - 1; remaining_nodes >= 0; remaining_nodes--) {
        nodes_count += branches_at_level_i;
        branches_at_level_i *= remaining_nodes;
    }
    return nodes_count;
}

constexpr auto make_subtree_nodes_count_table() {
    std::array<long double, MAX_CITIES + 1> t{};
    for (int k = 0; k <= MAX_CITIES; ++k) {
        t[k] = subtree_nodes_count(k);
    }
    return t;
}

constexpr auto SUBTREE_NODES_COUNT_BY_TREE_HEIGHT = make_subtree_nodes_count_table();


class FastTaskDS;
class TSPParraTask;


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
    static LockFreeQueue<Task>* _free_list;

    double estimated_cost;// actual distance + heuristic

    // DEBUG
    static std::atomic<long> _counter;
    int _id;

    // Get a free task from free_list or allocate a new one
    TSPParraTask* reusealloc(int node) {
        Task* existing_task = nullptr;
        if (_free_list->dequeue(existing_task)) {
            auto existing_para_task = (TSPParraTask*) existing_task;
            *existing_para_task = *this;
            existing_para_task->init(node);
            DEBUG "Allocated new task " << *existing_para_task;
            return existing_para_task;
        } else {
            Task* new_task = new TSPParraTask(this, node);
            DEBUG "Allocated new task " << *new_task;
            return (TSPParraTask*) new_task;
        }
    }

    TSPPath _path;
    int _cutoff_size;

    TSPParraTask(TSPParraTask* task, int node) : _path(task->_path), _cutoff_size(task->_cutoff_size), _id(_counter++) {
        init(node);
    }

    // This is used to fill an existing TSPParraTask on the free_list
    TSPParraTask& operator=(TSPParraTask& task) {
        this->_path = task._path;
        this->_cutoff_size = task._cutoff_size;
        this->_id = _counter++;
        return *this;
    }

    void init(int node) {
        _path.push(node);
        estimated_cost = _path.distance() - _path.size();// actual distance - path already done > probably a better way
    }


public:
    // Release a task and put it in the free_list
    static void reusefree(TSPParraTask* p) {
        TRACE "Freeing task " << *p;
        _free_list->enqueue(p);
    }


    TSPParraTask() : _id(_counter++) {
        _cutoff_size = TSPPath::full();
    }
    ~TSPParraTask() override {
    }

    double get_estimated_cost() const { return estimated_cost; }

    TSPPath& path() { return _path; }


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
     * return < 0: cut the branch, distance is too long, return the nb of node cut
     * return > 0: continue to split
     */
    int split(TaskCollection* collection) override {
        collection->clear();
        if (_path.size() >= _cutoff_size) return 0;
        if (_path.distance() >= _shortest.distance()) return -(TSPPath::full() - _path.size());// return a negative value as a marker
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
            reusefree(t);
        }
    }

    void solve() override {
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
        os << "Task(id=" << _id << ",c=" << _cutoff_size << ')' << _path;
    }
};
// TODO: transform this collection with CAS using atomic_stamped
// TODO: implement a first queue structure SAMUEL
class FastTaskDS : public TaskCollection {
private:
    std::vector<TSPParraTask*> _tab;

public:
    FastTaskDS(int cap) { _tab.reserve(cap); }
    int size() const override { return _tab.size(); }
    Task* operator[](int i) override { return (Task*) _tab[i]; }

    void push(Task* t) {
        TSPParraTask* tp = static_cast<TSPParraTask*>(t);
        // sorted insert
        auto it = std::upper_bound(_tab.begin(), _tab.end(), tp,
                                   [](TSPParraTask* a, TSPParraTask* b) { return a->get_estimated_cost() < b->get_estimated_cost(); });
        _tab.insert(it, tp);
    }
    Task* pop() override {
        if (_tab.size() <= 0)
            throw std::runtime_error("TaskStack empty!");
        TSPParraTask* ret = _tab.back();
        _tab.pop_back();
        return (Task*) ret;
    }
    void clear() override { _tab.clear(); }
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
    const int _cutoff;// a copy of the cutoff (the complement of the cutoff size !) to let recurse() access it
    const int _nbThreads;

    // variables for thread pool
    std::vector<std::thread> workers;                         // list with all threads ready to work
    std::vector<std::unique_ptr<CircularWSDeque<Task>>> _wsds;// work stealing deques for each thread
    std::atomic<uint64_t> _remaining_tasks_count;

    void recurse(Task* t, unsigned tid) {
        // 		Task* space[_size];
        //		FixedTaskStack coll(space, _size);
        FastTaskDS coll(_size);
        int n = t->split(&coll);
        TRACE "Calling recurse with tid " << tid << " on task " << *t << " with split returned " << n;
        // The path has been cut because it is too long
        if (n < 0) {
            // as n is negative to be marker in the returned value, we need to change it in positive again, thus the -n
            _remaining_tasks_count.fetch_sub(SUBTREE_NODES_COUNT_BY_TREE_HEIGHT[-n + 1], std::memory_order_relaxed);// subtree of task including current one (thus +1)
            t->merge(&coll);
            TSPParraTask::reusefree((TSPParraTask*) t);
            return;// the split has defined we are over the shortest path on this branch so we cut the branch
        }
        // The path need to be explored further, let's continue with given subtasks
        if (n > 0) {
            _remaining_tasks_count.fetch_sub(1, std::memory_order_relaxed);// current task is done
            _splits++;
            // keep the first task selfishly
            Task* next_local = coll.pop();
            TRACE "Keeping next_local task " << *next_local;
            for (int i = 1; i < n; i++) {
                Task* sub = coll.pop();
                TRACE "Enqueuing task " << *sub;
                enqueue(sub, tid);
            }
            TSPParraTask::reusefree((TSPParraTask*) t);
            // continue with local task
            recurse(next_local, tid);
        } else {// the current path has reached the end of the tree or the cutoff was reached, we need to solve the task
            // The number of nodes in the subtree of height cutoff + 1 (because the cutoff is stopped at the parent task)
            // This also work with a zero cutoff, 0 + 1 = 1, SUBTREE_NODES_COUNT_BY_TREE_HEIGHT[1] = 1
            // We can do this before the solve, to possibly allow threads to stop before we end our last solve()
            long toremove = SUBTREE_NODES_COUNT_BY_TREE_HEIGHT[_cutoff + 1];
            _remaining_tasks_count.fetch_sub(toremove, std::memory_order_relaxed);

            _solves++;
            t->solve();
            TSPParraTask::reusefree((TSPParraTask*) t);
        }
        // TODO: implement the glouton approach by giving an empty FastTaskDS to split(), then keeping a task to continue, and pushing other in the global FastTaskDS
    }

    ParallelTaskRunner() : _nbThreads(0), _cutoff(0) {}// cannot use default constructor

    // The entrypoint for a thread, with a thread id (tid)
    void worker(unsigned tid) {
        unsigned nextTidToStealFrom = 0;
        while (true) {
            Task* t = _wsds[tid]->popBottom();
            // STEALING STRATEGY
            if (t == CircularWSDeque<Task>::Empty) {
                // TODO: good idea to check that after stealing or before ?
                if (_remaining_tasks_count == 0) {
                    TRACE "exiting thread " << tid;
                    return;
                }
                // Try to steal the next thread
                t = _wsds[nextTidToStealFrom]->steal();
                if (t != CircularWSDeque<Task>::Empty && t != CircularWSDeque<Task>::Abort) {
                    // yoopi we stole a task !
                    TRACE "thread " << tid << " stole task on thread " << nextTidToStealFrom << ": " << *t;
                } else {
                    if (t == CircularWSDeque<Task>::Empty) {
                        TRACE "thread " << tid << " failed to steal with EMPTY on thread " << nextTidToStealFrom;
                        nextTidToStealFrom = (nextTidToStealFrom + 1) % _nbThreads;
                    }
                    if (t == CircularWSDeque<Task>::Abort) {
                        TRACE "thread " << tid << " failed to steal with ABORT on thread " << nextTidToStealFrom;
                        std::this_thread::yield();
                    }
                    continue;
                }
            }
            recurse(t, tid);
        }
    }

public:
    ParallelTaskRunner(int size, unsigned int nbThreads, int cutoff) : _size(size), _nbThreads(nbThreads), _splits(0), _solves(0), _cutoff(cutoff) {}

    virtual void run(Task* rootTask) override {
        TaskRunner::startTimer();

        _remaining_tasks_count.store(SUBTREE_NODES_COUNT_BY_TREE_HEIGHT[TSPPath::full()]);
        _wsds.reserve(_nbThreads);// reserve space to avoid reallocation for push_back
        for (unsigned int i = 0; i < _nbThreads; ++i) {
            _wsds.emplace_back(std::make_unique<CircularWSDeque<Task>>());
        }

        // TODO: try to better init the wsds to avoid too much stealing at first
        // or even fully init them before starting threads ??
        // FastTaskDS coll(_size);
        // int n = rootTask->split(&coll);
        // rootTask->split(TaskCollection *collection)

        // give the first task to be consumed by the thread pool
        enqueue(rootTask, 0);

        for (unsigned int i = 0; i < _nbThreads; ++i) {
            workers.emplace_back(&ParallelTaskRunner::worker, this, i);// emplace_back, like push_back but create objet in the call
        }
        // wait until all threads finished
        while (_remaining_tasks_count.load(std::memory_order_relaxed) > 0)
            std::this_thread::yield();

        // Joining all worker threads to ensure they have completed their tasks
        for (auto& thread: workers) {
            thread.join();
        }
        TaskRunner::stopTimer();
    }
    int solves() { return _solves; }
    int splits() { return _splits; }

    // Enqueue task for execution by the thread pool
    void enqueue(Task* t, unsigned tid) {
        _wsds[tid]->pushBottom(t);
    }
};


TSPPath TSPParraTask::_shortest = [] { TSPPath s; s.maximise(); return s; }();
std::atomic<long> TSPParraTask::_counter{0};
LockFreeQueue<Task>* TSPParraTask::_free_list = new LockFreeQueue<Task>;
#endif

#ifndef PARALLEL_WORK
#define PARALLEL_WORK

#include "task.hpp"
#include "tsptask.hpp"

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
        //		std::cout << "solving " << _path << "\n";
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
class ParallelTaskRunner : public TaskRunner {
private:
    int _size;
    int _splits;
    int _solves;
    void recurse(Task* t) {
        // 		Task* space[_size];
        //		FixedTaskStack coll(space, _size);
        TaskStack coll(_size);
        int n = t->split(&coll);
        if (n) {
            _splits++;
            for (int i = 0; i < n; i++)
                recurse(coll[i]);
            t->merge(&coll);
        } else {
            _solves++;
            t->solve();
        }
    }
    ParallelTaskRunner() {}// cannot use default constructor
public:
    ParallelTaskRunner(int size) : _size(size), _splits(0), _solves(0) {}
    virtual void run(Task* t) override {
        TaskRunner::startTimer();
        recurse(t);
        TaskRunner::stopTimer();
    }
    int solves() { return _solves; }
    int splits() { return _splits; }
};


TSPPath TSPParraTask::_shortest = [] { TSPPath s; s.maximise(); return s; }();
#endif

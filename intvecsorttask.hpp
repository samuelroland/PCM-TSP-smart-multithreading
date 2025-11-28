#include <random>
#include <vector>

#include "task.hpp"

/*****************************************************************
  IntVecSortTask class, extends Task
  Methods:
    reusealloc()/reusefree() replace new/delete (reuse tasks)
    randomize(s) fills the array with s random elements
    result() gets the resulting array after solve() or merge()
 *****************************************************************/

class IntVecSortTask : public Task {

private:
    std::vector<int> _data;
    static std::vector<IntVecSortTask*> _free_list;

    // this does not work with multiple threads!
    IntVecSortTask* reusealloc() {
        if (_free_list.empty())
            return new IntVecSortTask();
        IntVecSortTask* p = _free_list.back();
        _free_list.pop_back();
        p->_data.clear();
        return p;
    }

    // this does not work with multiple threads!
    void reusefree(IntVecSortTask* p) {
        _free_list.push_back(p);
    }

public:
    IntVecSortTask() {}
    ~IntVecSortTask() override = default;

    void randomize(int size) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, size * 10);
        _data.clear();
        for (int i = 0; i < size; i++)
            _data.push_back(dist(gen));
    }

    std::vector<int>& result() {
        return _data;
    }

    // Task interface implementation: split, merge, solve, write

    int split(TaskCollection* coll) override {
        if (_data.size() < 3) return 0;
        //		IntVecSortTask* leftt = new IntVecSortTask();
        //		IntVecSortTask* rightt = new IntVecSortTask();
        IntVecSortTask* leftt = reusealloc();
        IntVecSortTask* rightt = reusealloc();
        std::vector<int>& left = leftt->_data;
        std::vector<int>& right = rightt->_data;
        int pivot = _data[0];
        for (int i = 1; i < _data.size(); i++) {
            if (_data[i] < pivot)// could be <=
                left.push_back(_data[i]);
            else
                right.push_back(_data[i]);
        }
        left.push_back(pivot);// could be right
        coll->push(rightt);
        coll->push(leftt);// will pop left first
        return 2;
    }

    void merge(TaskCollection* coll) override {
        _data.clear();
        while (coll->size()) {
            IntVecSortTask* t = (IntVecSortTask*) coll->pop();
            for (int v: t->_data)
                _data.push_back(v);
            //			delete t;
            reusefree(t);
        }
    }

    void solve() override {
        if (_data.size() < 2) return;
        std::sort(_data.begin(), _data.end());
    }

    void write(std::ostream& os) const override {
        os << '{';
        int size = _data.size();
        if (size)
            os << _data[0];
        int tail = 1;
        if (size >= 10) {
            for (int i = 1; i < 5; i++)
                os << ' ' << _data[i];
            os << " ...";
            tail = size - 5;
        }
        for (int i = tail; i < size; i++)
            os << ' ' << _data[i];
        os << '}';
    }
};

std::vector<IntVecSortTask*> IntVecSortTask::_free_list;

#include <random>
#include <vector>

#include "task.hpp"

class IntVecSortTask : public Task {

private:
    std::vector<int> _data;

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
        IntVecSortTask* leftt = new IntVecSortTask();
        IntVecSortTask* rightt = new IntVecSortTask();
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
            delete t;
        }
    }

    void solve() override {
        if (_data.size() < 2) return;
        std::sort(_data.begin(), _data.end());
    }

    void write(std::ostream& os) const override {
        os << '{';
        int size = _data.size();
        if (size < 10)
            for (int i = 0; i < size; i++) {
                if (i) os << ' ';
                os << _data[i];
            }
        else {
            for (int i = 0; i < 5; i++) {
                if (i) os << ' ';
                os << _data[i];
            }
            os << " ...";
            for (int i = size - 5; i < size; i++)
                os << ' ' << _data[i];
        }
        os << '}';
    }
};

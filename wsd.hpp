// An implementation of the "Dynamic Circular Work-Stealing Deque" as presented in paper of the same name, from David Chase and Yossi Lev
// TODO: provide link to paper PDF
// This implements the basic algorithm presented in the paper without the further improvements.
// TODO: correct ?
//
// Adaptation
// 1. To avoid storing tasks directly but pointers to task like in the Java in the paper, we use a T** segment
// 2. The casTop() method has been replaced by compare_excha TODO
// 3. Added destructor to CircularWSDeque
//
#ifndef WSD
#define WSD

#include <atomic>
#include <cstdlib>

// First structure defined in Figure 4: Growable Circular Array
// It grows by doubling the capacity each time the size is reached
template<typename T>
class CircularArray {
private:
    int log_size;    // the segment has a size of 2^log_size
    long stored_size;// 2^log_size to avoid recalculation each time we call size
    T** segment;

public:
    CircularArray<T>(int log_size) : log_size(log_size), stored_size(1 << log_size) {
        segment = (T**) malloc(stored_size * sizeof(T**));
    }
    ~CircularArray() {
        // NOTE: an idea would be to shallow free and free tasks from the task free list only  ??
        free(segment);
        segment = nullptr;
        // TODO: should we delete tasks as well ???
    }
    long size() {
        return stored_size;
    }
    T* get(long i) {
        return segment[i % stored_size];
    }
    void put(long i, T* new_object) {
        segment[i % stored_size] = new_object;
    }
    CircularArray<T>* grow(long bottom_index, long top_index) {
        CircularArray<T>* a = new CircularArray(log_size + 1);
        for (long i = top_index; i < bottom_index; i++) {
            a->put(i, get(i));
        }
        return a;
    }
};


template<typename T>
class CircularWSDeque {
public:
    static T* Empty;
    static T* Abort;
    static int LogInitialSize;

private:
    long bottom = 0;
    std::atomic<long> top = 0;
    CircularArray<T>* activeArray = new CircularArray<T>(LogInitialSize);
    // TODO: current segfault a probably caused by these issues here, race conditions between steal and pushbottom or grow(). when I remove the stealing part it seems segfault disappear
    // by commenting line "nextTidToStealFrom = (nextTidToStealFrom + 1) % _nbThreads;" in wsd.hpp and running "./tsp dj38.tsp 14 10 8", this is segfaulting all the time for me
    // TODO: refactor the management of activeArray to maybe have atomic pointers ? at least we don't want to do copies like CircularArray a = activeArray;
    // this needs further understanding of the algo in the paper...

public:
    void pushBottom(T* new_object) {
        // std::cout << "pushBottom of " << *o << std::endl;
        long b = bottom;
        long t = top;
        CircularArray<T>* a = activeArray;
        long size = b - t;
        if (size >= a->size() - 1) {
            a = a->grow(b, t);
            activeArray = a;
        }
        a->put(b, new_object);
        bottom = b + 1;
    }

    T* popBottom() {
        // std::cout << "popBottom\n";
        long b = bottom;
        CircularArray<T>* a = activeArray;
        b = b - 1;
        bottom = b;
        long t = top;
        long size = b - t;
        if (size < 0) {
            bottom = t;
            // std::cout << "pop got empty...\n";
            return Empty;
        }
        T* o = a->get(b);
        if (size > 0)
            return o;
        if (!top.compare_exchange_strong(t, t + 1))// TODO: strong or weak option ? memory order to specify or not ?
            o = Empty;
        bottom = t + 1;
        // std::cout << "pop got -> ptr ";
        // printf("%p\n", o);
        return o;
    }

    T* steal() {
        // std::cout << "steal !\n";
        long t = top;
        long b = bottom;
        CircularArray<T>* a = activeArray;
        long size = b - t;
        if (size <= 0) return Empty;
        T* o = a->get(t);
        if (!top.compare_exchange_strong(t, t + 1))// TODO: strong or weak option ? memory order to specify or not ?
            return Abort;
        return o;
    }
};

template<typename T>
int CircularWSDeque<T>::LogInitialSize = 5;// TODO: make it bigger or initialize that at the start depending on the size of the problem

// This is a hack to avoid sentinel values in a struct, that would add some additionnals byte to allocate
// This is using special static pointers that are returned to mean Empty or Abort, keeping the 8 bytes size of a pointer
// TODO: is this okay or should we change it ?
static long a = 1;
template<typename T>
T* CircularWSDeque<T>::Empty = nullptr;
template<typename T>
T* CircularWSDeque<T>::Abort = reinterpret_cast<T*>(&a);

#endif

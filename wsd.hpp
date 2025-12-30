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

// TODO: I used T in expecting to receive Task*, should we switch to Task, and use T* instead ?
// First structure defined in Figure 4: Growable Circular Array
// It grows by doubling the capacity each time the size is reached
template<typename T>
class CircularArray {
private:
    int log_size;// the segment has a size of 2^log_size
    T* segment;

public:
    CircularArray<T>(int log_size) : log_size(log_size) {
        segment = new T[1 << log_size];
    }
    ~CircularArray() {
        delete[] segment;// TODO: should we delete tasks as well
    }
    long size() {
        return 1 << log_size;
    }
    T get(long i) {
        return segment[i % size()];
    }
    void put(long i, T new_object) {
        segment[i % size()] = new_object;
    }
    CircularArray grow(long bottom_index, long top_index) {
        CircularArray a = CircularArray(log_size + 1);
        for (long i = top_index; i < bottom_index; i++) {
            a.put(i, get(i));
        }
        return a;
    }
};


template<typename T>
class CircularWSDeque {
public:
    static T Empty;
    static T Abort;
    static int LogInitialSize;

private:
    long bottom = 0;
    std::atomic<long> top = 0;
    CircularArray<T> activeArray = CircularArray<T>(LogInitialSize);

public:
    void pushBottom(T o) {
        long b = bottom;
        long t = top;
        CircularArray a = activeArray;
        long size = b - t;
        if (size >= a.size() - 1) {
            a = a.grow(b, t);
            activeArray = a;
        }
        a.put(b, o);
        bottom = b + 1;
    }

    T popBottom() {
        long b = bottom;
        CircularArray a = activeArray;
        b = b - 1;
        bottom = b;
        long t = top;
        long size = b - t;
        if (size < 0) {
            bottom = t;
            return Empty;
        }
        T o = a.get(b);
        if (size > 0)
            return o;
        if (!top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst))// TODO: strong or weak option ? memory order to specify or not ?
            o = Empty;
        bottom = t + 1;
        return o;
    }

    T steal() {
        long t = top;
        long b = bottom;
        CircularArray a = activeArray;
        long size = b - t;
        if (size <= 0) return Empty;
        T o = a.get(t);
        if (!top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst))// TODO: strong or weak option ? memory order to specify or not ?
            return Abort;
        return o;
    }
};

template<typename T>
int CircularWSDeque<T>::LogInitialSize = 5;// TODO: make it bigger or initialize that at the start depending on the size of the problem

// This is a hack to avoid sentinel values in a struct, that would add some additionnals byte to allocate
// This is using special static pointers that are returned to mean Empty or Abort, keeping the 8 bytes size of a pointer
// TODO: is this okay or should we change it ?
template<typename T>
T CircularWSDeque<T>::Empty = nullptr;
template<typename T>
T CircularWSDeque<T>::Abort = reinterpret_cast<T>(1);

#endif

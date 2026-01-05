// An implementation of the "Dynamic Circular Work-Stealing Deque" as presented in paper of the same name, from David Chase and Yossi Lev
// See article page: https://dl.acm.org/doi/10.1145/1073970.1073974
// This implements the basic algorithm presented in the paper without the further improvements.
// TODO: correct ?
//
// Adaptation
// 1. To avoid storing tasks directly but pointers to task like in the Java in the paper, we use a std::atomic<T*>* segment;
// 2. The casTop() method has been replaced by compare_exchange_strong
// 3. Added destructor to CircularWSDeque
//
#ifndef WSD
#define WSD

#include "util.hpp"
#include <atomic>

// First structure defined in Figure 4: Growable Circular Array
// It grows by doubling the capacity each time the size is reached
template<typename T>
class CircularArray {
private:
    int log_size;    // the segment has a size of 2^log_size
    long stored_size;// 2^log_size to avoid recalculation each time we call size
    std::atomic<T*>* segment;

public:
    explicit CircularArray(int log_size)
        : log_size(log_size),
          stored_size(1L << log_size),
          segment(new std::atomic<T*>[stored_size]) {
        // Initialize all slots to nullptr
        for (long i = 0; i < stored_size; ++i) {
            segment[i].store(nullptr, std::memory_order_relaxed);
        }
    }
    ~CircularArray() {
        // IMPORTANT:
        // - This destroys atomic<T*> objects
        // - It does NOT delete the T* values
        delete[] segment;
        segment = nullptr;
    }
    long size() {
        return stored_size;
    }
    T* get(long i) {
        return segment[i % stored_size].load(std::memory_order_acquire);
    }
    void put(long i, T* new_object) {
        segment[i % stored_size].store(new_object, std::memory_order_release);
    }
    CircularArray<T>* grow(long bottom_index, long top_index) {
        TRACE "GROWING from 2^" << log_size << "=" << stored_size << " to 2^" << (log_size + 1) << "=" << (1 << (log_size + 1));
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
    std::atomic<long> bottom = 0;
    std::atomic<long> top = 0;
    std::atomic<CircularArray<T>*> activeArray = new CircularArray<T>(LogInitialSize);

public:
    void pushBottom(T* new_object) {
        long b = bottom.load(std::memory_order_relaxed);
        long t = top.load(std::memory_order_acquire);
        CircularArray<T>* a = activeArray.load(std::memory_order_acquire);
        long size = b - t;
        if (size >= a->size() - 1) {
            a = a->grow(b, t);
            // NOTE: old CircularArray instances are intentionally leaked
            // until a safe way to reuse or free them is implemented
            activeArray.store(a, std::memory_order_release);
        }
        a->put(b, new_object);

        // This is a test to ensure all changes before are visible
        std::atomic_thread_fence(std::memory_order_seq_cst);
        bottom.store(b + 1, std::memory_order_release);
    }

    T* popBottom() {
        // NOTE: relaxed memory orders on bottom is fine because
        // only one thread is changing this value
        // and stealers will be updated when they load with acquire order but only when they need it
        long b = bottom.load(std::memory_order_relaxed);
        CircularArray<T>* a = activeArray.load(std::memory_order_acquire);
        b = b - 1;
        bottom.store(b, std::memory_order_relaxed);

        // This is a test to ensure all changes before are visible
        std::atomic_thread_fence(std::memory_order_seq_cst);

        long t = top.load(std::memory_order_acquire);
        long size = b - t;
        if (size < 0) {
            bottom.store(t, std::memory_order_relaxed);
            return Empty;
        }
        T* o = a->get(b);
        if (size > 0)
            return o;
        if (!top.compare_exchange_strong(t, t + 1, std::memory_order_acq_rel,
                                         std::memory_order_acquire))
            o = Empty;
        bottom.store(t + 1, std::memory_order_relaxed);
        return o;
    }

    T* steal() {
        long t = top.load(std::memory_order_acquire);
        long b = bottom.load(std::memory_order_acquire);

        CircularArray<T>* a = activeArray.load(std::memory_order_acquire);
        long size = b - t;
        if (size <= 0) return Empty;
        T* o = a->get(t);

        // This is a test to ensure all changes before are visible
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (!top.compare_exchange_strong(t, t + 1, std::memory_order_acq_rel,
                                         std::memory_order_acquire))
            return Abort;
        return o;
    }
};

template<typename T>
int CircularWSDeque<T>::LogInitialSize = 5;

// This is a hack to avoid sentinel values in a struct, that would add some additionnals byte to allocate
// This is using special static pointers that are returned to mean Empty or Abort, keeping the 8 bytes size of a pointer
static long a = 1;
template<typename T>
T* CircularWSDeque<T>::Empty = nullptr;
template<typename T>
T* CircularWSDeque<T>::Abort = reinterpret_cast<T*>(&a);

#endif

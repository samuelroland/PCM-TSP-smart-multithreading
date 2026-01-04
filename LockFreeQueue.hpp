/// --------------------------------------------------------
/// Queue lock-free pour Task*
// Given in course, chap5 in java, inspired from Michael & Scott
// modified to delete dummy in destructor to avoid read-after-free or memory leak
/// --------------------------------------------------------
#include "util.hpp"
#include <atomic>
#include <iostream>
template<typename T>
class LockFreeQueue {
private:
    struct Node {
        T* value;
        std::atomic<Node*> next;
        Node(T* t) : value(t), next(nullptr) {}
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

    // for memory claim
    std::atomic<Node*> retired_head{nullptr};

public:
    LockFreeQueue() {
        Node* dummy = new Node(nullptr);// dummy node
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

        // free retired node
        Node* n = retired_head.exchange(nullptr);
        while (n) {
            Node* next = n->next.load();
            delete n;
            n = next;
        }
    }

    void enqueue(T* t) {
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

    bool dequeue(T*& result) {
        while (true) {
            Node* first = head.load(std::memory_order_acquire);
            Node* last = tail.load(std::memory_order_acquire);
            Node* next = first->next.load(std::memory_order_acquire);

            if (first == head.load(std::memory_order_acquire)) {
                if (first == last) {
                    if (next == nullptr)
                        return false;// queue vide
                    tail.compare_exchange_strong(last, next,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed);
                } else {
                    result = next->value;
                    if (head.compare_exchange_strong(first, next,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed)) {

                        // Store removed node in another list to free it later
                        Node* old;
                        do {
                            old = retired_head.load(std::memory_order_acquire);
                            first->next.store(old, std::memory_order_release);

                            if (retired_head.compare_exchange_strong(
                                        old, first,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) break;
                            DEBUG "again " << first;
                        } while (true);

                        return true;
                        //delete first; // do not delete dummy here to avoid use-after-free
                    }
                }
            }
        }
    }
};

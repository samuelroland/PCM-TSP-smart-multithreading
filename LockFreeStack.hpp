//
// Created by fail on 29.11.25.
//
#pragma once
#include "atomic.hpp"
#include <atomic>

template<typename T>
class LockFreeStack {
private:
    struct Node {
        T* data;
        Node* next;
        Node(T* d) : data(d), next(nullptr) {}
    };

    // sommet de la pile avec ABA-safe
    atomic_stamped<Node> _head;

public:
    LockFreeStack() : _head(nullptr, 0) {}

    // push : ajoute un élément en haut de la pile
    void push(T* item) {
        Node* n = new Node(item);
        uint64_t stamp;
        Node* old_head;

        do {
            old_head = _head.get(stamp);
            n->next = old_head;
            // CAS : si le sommet n'a pas changé, on met à jour
        } while (!_head.cas(old_head, n, stamp, stamp + 1));
    }

    // pop : retire l'élément du sommet
    T* pop() {
        uint64_t stamp;
        Node* old_head;

        do {
            old_head = _head.get(stamp);
            if (!old_head)
                return nullptr;// pile vide
            // CAS : on remplace le sommet par l'élément suivant
        } while (!_head.cas(old_head, old_head->next, stamp, stamp + 1));

        T* ret = old_head->data;
        // TODO: lu par un autre thread? peut etre pas thread-safe
        delete old_head;
        return ret;
    }

    bool empty() {
        uint64_t stamp;
        return _head.get(stamp) == nullptr;
    }
};
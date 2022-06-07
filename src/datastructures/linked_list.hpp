#pragma once


#include "db/mempools.hpp"

#include <iostream>


template <typename T>
struct LinkedList {
    struct Node {
        Node* next;
        T val;

        bool operator<(const Node& other) {
            return val < other.val;
        }
    };


    static inline FixedThreadsafeMempool<Node> pool{4096};
    // FixedMempool<Node> pool{80};
    Node* head = nullptr;

    ~LinkedList() {
        if (!empty()) {
            int nodes = 0;
            remove_until([&](const auto&) {
                ++nodes;
                return true;
            });
            std::cerr << "linked list still filled with " << nodes << " nodes!\n";
        }
    }

    void add_sorted(T&& val) {
        Node* node = pool.allocate();
        node->val = val;

        Node** pp = &head;
        while (*pp && **pp < *node) {
            pp = &(*pp)->next;
        }
        node->next = *pp;
        *pp = node;
    }

    template <typename Fn>
    void remove_if(Fn&& fn) {
        Node** pp = &head;
        while (*pp) {
            Node* node = *pp;
            if (fn(node->val)) {
                *pp = node->next;
                pool.deallocate(node);
            } else {
                pp = &(node->next);
            }
        }
    }


    template <typename Fn>
    void remove_if_one(Fn&& fn) {
        Node** pp = &head;
        while (*pp) {
            Node* node = *pp;
            if (fn(node->val)) {
                *pp = node->next;
                pool.deallocate(node);
                break;
            } else {
                pp = &(node->next);
            }
        }
    }


    template <typename Fn>
    void remove_until(Fn&& fn) {
        Node** pp = &head;
        while (*pp) {
            Node* node = *pp;
            bool rm = fn(node->val);
            if (!rm) {
                break;
            }
            *pp = node->next;
            pool.deallocate(node);
        }
    }

    bool empty() {
        return head == nullptr;
    }
};

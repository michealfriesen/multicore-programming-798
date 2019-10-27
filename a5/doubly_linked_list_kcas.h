#pragma once

#include <cassert>
#include "kcas.h"

class DoublyLinkedList {
private:
    volatile char padding0[PADDING_BYTES];
    const int numThreads;
    const int minKey;
    const int maxKey;
    volatile char padding1[PADDING_BYTES];

    class Node {
        volatile char padding0[PADDING_BYTES];
        Node * prev; 
        Node * next;
        casword_t data;
    };

public:
    DoublyLinkedList(const int _numThreads, const int _minKey, const int _maxKey);
    ~DoublyLinkedList();
    
    bool contains(const int tid, const int & key);
    bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
    bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise
    
    long getSumOfKeys(); // should return the sum of all keys in the set
    void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function
};

DoublyLinkedList::DoublyLinkedList(const int _numThreads, const int _minKey, const int _maxKey)
        : numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey) {
    // it may be useful to know about / use the "placement new" operator (google)
    // because the simple_record_manager::allocate does not take constructor arguments
    // ... placement new essentially lets you call a constructor after an object already exists.
    
    // TODO:
    // Create the max and min nodes
    // Connect them
}

DoublyLinkedList::~DoublyLinkedList() {
    // TODO:
    // Delete the nodes by starting at the left, and deleting the data as we go 
}

bool DoublyLinkedList::contains(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
    return false;

    // TODO:
    // Loop from the left of the list until we find the value. 
}

bool DoublyLinkedList::insertIfAbsent(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
    return false;

    // TODO:
    // Loop from left of the list until we find a value >= key
    // If we found the key return false (its in the list)
    // If it is not the key, and it is greater than the key
    // start the KCAS.
    // TODO: Understand when to mark the bits.
}

bool DoublyLinkedList::erase(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
    return false;

    // TODO:
    // Loop through the array until we find the value
    // Once we find the value, start the KCAS
    // Maybe this is where we mark the bit?
}

long DoublyLinkedList::getSumOfKeys() {
    return -1;
    // Start from left to right, reading the value
    // after minkey, adding the value's data.
}

void DoublyLinkedList::printDebuggingDetails() {
    
}

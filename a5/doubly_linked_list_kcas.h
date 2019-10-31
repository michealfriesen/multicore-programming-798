#pragma once

#include <cassert>
#include "kcas.h"

class DoublyLinkedList {
private:
    const int numThreads;
    const int minKey;
    const int maxKey;

    KCASLockFree<5> kcas;

    struct Node {
        KCASLockFree<1> initKcas;
        casword_t prev; 
        casword_t next;
        int data;
        casword_t mark;

        Node(int tid, Node * p, Node * n, int d) {
            initKcas.writeInitPtr(tid, &next, (casword_t) n);
            initKcas.writeInitPtr(tid, &prev, (casword_t) p);
            data = d;
            initKcas.writeInitVal(tid, &mark, (casword_t) false);
        }

        // Constructor for an empty node that will be connected
        // at a later point (like the head and tail node)
        Node(int tid, int d) {
            initKcas.writeInitPtr(tid, &next, (casword_t) NULL);
            initKcas.writeInitPtr(tid, &prev, (casword_t) NULL);
            data = d;
            initKcas.writeInitVal(tid, &mark, (casword_t) false);
        }
    };

    Node * head;
    Node * tail;

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
    : numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey){
    
    head = new Node(0, minKey);
    tail = new Node(0, maxKey + 1);
    
    // Effectively head->next = tail;
    auto descPtrHead = kcas.getDescriptor(0);
    descPtrHead->addPtrAddr(&head->next, (casword_t) NULL, (casword_t) tail);
    if(!kcas.execute(0, descPtrHead)) {
        assert(false); // Something went wrong!
    }

    // Effectively tail->prev = head;
    auto descPtrTail = kcas.getDescriptor(0);
    descPtrTail->addPtrAddr(&tail->prev, (casword_t) NULL, (casword_t) head);
    if(!kcas.execute(0, descPtrTail)) {
        assert(false); // Something went wrong!
    }
}

DoublyLinkedList::~DoublyLinkedList() {
    delete head;
    delete tail;
}

bool DoublyLinkedList::contains(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);

    // Loop from the left of the list until we find the value. 
    Node * currentNode = head;
    while (key >= currentNode->data && (key < tail->data)) {
        if (key == currentNode->data) {
            return true;
        }
        currentNode = (Node *) kcas.readPtr(tid, &currentNode->next);
    };
    return false;
}

bool DoublyLinkedList::insertIfAbsent(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);

    // Loop from the left of the list until we find the value.
    Node * currentNode = head;
    while (key >= currentNode->data && (key < tail->data)) {
        if (key == currentNode->data) {
            return false; // key already exists
        }
        currentNode = (Node *) kcas.readPtr(tid, &currentNode->next); // Cant be tail as we have tail for that!
    };

    Node * prevNode = (Node *) kcas.readPtr(tid, &currentNode->prev);
    Node * newNode = new Node(tid, prevNode, currentNode, key);
    
    auto descPtr = kcas.getDescriptor(tid);
    descPtr->addValAddr(&prevNode->mark, false, false);
    descPtr->addValAddr(&currentNode->mark, false, false);
    descPtr->addPtrAddr(&prevNode->next, (casword_t) currentNode,(casword_t) newNode);
    descPtr->addPtrAddr(&currentNode->prev, (casword_t) prevNode, (casword_t) newNode);

    if (kcas.execute(tid, descPtr)) {
        return true;
    }
    else {
        delete newNode;
        insertIfAbsent(tid, key);
    }

    return false;
}

bool DoublyLinkedList::erase(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);

    // Loop from the left of the list until we find the value. 
    Node * currentNode = head;
    while (key >= currentNode->data && (key < tail->data)) {
        if (key == currentNode->data) {
            // This where we do the delete!
            Node * prevNode = (Node *) kcas.readPtr(tid, &currentNode->prev);
            Node * nextNode = (Node *) kcas.readPtr(tid, &currentNode->next);
            
            auto descPtr = kcas.getDescriptor(tid);
            descPtr->addValAddr(&prevNode->mark, false, false);
            descPtr->addValAddr(&currentNode->mark, false, true); // Marked so nothing can happen with inserts!
            descPtr->addPtrAddr(&nextNode->mark, false, false);;
            descPtr->addPtrAddr(&prevNode->next, (casword_t) currentNode, (casword_t) nextNode);
            descPtr->addPtrAddr(&nextNode->prev, (casword_t) currentNode, (casword_t) prevNode);

            if (kcas.execute(tid, descPtr)) {
                return true;
            }
            return false;
        }
        currentNode = (Node *) kcas.readPtr(tid, &currentNode->next);
    };
    return false;
}

long DoublyLinkedList::getSumOfKeys() {
    // Loop from the left of the list, adding all values. 
    Node * currentNode = head;
    long sum = 0;
    while(currentNode->data < tail->data) {
        sum += currentNode->data;
        currentNode = (Node *) kcas.readPtr(0, &currentNode->next);
    };
    return sum;
}

void DoublyLinkedList::printDebuggingDetails() {
    
}

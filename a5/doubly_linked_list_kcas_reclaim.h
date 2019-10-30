#pragma once

#include <cassert>
#include "recordmgr/record_manager.h"
#include "kcas.h"

class DoublyLinkedListReclaim {
private:
    volatile char padding0[PADDING_BYTES];
    const int numThreads;
    const int minKey;
    const int maxKey;
    volatile char padding1[PADDING_BYTES];

    KCASLockFree<5> kcas;
    
    struct Node {
        KCASLockFree<1> initKcas;
        volatile char padding0[PADDING_BYTES - (sizeof(casword_t) * 4)];
        casword_t prev; 
        casword_t next;
        casword_t data;
        casword_t mark;

        void setup(int tid, casword_t d) {
            initKcas.writeInitPtr(tid, &next, (casword_t) NULL);
            initKcas.writeInitPtr(tid, &prev, (casword_t) NULL);
            initKcas.writeInitVal(tid, &data, d);
            initKcas.writeInitVal(tid, &mark, (casword_t) false);
        }

        void setup(int tid, Node * p, Node * n, casword_t d) {
            initKcas.writeInitPtr(tid, &next, (casword_t) n);
            initKcas.writeInitPtr(tid, &prev, (casword_t) p);
            initKcas.writeInitVal(tid, &data, d);
            initKcas.writeInitVal(tid, &mark, (casword_t) false);
        }
    };
    simple_record_manager<Node> * recmgr;

    Node * head;
    Node * tail;

public:
    DoublyLinkedListReclaim(const int _numThreads, const int _minKey, const int _maxKey);
    ~DoublyLinkedListReclaim();
    
    bool contains(const int tid, const int & key);
    bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
    bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise
    
    long getSumOfKeys(); // should return the sum of all keys in the set
    void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function
    
};

DoublyLinkedListReclaim::DoublyLinkedListReclaim(const int _numThreads, const int _minKey, const int _maxKey)
    : numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey), recmgr(new simple_record_manager<Node>(MAX_THREADS)){

    recmgr->getGuard(0);

    head = recmgr->allocate<Node>(0);
    head->setup(0, minKey);
        
    tail = recmgr->allocate<Node>(0);
    tail->setup(0, maxKey);

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

    // cout << kcas.readVal(0, &head->data) << endl;
    // cout << kcas.readVal(0, &tail->data) << endl;
}

DoublyLinkedListReclaim::~DoublyLinkedListReclaim() {
    Node * currentNode = head;
    Node * nextNode = (Node *) kcas.readPtr(0, &currentNode->next);
    while(nextNode != NULL) {
        currentNode = nextNode;
        nextNode = (Node *) kcas.readPtr(0, &currentNode->next);
        Node * deletingNode = (Node *) kcas.readPtr(0, &currentNode->prev);
        recmgr->deallocate(0, deletingNode);
    }
    recmgr->deallocate(0, tail);
    delete recmgr;
}

bool DoublyLinkedListReclaim::contains(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);

    recmgr->getGuard(tid);

    // Loop from the left of the list until we find the value. 
    Node * currentNode = head;
    int indexValue = (int) kcas.readVal(tid, &currentNode->data);
    while (key >= indexValue) {
        if (key == indexValue) {
            return true;
        }
        if ((bool) kcas.readVal(tid, &currentNode->next) != NULL) {
            currentNode = (Node *) kcas.readPtr(tid, &currentNode->next);
            indexValue = kcas.readVal(tid, &currentNode->data);
        }
        else {
            return false;
        }
    };
    return false;
}

bool DoublyLinkedListReclaim::insertIfAbsent(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);

    recmgr->getGuard(tid);
    // Loop from the left of the list until we find the value. 
    Node * currentNode = head;
    int indexValue = (int) kcas.readVal(tid, &currentNode->data);
    while (key >= indexValue) {
        if (key == indexValue) {
            return false; // key already exists
        }
        if ((bool) kcas.readVal(tid, &currentNode->next) != NULL) {
            currentNode = (Node *) kcas.readPtr(tid, &currentNode->next);
            indexValue = kcas.readVal(tid, &currentNode->data);
        }
        // There are no keys in the list that are as big as key (assert false as this should never happen.)
        else {
            assert(false);
        }
    };

    Node * prevNode = (Node *) kcas.readPtr(tid, &currentNode->prev);
    Node * newNode = recmgr->allocate<Node>(tid); 
    newNode->setup(tid, prevNode, currentNode, (casword_t) key);
    
    auto descPtr = kcas.getDescriptor(tid);
    descPtr->addValAddr(&prevNode->mark, false, false);
    descPtr->addValAddr(&currentNode->mark, false, false);
    descPtr->addPtrAddr(&prevNode->next, (casword_t) currentNode,(casword_t) newNode);
    descPtr->addPtrAddr(&currentNode->prev, (casword_t) prevNode, (casword_t) newNode);

    if (kcas.execute(tid, descPtr)) {
        return true;
    }
    else {
        recmgr->deallocate(tid, newNode);
    }

    return false;
}

bool DoublyLinkedListReclaim::erase(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);

    recmgr->getGuard(tid);
    // Loop from the left of the list until we find the value. 
    Node * currentNode = head;
    int indexValue = (int) kcas.readVal(tid, &currentNode->data);
    while (key >= indexValue && (key < maxKey)) {
        if (key == indexValue) {
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
                // Successfully deleted.
                recmgr->retire(tid, currentNode);
                return true;
            }
            return false;
        }
        else if ((bool) kcas.readVal(tid, &currentNode->next) != NULL) {
            currentNode = (Node *) kcas.readPtr(tid, &currentNode->next);
            indexValue = kcas.readVal(tid, &currentNode->data);
        }
        // There are no keys in the list that are as big as key (assert false as this should never happen.)
        else {
            assert(false);
        }
    };
    return false;
}

long DoublyLinkedListReclaim::getSumOfKeys() {
    recmgr->getGuard(0);
    // Loop from the left of the list, adding all values. 
    Node * currentNode = head;
    long sum = 0;
    while(kcas.readVal(0, &currentNode->data) < maxKey) {
        sum += (long) kcas.readVal(0, &currentNode->data);
        currentNode = (Node *) kcas.readPtr(0, &currentNode->next);
    };
    return sum;
}

void DoublyLinkedListReclaim::printDebuggingDetails() {
    
}


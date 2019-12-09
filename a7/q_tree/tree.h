#pragma once

#include <immintrin.h>
#include <cassert>
#include "util.h"
//Use this for your maximum number of retires for your fast path 
#define MAX_RETRIES 40 

class ExternalBST {
private:
    struct Node {
        int key;
        Node * left;
        Node * right;

        bool isLeaf() {
            bool result = (left == NULL);
            assert(!result || right == NULL);
            return result;
        }
    };
    
    // this is a local struct that is only created/accessed by a thread on its own stack
    // should be optimized out by the compiler
    struct SearchRecord {
        Node * gp;
        Node * p;
        Node * n;
        SearchRecord(Node * _gp, Node * _p, Node * _n) : gp(_gp), p(_p), n(_n) {}
    };
    
    volatile char padding0[PADDING_BYTES];
    const int numThreads;
    const int minKey;
    const int maxKey;
    volatile char padding1[PADDING_BYTES];
    Node * root;
    volatile char padding2[PADDING_BYTES];

    // suggestion: place your lock to be used for TLE here (see util.h for a lock implementation)
    // be sure to add padding as appropriate! you REALLY want to ensure there's NO false sharing on your lock.
    
    volatile char padding3[PADDING_BYTES];
 
public:
    ExternalBST(const int _numThreads, const int _minKey, const int _maxKey);
    ~ExternalBST();

    // these functions must be implemented
    bool contains(const int tid, const int & key);
    bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
    bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise

    // no need to worry about these functions
    long getSumOfKeys(); // should return the sum of all keys in the set
    void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function
    
private:
    // these are given to you
    bool sequentialContains(const int tid, const int & key);
    bool sequentialInsertIfAbsent(const int tid, const int & key);
    bool sequentialErase(const int tid, const int & key);
    
    SearchRecord search(const int tid, const int & key);
    Node * createInternal(int key, Node * left, Node * right);
    Node * createLeaf(int key);
    void freeSubtree(Node * node);
    long getSumOfKeysInSubtree(Node * node);
};

ExternalBST::ExternalBST(const int _numThreads, const int _minKey, const int _maxKey)
: numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey) {
    Node * rootLeft = createLeaf(minKey - 1);
    Node * rootRight = createLeaf(maxKey + 1);
    root = createInternal(minKey - 1, rootLeft, rootRight);
}
ExternalBST::~ExternalBST() {
    freeSubtree(root);
}

bool ExternalBST::contains(const int tid, const int & key) {
    return sequentialContains(tid, key);
}

bool ExternalBST::insertIfAbsent(const int tid, const int & key) {
    return sequentialInsertIfAbsent(tid, key);
}

bool ExternalBST::erase(const int tid, const int & key) {
    return sequentialErase(tid, key);
}

ExternalBST::SearchRecord ExternalBST::search(const int tid, const int & key) {
    Node * gp;
    Node * p = NULL;
    Node * n = root;
    while (!n->isLeaf()) {
        gp = p;
        p = n;
        n = key < n->key ? n->left : n->right;
    }
    return SearchRecord(gp, p, n);
}

bool ExternalBST::sequentialContains(const int tid, const int & key) {
    assert(key >= minKey && key <= maxKey);
    SearchRecord rec = search(tid, key);
    return (rec.n->key == key);
}

bool ExternalBST::sequentialInsertIfAbsent(const int tid, const int & key) {
    assert(key >= minKey && key <= maxKey);
    SearchRecord ret = search(tid, key);
    if (key == ret.n->key) return false;

    // create two new nodes
    Node * newLeaf = createLeaf(key);
    Node * newInternal;
    if (key < ret.n->key) {
        newInternal = createInternal(ret.n->key, newLeaf, ret.n);
    } else {
        newInternal = createInternal(key, ret.n, newLeaf);
    }
    
    // change child
    if (ret.p->left == ret.n) {
        ret.p->left = newInternal;
    } else {
        ret.p->right = newInternal;
    }
    return true;
}

bool ExternalBST::sequentialErase(const int tid, const int & key) {
    assert(key >= minKey && key <= maxKey);
    SearchRecord ret = search(tid, key);
    if (key != ret.n->key) return false;
    
    // change appropriate child pointer of gp from p to n's sibling
    Node * sibling = (ret.p->left == ret.n) ? ret.p->right : ret.p->left;
    if (ret.gp->left == ret.p) {
        ret.gp->left = sibling;
    } else {
        ret.gp->right = sibling;
    }
    
    delete ret.p;
    delete ret.n;
    return true;
}

ExternalBST::Node * ExternalBST::createInternal(int key, Node * left, Node * right) {
    Node * node = new Node();
    node->key = key;
    node->left = left;
    node->right = right;
    return node;
}
ExternalBST::Node * ExternalBST::createLeaf(int key) {
    return createInternal(key, NULL, NULL);
}
void ExternalBST::freeSubtree(Node * node) {
    if (node == NULL) return;
    freeSubtree(node->left);
    freeSubtree(node->right);
    delete node;
}

long ExternalBST::getSumOfKeysInSubtree(Node * node) {
    if (node == NULL) return 0;
    // only leaves contain real keys
    if (node->isLeaf()) {
        // and we must ignore dummy sentinel keys that are not in [minKey, maxKey]
        if (node->key >= minKey && node->key <= maxKey) {
            //std::cout<<"counting key "<<node->key;
            return node->key;
        } else {
            return 0;
        }
    } else {
        return getSumOfKeysInSubtree(node->left)
             + getSumOfKeysInSubtree(node->right);
    }
}
long ExternalBST::getSumOfKeys() {
    return getSumOfKeysInSubtree(root);
}
void ExternalBST::printDebuggingDetails() {
}


#pragma once

#include <cassert>

class ExternalSequential {
private:
    struct Node {
        int key;
        Node * left;
        Node * right;
        bool marked;

        
        bool isLeaf() {
            bool result = (left == NULL);
            assert(!result || right == NULL);
            return result;
        }
        bool isParentOf(Node * other) {
            return (left == other || right == other);
        }
    };
    
    // this is a local struct that is only created/accessed by a thread on its own stack
    // should be optimized out by the compiler
    struct SearchRecord {
        Node * gp;
        Node * p;
        Node * n;
        
        SearchRecord(Node * _gp, Node * _p, Node * _n)
        : gp(_gp), p(_p), n(_n) {
        }
    };
    
    volatile char padding0[PADDING_BYTES];
    const int numThreads;
    const int minKey;
    const int maxKey;
    volatile char padding1[PADDING_BYTES];
    Node * root;
    volatile char padding2[PADDING_BYTES];
 
public:
    ExternalSequential(const int _numThreads, const int _minKey, const int _maxKey);
    ~ExternalSequential();
    long compareTo(int a, int b);
    bool contains(const int tid, const int & key);
    bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
    bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise
    long getSumOfKeys(); // should return the sum of all keys in the set
    void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function
private:
    auto search(const int tid, const int & key);
    auto createInternal(int key, Node * left, Node * right);
    auto createLeaf(int key);
    void freeSubtree(const int tid, Node * node);
    long getSumOfKeysInSubtree(Node * node);

};

auto ExternalSequential::createInternal(int key, Node * left, Node * right) {
    Node * node = new Node();
    node->key = key;
    node->left = left;
    node->right = right;
    node->marked = false;
    return node;
}

auto ExternalSequential::createLeaf(int key) {
    return createInternal(key, NULL, NULL);
}

ExternalSequential::ExternalSequential(const int _numThreads, const int _minKey, const int _maxKey)
: numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey) {
    auto rootLeft = createLeaf(minKey - 1);
    auto rootRight = createLeaf(maxKey + 1);
    root = createInternal(minKey - 1, rootLeft, rootRight);
}

ExternalSequential::~ExternalSequential() {
    freeSubtree(0 /* dummy thread id */, root);
}

inline long ExternalSequential::compareTo(int a, int b) {
    return ((long)a - (long)b); // casts to guarantee correct behaviour with large negative numbers
}

inline auto ExternalSequential::search(const int tid, const int & key) {
    Node * gp;
    Node * p = NULL;
    Node * n = root;
    while (!n->isLeaf()) {
        gp = p;
        p = n;
        n = (key <= n->key) ? n->left : n->right;
    }
    return SearchRecord(gp, p, n);
}

bool ExternalSequential::contains(const int tid, const int & key) {
    assert(key <= maxKey);
    auto rec = search(tid, key);
    return (rec.n->key == key);
}

bool ExternalSequential::insertIfAbsent(const int tid, const int & key) {
    assert(key <= maxKey);
    while (true) {
        auto ret = search(tid, key);
        auto dir = compareTo(key, ret.n->key);
        if (dir == 0) return false;
        // create two new nodes
        auto na = createLeaf(key);
        auto leftChild = (dir <= 0) ? na : ret.n;
        auto rightChild = (dir <= 0) ? ret.n : na;
        auto n1 = createInternal(std::min(key, ret.n->key), leftChild, rightChild);
        //atomic {
            if (!ret.p->marked && ret.p->isParentOf(ret.n)) {
                // change child
                if (ret.p->left == ret.n) {
                    ret.p->left = n1;
                    return true;
                } else {
                    assert(ret.p->right == ret.n);
                    ret.p->right = n1;
                    return true;
                }
            } else {
                // even in a concurrent setting, no other thread can have access to n1 or na here, so we can just delete/free
                delete n1;
                delete na;
            }
        //}
    }
}

bool ExternalSequential::erase(const int tid, const int & key) {
    assert(key <= maxKey);
    while (true) {
        auto ret = search(tid, key);
        auto dir = compareTo(key, ret.n->key);
        if (dir != 0) return false;
        //atomic {
            if (ret.gp->isParentOf(ret.p) && ret.p->isParentOf(ret.n) && !ret.gp->marked) {
                ret.n->marked = true;
                ret.p->marked = true;
                // change appropriate child pointer of gp from p to n's sibling
                auto sibling = (ret.p->left == ret.n) ? ret.p->right : ret.p->left;
                if (ret.gp->left == ret.p) {
                    ret.gp->left = sibling;
                } else {
                    assert(ret.gp->right == ret.p);
                    ret.gp->right = sibling;
                }
                // need safe memory reclamation here (in addition to a valid atomic block) if we have multiple threads
                delete ret.p;
                delete ret.n;

                return true;
            }
        //}
    }
    return false;
}

long ExternalSequential::getSumOfKeysInSubtree(Node * node) {
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
long ExternalSequential::getSumOfKeys() {
    return getSumOfKeysInSubtree(root);
}
void ExternalSequential::printDebuggingDetails() {
}

void ExternalSequential::freeSubtree(const int tid, Node * node) {
    if (node == NULL) return;
    freeSubtree(tid, node->left);
    freeSubtree(tid, node->right);
    delete node;
}

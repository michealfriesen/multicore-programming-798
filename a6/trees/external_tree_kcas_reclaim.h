#pragma once

#include <cassert>
#include "../recordmgr/record_manager.h"
#include "../kcas/kcas.h"

using namespace std;

class ExternalKCASReclaim {
private:
    struct Node {
        int key;
        casword<Node *> left;
        casword<Node *> right;
        casword<bool> marked;

        
        bool isLeaf() {
            bool result = (left == NULL);
            assert(!result || right == NULL);
            return result;
        }
        bool isParentOf(Node * other) {
            return (left == other || right == other);
        }

        DIRECTION whichParent(Node * other) {
            if (other == left) return LEFT;
            if (other == right) return RIGHT;
            return NEITHER;
        }
    };

    simple_record_manager<Node> * recmgr;
    
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
    ExternalKCASReclaim(const int _numThreads, const int _minKey, const int _maxKey);
    ~ExternalKCASReclaim();
    long compareTo(const int tid, int a, int b);
    bool contains(const int tid, const int & key);
    bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
    bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise
    long getSumOfKeys(); // should return the sum of all keys in the set
    void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function

private:
    auto search(const int tid, const int & key);
    auto createInternal(const int tid, int key, Node * left, Node * right);
    auto createLeaf(const int tid, int key);
    void freeSubtree(const int tid, Node * node);
    long getSumOfKeysInSubtree(Node * node);

};

auto ExternalKCASReclaim::createInternal(const int tid, int key, Node * left, Node * right) {
    Node * node = recmgr->allocate<Node>(tid);
    node->key = key;
    node->left.setInitVal(left);
    node->right.setInitVal(right);
    node->marked.setInitVal(false);
    return node;
}

auto ExternalKCASReclaim::createLeaf(const int tid, int key) {
    return createInternal(tid, key, NULL, NULL);
}

ExternalKCASReclaim::ExternalKCASReclaim(const int _numThreads, const int _minKey, const int _maxKey)
: numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey), recmgr(new simple_record_manager<Node>(MAX_THREADS)) {
    auto rootLeft = createLeaf(0, minKey - 1);
    auto rootRight = createLeaf(0, maxKey + 1);
    root = createInternal(0, minKey - 1, rootLeft, rootRight);
}

ExternalKCASReclaim::~ExternalKCASReclaim() {
    freeSubtree(0 /* dummy thread id */, root);
    delete recmgr;
}

inline long ExternalKCASReclaim::compareTo(const int tid, int a, int b) {
    return ((long)a - (long)b); // casts to guarantee correct behaviour with large negative numbers
}

inline auto ExternalKCASReclaim::search(const int tid, const int & key) {
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

bool ExternalKCASReclaim::contains(const int tid, const int & key) {
    auto rec = search(tid, key);
    return (rec.n->key == key);
}

bool ExternalKCASReclaim::insertIfAbsent(const int tid, const int & key) {
    recmgr->getGuard(tid);
    assert(key <= maxKey);
    while (true) {
        auto ret = search(tid, key);
        auto dir = compareTo(tid, key, ret.n->key);
        if (dir == 0) return false;
        // create two new nodes
        auto na = createLeaf(tid, key);
        auto leftChild = (dir <= 0) ? na : ret.n;
        auto rightChild = (dir <= 0) ? ret.n : na;
        auto n1 = createInternal(tid, std::min(key, (int) ret.n->key), leftChild, rightChild);

        kcas::start();
        kcas::add(&ret.p->marked, false, false);

        auto direction = ret.p->whichParent(ret.n);
        if (direction == LEFT) kcas::add(&ret.p->left, ret.n, n1);
        else if (direction == RIGHT) kcas::add(&ret.p->right, ret.n, n1);
        if (direction != NEITHER) {
            if (kcas::execute()) return true;
            else {
                recmgr->retire(tid, n1);
                recmgr->retire(tid, na);
            }
        }
        else {
            recmgr->retire(tid, n1);
            recmgr->retire(tid, na);
        }
    }
}

bool ExternalKCASReclaim::erase(const int tid, const int & key) {
    recmgr->getGuard(tid);
    assert(key <= maxKey);
    while (true) {
        auto ret = search(tid, key);
        auto dir = compareTo(tid, key, ret.n->key);
        if (dir != 0) return false;

        auto gpDir = ret.gp->whichParent(ret.p);
        auto pDir = ret.p->whichParent(ret.n);

        if (gpDir != NEITHER && pDir != NEITHER) {
            bool nMark = ret.n->marked;
	    	bool pMark = ret.p->marked;
			kcas::start();
            kcas::add(
                &ret.n->marked, nMark, true,
                &ret.p->marked, pMark, true,
				&ret.gp->marked, false, false
            );
	
	    Node * sib;
            if (gpDir == LEFT) {
                if (pDir == LEFT) {
		    		sib = ret.p->right;
                    kcas::add(
                        &ret.p->left, ret.n, ret.n,
                        &ret.gp->left, ret.p, sib,
						&ret.p->right, sib, sib
                    );
                }
                else if (pDir == RIGHT) {
		    		sib = ret.p->left;
                    kcas::add(
                        &ret.p->right, ret.n, ret.n,
                        &ret.gp->left, ret.p, sib,
						&ret.p->left, sib, sib
                    );
                } 
            }
            
            else if (gpDir == RIGHT) {
                if (pDir == LEFT) {
		    		sib = ret.p->right;
                    kcas::add(
                        &ret.p->left, ret.n, ret.n,
                        &ret.gp->right, ret.p, sib,
						&ret.p->right, sib, sib
                    );
                }
                else if (pDir == RIGHT) {
		    		sib = ret.p->left;
                    kcas::add(
                        &ret.p->right, ret.n, ret.n,
                        &ret.gp->right, ret.p, sib,
						&ret.p->left, sib, sib
                    );
                } 
            }
            if (kcas::execute()) {
                recmgr->retire(tid, ret.p);
                recmgr->retire(tid, ret.n);
                return true;
            }
	    else {
		// cout << "kcas failed." << endl;
	    } 
        }
    }
}


long ExternalKCASReclaim::getSumOfKeysInSubtree(Node * node) {
    recmgr->getGuard(0);
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
long ExternalKCASReclaim::getSumOfKeys() {
    return getSumOfKeysInSubtree(root);
}

void ExternalKCASReclaim::printDebuggingDetails() {
}

void ExternalKCASReclaim::freeSubtree(const int tid, Node * node) {
    recmgr->getGuard(tid);
    if (node == NULL) return;
    freeSubtree(tid, node->left);
    freeSubtree(tid, node->right);
    recmgr->deallocate(tid, node);
}



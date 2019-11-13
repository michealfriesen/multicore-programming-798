#pragma once

#include <cassert>

/***CHANGE THIS VALUE TO YOUR LARGEST KCAS SIZE****/
#define MAX_KCAS 1
/***CHANGE THIS VALUE TO YOUR LARGEST KCAS SIZE ****/

#include "../kcas/kcas.h"
#include "../recordmgr/record_manager.h"

using namespace std;
class ExternalKCASReclaim {
public:

private:
    volatile char padding0[PADDING_BYTES];
    const int numThreads;
    const int minKey;
    const int maxKey;
    volatile char padding1[PADDING_BYTES];


public:
    ExternalKCASReclaim(const int _numThreads, const int _minKey, const int _maxKey);
    ~ExternalKCASReclaim();

    bool contains(const int tid, const int & key);
    bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
    bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise

    long getSumOfKeys(); // should return the sum of all keys in the set
    void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function
};


ExternalKCASReclaim::ExternalKCASReclaim(const int _numThreads, const int _minKey, const int _maxKey)
: numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey) {
}

ExternalKCASReclaim::~ExternalKCASReclaim() {
    
}

bool ExternalKCASReclaim::contains(const int tid, const int & key) {
	return false;
}

bool ExternalKCASReclaim::insertIfAbsent(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
    return false;
}

bool ExternalKCASReclaim::erase(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
	return false;
}

long ExternalKCASReclaim::getSumOfKeys() {
}

void ExternalKCASReclaim::printDebuggingDetails() {

}



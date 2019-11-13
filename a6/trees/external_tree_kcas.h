#pragma once

#include <cassert>

/***CHANGE THIS VALUE TO YOUR LARGEST KCAS SIZE ****/
#define MAX_KCAS 1
/***CHANGE THIS VALUE TO YOUR LARGEST KCAS SIZE ****/

#include "../kcas/kcas.h"


using namespace std;
class ExternalKCAS {
private:    
	volatile char padding0[PADDING_BYTES];
	const int numThreads;
	const int minKey;
	const int maxKey;
	volatile char padding1[PADDING_BYTES];
public:
	ExternalKCAS(const int _numThreads, const int _minKey, const int _maxKey);
	~ExternalKCAS();
	bool contains(const int tid, const int & key);
	bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
	bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise
    
	long getSumOfKeys(); // should return the sum of all keys in the set
	void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function

};

ExternalKCAS::ExternalKCAS(const int _numThreads, const int _minKey, const int _maxKey)
        : numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey) {                  
}

ExternalKCAS::~ExternalKCAS() {
}

bool ExternalKCAS::contains(const int tid, const int & key) { 
	return false;
}

bool ExternalKCAS::insertIfAbsent(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
	return false;
}

bool ExternalKCAS::erase(const int tid, const int & key) {
	assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
	return false;
}


long ExternalKCAS::getSumOfKeys() {
	return -1;
}

void ExternalKCAS::printDebuggingDetails() {}

#pragma once
#include "util.h"
#include <atomic>
#include <mutex>
#include <iostream>
using namespace std;

struct paddedData {
    char padding2[PADDING_BYTES];
    atomic<uint64_t> d;
    mutex m;
    char padding3[PADDING_BYTES];
};

class AlgorithmA {
public:
    static constexpr int TOMBSTONE = -1;

    char padding0[PADDING_BYTES];
    const int numThreads;
    int capacity;
    char padding2[PADDING_BYTES];
    paddedData * data;

    AlgorithmA(const int _numThreads, const int _capacity);
    ~AlgorithmA();
    bool insertIfAbsent(const int tid, const int & key);
    bool erase(const int tid, const int & key);
    long getSumOfKeys();
    void printDebuggingDetails(); 
};

/**
 * constructor: initialize the hash table's internals
 * 
 * @param _numThreads maximum number of threads that will ever use the hash table (i.e., at least tid+1, where tid is the largest thread ID passed to any function of this class)
 * @param _capacity is the INITIAL size of the hash table (maximum number of elements it can contain WITHOUT expansion)
 */
AlgorithmA::AlgorithmA(const int _numThreads, const int _capacity)
: numThreads(_numThreads), capacity(_capacity) {
	data = new paddedData[capacity];
    for (int i = 0; i < _capacity; i++)
        data[i].d = NULL; // Initalize the data structure.
}

// destructor: clean up any allocated memory, etc.
AlgorithmA::~AlgorithmA() {
    delete data;
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmA::insertIfAbsent(const int tid, const int & key) {
    int32_t h = murmur3(key); // Generate hash that is indexed to our array.
    for (int i = 0; i < capacity; i++) {
        int index = (h + i) % capacity;
        data[index].m.lock(); // Locking to check if it is the correct value
        if(data[index].d == key) {
            data[index].m.unlock();
            return false;
        }
        if(data[index].d == NULL) { // Empty
            data[index].d = key;
            data[index].m.unlock();
            return true;
        }
        data[index].m.unlock();
    }
    return false; // Return false if there was no space, and the key wasn't found.
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmA::erase(const int tid, const int & key) {
    int32_t h = murmur3(key); // Generate hash that is indexed to our array.
    for (int i = 0; i < capacity; i++) {
        int index = (h + i) % capacity;
        data[index].m.lock(); // Locking to check if it is the correct value
        if(data[index].d == key) {
            data[index].d = TOMBSTONE;
            data[index].m.unlock();
            return true;
        }
        if(data[index].d == NULL) { // Empty
            data[index].m.unlock();
            return false;
        }
        data[index].m.unlock();
    }
    return false; // Return false if there was no space, and the key wasn't found.
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmA::getSumOfKeys() {
	// This is the naive way of adding all the values.
	int64_t sum = 0;
	for (int i = 0; i < capacity; i++) {
        data[i].m.lock();
        if(!(data[i].d == TOMBSTONE)) // Make sure the data is not deleted.
		    sum += data[i].d;
        data[i].m.unlock();
    }
	return sum;
}

// print any debugging details you want at the end of a trial in this function
void AlgorithmA::printDebuggingDetails() {
    // for (int i = 0; i < capacity; i++)
	// 	cout << data[i].d << endl;
}

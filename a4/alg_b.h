#pragma once
#include "util.h"
#include <atomic>
#include <mutex>
using namespace std;

class AlgorithmB {
public:
    static constexpr int TOMBSTONE = -1;

    char padding0[PADDING_BYTES];
    const int numThreads;
    int capacity;
    char padding2[PADDING_BYTES];
    paddedData * data;

    AlgorithmB(const int _numThreads, const int _capacity);
    ~AlgorithmB();
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
AlgorithmB::AlgorithmB(const int _numThreads, const int _capacity)
: numThreads(_numThreads), capacity(_capacity) {
    data = new paddedData[capacity];
    memset(data, 0, capacity * sizeof(paddedData));
}

// destructor: clean up any allocated memory, etc.
AlgorithmB::~AlgorithmB() {
    delete data;
} 

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmB::insertIfAbsent(const int tid, const int & key) {
    uint32_t h = murmur3(key); // Generate hash that is indexed to our array.
    for (uint32_t i = 0; i < capacity; i++) {
        uint32_t index = (h + i) % capacity;
        if(data[index].d == key) {
            return false; // No need to lock as there is no risk of overwriting
        }
        if(data[index].d == 0) { // Empty
            data[index].m.lock(); // Locking to ensure the key does not get overwritten before the change
            data[index].d = key;
            data[index].m.unlock();
            return true;
        }
    }
    return false; // Return false if there was no space, and the key wasn't found.
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmB::erase(const int tid, const int & key) {
    uint32_t h = murmur3(key); // Generate hash that is indexed to our array.
    for (uint32_t i = 0; i < capacity; i++) {
        uint32_t index = (h + i) % capacity;
        
        if(data[index].d == key) {
            data[index].m.lock(); // Locking to ensure no overwrites occur.
            data[index].d = TOMBSTONE;
            data[index].m.unlock();
            return true;
        }

        if(data[index].d == 0) {
            return false; // Not overwritting due to no issues with overwritting.
        }
    }
    return false; // Return false if there was no space, and the key wasn't found.
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmB::getSumOfKeys() {
	int64_t sum = 0;
	for (int i = 0; i < capacity; i++) {
        data[i].m.lock();
        if(data[i].d == TOMBSTONE){
        }
        else {
		    sum += data[i].d;
        }
        data[i].m.unlock();
    }
	return sum;
}

// print any debugging details you want at the end of a trial in this function
void AlgorithmB::printDebuggingDetails() {
    // int printAmount;
    // if (capacity < 500)
    //     printAmount = capacity;
    // else {
    //     printAmount = 500;
    // }
    // for (int i = 0; i < printAmount; i++) {
    //     if (data[i].d == TOMBSTONE)
    //         cout << "*T*";
        
    //     else 
    //         cout << data[i].d;
    // }
}

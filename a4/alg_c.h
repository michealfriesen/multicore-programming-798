#pragma once
#include "util.h"
#include <atomic>
using namespace std;

class AlgorithmC {
public:
    static constexpr int TOMBSTONE = -1;

    char padding0[PADDING_BYTES];
    const int numThreads;
    int capacity;
    char padding2[PADDING_BYTES];
    
    paddedData * data;

    AlgorithmC(const int _numThreads, const int _capacity);
    ~AlgorithmC();
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
AlgorithmC::AlgorithmC(const int _numThreads, const int _capacity)
: numThreads(_numThreads), capacity(_capacity) {
    data = new paddedData[capacity];
    for (int i = 0; i < _capacity; i++)
        data[i].d = 0; // Initalize the data structure.
}

// destructor: clean up any allocated memory, etc.
AlgorithmC::~AlgorithmC() {
    delete data;
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmC::insertIfAbsent(const int tid, const int & key) {
    uint32_t h = murmur3(key); // Generate hash that is indexed to our array.
    for (uint32_t i = 0; i < capacity; i++) {
        uint32_t index = (h + i) % capacity;
        uint32_t value = data[index].d;
        
        if(value == key) {
            return false; // No need to lock as there is no risk of overwriting
        }

        if(value == 0) { // Empty
            if(data[index].d.compare_exchange_strong(value, key)){
                return true;
            }
            else if (data[index].d == key) {
                return false;
            }
        }
    }
    return false; // Return false if there was no space, and the key wasn't found.
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmC::erase(const int tid, const int & key) {
    uint32_t h = murmur3(key); // Generate hash that is indexed to our array.
    for (uint32_t i = 0; i < capacity; i++) {
        uint32_t index = (h + i) % capacity;
        uint32_t value = data[index].d;
        
        if(data[index].d == 0) {
            return false;
        }
        else if(data[index].d == key) {
            // This is returning if the CAS was successful or not.
            return data[index].d.compare_exchange_strong(value, TOMBSTONE);
        }
    }
    return false; // Return false if there was no space, and the key wasn't found.
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmC::getSumOfKeys() {
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
void AlgorithmC::printDebuggingDetails() {
    int printAmount;
    if (capacity < 500)
        printAmount = capacity;
    else {
        printAmount = 500;
    }
    for (int i = 0; i < printAmount; i++) {
        if (data[i].d == TOMBSTONE)
            cout << "*T*";
        
        else 
            cout << data[i].d;
    }
}

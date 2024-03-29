#pragma once
#include "util.h"
#include <atomic>
#include <cmath>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
using namespace std;

class AlgorithmD {
private:
    enum {
        MARKED_MASK = (int) 0x80000000,     // most significant bit of a 32-bit key
        TOMBSTONE = (int) 0x7FFFFFFF,       // largest value that doesn't use bit MARKED_MASK
        EMPTY = (int) 0
    }; // with these definitions, the largest "real" key we allow in the table is 0x7FFFFFFE, and the smallest is 1 !!

    static const int EXPANSION_FACTOR = 2;
    static const int EXPANSION_SIZE = 2;
    static const int PROBING_AMOUNT = 100;


    struct table {
        // data types
        char padding2[PADDING_BYTES];
        atomic<uint32_t> * data;
        atomic<uint32_t> * old;
        counter * tombstoneCount;
        counter * approxSize;
        uint32_t capacity;
        uint32_t oldCapacity;
        // Adding addition padding to avoid a thread from spinning and cache missing a bunch, invalidating other's.
        char padding3[PADDING_BYTES];
        atomic<uint32_t> chunksClaimed;
        char padding4[PADDING_BYTES];
        atomic<uint32_t> chunksDone;
        char padding5[PADDING_BYTES];
        
        // constructor
        table(atomic<uint32_t> * _old, uint32_t _oldCapacity, int _numThreads) : 
        old(_old), oldCapacity(_oldCapacity), capacity(_oldCapacity * EXPANSION_SIZE), chunksClaimed(0), chunksDone(0) {
            data = new atomic<uint32_t>[capacity]();
            tombstoneCount = new counter(_numThreads);
            approxSize = new counter(_numThreads);
        }

        ~table() {
            delete[] data;
            delete tombstoneCount;
            delete approxSize;
        }
    };
    
    bool expandAsNeeded(const int tid, atomic<table *> t, int i);
    void helpExpansion(const int tid, table * t);
    void startExpansion(const int tid, atomic<table *> t);
    void migrate(const int tid, atomic<table *> t, int myChunk);
    
    char padding0[PADDING_BYTES];
    int initCapacity;
    // more fields (pad as appropriate)
    char padding2[PADDING_BYTES];
    // Padding below from currentTable
    atomic<table *> currentTable;
    
public:
    AlgorithmD(const int _numThreads, const int _capacity);
    ~AlgorithmD();
    bool insertIfAbsent(const int tid, const int & key, bool disableExpansion);
    bool erase(const int tid, const int & key);
    long getSumOfKeys();
    uint32_t getHash(const int& key, uint32_t capacity);
    void printDebuggingDetails();
    int numThreads; 
};

/**
 * constructor: initialize the hash table's internals
 * 
 * @param _numThreads maximum number of threads that will ever use the hash table (i.e., at least tid+1, where tid is the largest thread ID passed to any function of this class)
 * @param _capacity is the INITIAL size of the hash table (maximum number of elements it can contain WITHOUT expansion)
 */
AlgorithmD::AlgorithmD(const int _numThreads, const int _capacity)
: numThreads(_numThreads), initCapacity(_capacity) {
    currentTable = new table(0, _capacity, _numThreads);
    // Initialize the chunks claimed and chunks done to a state that resembles a normal state.
    currentTable.load()->chunksClaimed = ceil((float) _capacity / 4096);
    currentTable.load()->chunksDone = ceil((float) _capacity / 4096);
}

// destructor: clean up any allocated memory, etc.
AlgorithmD::~AlgorithmD() {
    delete currentTable;
}

// This will implicitly check if expanding is true by trying to help.
bool AlgorithmD::expandAsNeeded(const int tid, atomic<table *> t, int i) {
    // return false;
    
    helpExpansion(tid, t.load());
    // If it isn't, check the capacity, or how often we have probed.
    // If we see the approx size is larger than 1/2 of the size, expand.
    // If we see we are probing a large amount, get a more accurate check.
    // TODO: Play with these numbers to check how they impact performance.

    if (t.load()->approxSize->get() + t.load()->tombstoneCount->get() > (ceil((float)t.load()->capacity)/EXPANSION_FACTOR)) {
        startExpansion(tid, t.load());
        return true;
    }
    else if (i > PROBING_AMOUNT) {
        if (t.load()->approxSize->getAccurate() + t.load()->tombstoneCount->getAccurate() > ceil((float)t.load()->capacity/EXPANSION_FACTOR)) {
            startExpansion(tid, t.load());
            return true;
        }
    }
    return false;
}

void AlgorithmD::helpExpansion(const int tid, table * t) {
    
    uint32_t totalOldChunks = ceil((float) t->oldCapacity / 4096);
    // While there are chunks to claim,
    // Claim some chunks
    while (t->chunksClaimed < totalOldChunks) {
        int myChunk = t->chunksClaimed++;
        // This checks if this work is actually within the bounds of the old data.
        if (myChunk < totalOldChunks) {
            migrate(tid, t, myChunk);
            t->chunksDone++;
        }
    }
    
    while (t->chunksClaimed < totalOldChunks) {
        // Do nothing and just wait for the last thread to finish.
    }
}

void AlgorithmD::startExpansion(const int tid, atomic<table *> t) {
    table * passedTable = t.load(); 

    table * newTable = new table(t.load()->data, t.load()->capacity, numThreads);

    // Make a new table
    if (!(currentTable.compare_exchange_strong(passedTable, newTable))) {
        delete newTable;
    };

    helpExpansion(tid, currentTable);
}

void AlgorithmD::migrate(const int tid, atomic<table *> t, int myChunk) {

    int startingIndex = myChunk * 4096;
    int totalInserts = t.load()->oldCapacity - startingIndex;
    if (totalInserts > 4096) {
        totalInserts = 4096;
    }

    assert(startingIndex < t.load()->oldCapacity);
    assert(totalInserts <= 4096);

    for (int i = 0; i < totalInserts; i++) {

        uint32_t currKey = t.load()->old[i + startingIndex];
        while(!(t.load()->old[i + startingIndex].compare_exchange_strong(currKey, currKey | MARKED_MASK))) {
            currKey = t.load()->old[i + startingIndex];
        }
        int v = t.load()->old[i + startingIndex] & ~MARKED_MASK;
        // Do an insertIfAbsent with disableExpansion true to avoid recursion loop
        if ((v != TOMBSTONE) && (v != EMPTY)) {

            // Grab the old value
            // Do an insert in the new table with the value, disablingExpansion
            insertIfAbsent(tid, v, true);
        }
    }
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmD::insertIfAbsent(const int tid, const int & key, bool disableExpansion = false) {
    table * t = currentTable.load();
    uint32_t h = getHash(key, t->capacity); // Generate hash that is indexed to our array.
    for (uint32_t i = 0; i < t->capacity; ++i) {

        // Prevent the infinite loop for helping from occuring when migrating.
        if (disableExpansion) {
            uint32_t index = (h + i) % t->capacity;
            uint32_t value = t->data[index];
            
            assert(key != TOMBSTONE);
            assert(key > 0);

            // This means something went wrong during migration...
            if (value == key) {
            }

            else if (value == EMPTY) {
                if (t->data[index].compare_exchange_strong(value, key)){
                    t->approxSize->inc(tid);
                    return true;
                }
                else {
                }
            }
        }
        else {
            if (expandAsNeeded(tid, t, i)) return insertIfAbsent(tid, key, false);

            uint32_t index = (h + i) % t->capacity;
            uint32_t value = t->data[index];
            
            // Expansion happening
            if (value & MARKED_MASK) {
                return insertIfAbsent(tid, key, false);
            }

            // Key already found
            else if (value == key) {
                return false;
            }

            else if (value == EMPTY) {
                // Successful insert!
                if (t->data[index].compare_exchange_strong(value, key)){
                    t->approxSize->inc(tid); // incrementing as we added a value
                    return true;
                }
                else {
                    value = t->data[index];

                    // Expansion started, go help.
                    if (value & MARKED_MASK) {
                        assert(!disableExpansion);
                        return insertIfAbsent(tid, value |= MARKED_MASK, false);
                    }   
        
                    // Another thread inserted the key
                    else if (t->data[index] == key) {
                        return false;
                    }
                }            
            }
        }
    }
    return false; // Return false if there was no space, and the key wasn't found.
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmD::erase(const int tid, const int & key) {

    table * t = currentTable.load();

    // Generate hash that is indexed to our array.
    uint32_t h = getHash(key, t->capacity);
    for (uint32_t i = 0; i < t->capacity; i++) {
        if (expandAsNeeded(tid, t, i)) return erase(tid, key); 

        uint32_t index = (h + i) % t->capacity;
        uint32_t value = t->data[index];
        
        // Expansion happening
        if (value & MARKED_MASK) return erase(tid, key);
        
        if(t->data[index] == 0) {
            return false;
        }
        else if(t->data[index] == key) {
            // This is returning if the CAS was successful or not.
            if (t->data[index].compare_exchange_strong(value, TOMBSTONE))  {
                t->tombstoneCount->inc(tid);
                return true;
            }
            else {
                value = t->data[index];
                
                // Expansion started
                if (value & MARKED_MASK) return erase(tid, key);

                // Someone else deleted
                else if (t->data[index] == TOMBSTONE) {
                    return false;
                }
            }
        }
    }
    return false; // Return false if there was no space, and the key wasn't found.
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmD::getSumOfKeys() {
    table * t = currentTable.load();
    int64_t sum = 0;
	for (int i = 0; i < t->capacity; i++) {
        if(t->data[i] == TOMBSTONE){
        }
        else {
		    sum += t->data[i];
        }
    }
	return sum;
}

uint32_t AlgorithmD::getHash(const int& key, uint32_t capacity) {
    return floor(((double ) murmur3(key) / (double) UINT32_MAX) * capacity); 
}

// print any debugging details you want at the end of a trial in this function
void AlgorithmD::printDebuggingDetails() {

    // cout << "Final Capacity is: " << currentTable.load()->capacity << endl;
    // table * t = currentTable.load();
    // int printAmount;
    // if (t->capacity < 500)
    //     printAmount = t->capacity;
    // else {
    //     printAmount = 500;
    // }
    // for (int i = 0; i < printAmount; i++) {
    //     if (t->data[i] == TOMBSTONE)
    //         cout << "*T*";
        
    //     else if (t->data[i] == EMPTY)
    //         cout << "*EMPTY*";
        
    //     else 
    //         cout << t->data[i];
    // }
}

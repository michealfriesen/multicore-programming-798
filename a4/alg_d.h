#pragma once
#include "util.h"
#include <atomic>
#include <cmath>
#include <stdio.h>
#include <stdint.h>
using namespace std;

class AlgorithmD {
private:
    enum {
        MARKED_MASK = (int) 0x80000000,     // most significant bit of a 32-bit key
        TOMBSTONE = (int) 0x7FFFFFFF,       // largest value that doesn't use bit MARKED_MASK
        EMPTY = (int) 0
    }; // with these definitions, the largest "real" key we allow in the table is 0x7FFFFFFE, and the smallest is 1 !!

    struct table {
        // data types
        char padding2[64]; 
        paddedDataNoLock * data;
        paddedDataNoLock * old; 
        uint32_t capacity;
        uint32_t oldCapacity;
        counter * approxSize;
        atomic<uint32_t> chunksClaimed;
        atomic<uint32_t> chunksDone;
        char padding3[64];
        
        // constructor
        table(paddedDataNoLock * _old, uint32_t _oldCapacity) : old(_old), oldCapacity(_oldCapacity), capacity(_oldCapacity * 2), chunksClaimed(0), chunksDone(0) {
            data = new paddedDataNoLock[capacity];
            for (int i = 0; i < capacity; i++)
                data[i].d = EMPTY; // Initalize the data structure.
        }
        
        // destructor
        ~table() {
            // this check ensures expansion is not happening still.
            if(chunksDone == capacity)
                delete data;
        }
    };
    
    bool expandAsNeeded(const int tid, table * t, int i);
    void helpExpansion(const int tid, table * t);
    void startExpansion(const int tid, table * t);
    void migrate(const int tid, table * t, int myChunk);
    
    char padding0[PADDING_BYTES];
    int numThreads;
    int initCapacity;
    // more fields (pad as appropriate)
    char padding1[PADDING_BYTES];
    table * currentTable;
    
public:
    AlgorithmD(const int _numThreads, const int _capacity);
    ~AlgorithmD();
    bool insertIfAbsent(const int tid, const int & key, bool disableExpansion);
    bool erase(const int tid, const int & key);
    long getSumOfKeys();
    uint32_t getHash(const int& key, uint32_t capacity);
    void printDebuggingDetails(); 
};

/**
 * constructor: initialize the hash table's internals
 * 
 * @param _numThreads maximum number of threads that will ever use the hash table (i.e., at least tid+1, where tid is the largest thread ID passed to any function of this class)
 * @param _capacity is the INITIAL size of the hash table (maximum number of elements it can contain WITHOUT expansion)
 */
AlgorithmD::AlgorithmD(const int _numThreads, const int _capacity)
: numThreads(_numThreads), initCapacity(_capacity) {
    currentTable = new table(0, _capacity);
}

// destructor: clean up any allocated memory, etc.
AlgorithmD::~AlgorithmD() {

}

bool AlgorithmD::expandAsNeeded(const int tid, table * t, int i) {
    return false;
}

void AlgorithmD::helpExpansion(const int tid, table * t) {

}

void AlgorithmD::startExpansion(const int tid, table * t) {

}

void AlgorithmD::migrate(const int tid, table * t, int myChunk) {

}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmD::insertIfAbsent(const int tid, const int & key, bool disableExpansion = false) {

    table * t = currentTable;
    uint32_t h = getHash(key, t -> capacity); // Generate hash that is indexed to our array.
    for (uint32_t i = 0; i < t->capacity; i++) {

        // TODO: understand the importance of the disableExpansion
        if (expandAsNeeded(tid, t, i)) return insertIfAbsent(tid, key, false); 
        uint32_t index = (h + i) % t->capacity;
        uint32_t value = t->data[index].d;
        
        // Expansion happening
        if (value & MARKED_MASK) return insertIfAbsent(tid, key, false);

        // Key already found
        if (value == key) {
            return false;
        }

        if (value == EMPTY) {
            // Successful insert!
            if (t->data[index].d.compare_exchange_strong(value, key)){
                return true;
            }
            else {
                value = t->data[index].d;
                // Expansion started
                if (value & MARKED_MASK) return insertIfAbsent(tid, key, false);
                
                // Another thread inserted the key
                else if (t->data[index].d == key) {
                    return false;
                }
            }            
        }
    }
    return false; // Return false if there was no space, and the key wasn't found.
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmD::erase(const int tid, const int & key) {

    table * t = currentTable;
    // TODO: BEFORE ALL OF THIS, CHECK IF WE NEED TO EXPAND

    // Generate hash that is indexed to our array.
    uint32_t h = getHash(key, t -> capacity);
    for (uint32_t i = 0; i < t->capacity; i++) {
        if (expandAsNeeded(tid, t, i)) return erase(tid, key); 

        uint32_t index = (h + i) % t->capacity;
        uint32_t value = t->data[index].d;
        
        // Expansion happening
        if (value & MARKED_MASK) return erase(tid, key);
        
        if(t->data[index].d == 0) {
            return false;
        }
        else if(t->data[index].d == key) {
            // This is returning if the CAS was successful or not.
            cout << value << endl;
            if (t->data[index].d.compare_exchange_strong(value, TOMBSTONE)) return true;
            else {
                value = t->data[index].d;
                
                // Expansion started
                if (value & MARKED_MASK) return erase(tid, key);

                // Someone else deleted
                else if (t->data[index].d == TOMBSTONE) {
                    return false;
                }
            }
        }
    }
    return false; // Return false if there was no space, and the key wasn't found.
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmD::getSumOfKeys() {
    table * t = currentTable;
    int64_t sum = 0;
	for (int i = 0; i < t->capacity; i++) {
        if(t->data[i].d == TOMBSTONE){
        }
        else {
		    sum += t->data[i].d;
        }
    }
	return sum;
}

uint32_t AlgorithmD::getHash(const int& key, uint32_t capacity) {
    return floor(((double) murmur3(key) / (double) UINT32_MAX) * capacity); 
}

// print any debugging details you want at the end of a trial in this function
void AlgorithmD::printDebuggingDetails() {

    table * t = currentTable;
    int printAmount;
    if (t -> capacity < 500)
        printAmount = t -> capacity;
    else {
        printAmount = 500;
    }
    for (int i = 0; i < printAmount; i++) {
        if (t -> data[i].d == TOMBSTONE)
            cout << "*T*";
        
        else if (t -> data[i].d == EMPTY)
            cout << "*EMPTY*";
        
        else 
            cout << t -> data[i].d;
    }
}

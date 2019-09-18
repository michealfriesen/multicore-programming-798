#ifndef COUNTERS_IMPL_H
#define COUNTERS_IMPL_H

#include <atomic>
#include <mutex>
#include <thread>

#define MAX_THREADS 256

class CounterNaive {
private:
    char padding0[64];
    int64_t v;
    char padding1[64];
public:
    CounterNaive(int _numThreads) : v(0) {}
    int64_t inc(int tid) {
        return v++;
    }
    int64_t read() {
        return v;
    }
};

class CounterLocked {
private:
    std::mutex counterMutex;
    /** Adding padding to prevent false sharing in the test suite
        (Even though my threads are constrained by the mutex) */
    char padding0[64];
    int64_t counter;
    char padding1[64];

public:
    CounterLocked(int _numThreads) : counter(0) {}
    int64_t inc(int tid) {
        counterMutex.lock();
        int prevValue = counter++;
        counterMutex.unlock();
        return prevValue;
    }
    int64_t read() {
        counterMutex.lock();
        int currentValue = counter;
        counterMutex.unlock();
        return currentValue;
    }
};

class CounterFetchAndAdd {
private:
    char padding0[64];
    atomic<int> counter;
    char padding1[64];

public:
    CounterFetchAndAdd(int _numThreads) : counter(0) {}
    int64_t inc(int tid) {
        int prevValue = counter++;
        return prevValue;
    }
    int64_t read() {
        return counter;
    }
};




class CounterApproximate {
private:
    // list of counters equal to the max number of threads
    struct padded_counter {
        int64_t c; // each are private so threads shouldnt need to compete...
        char padding[64-sizeof(int64_t)];
    };
    padded_counter counterList[MAX_THREADS];

    std::mutex gCounterMutex;
    int64_t gCounter;
    char padding1[64 - sizeof(int64_t)];

    // flush threshold
    int flushThreshold; //No padding because it is read only

public:
    CounterApproximate(int _numThreads) : gCounter(0), flushThreshold(_numThreads * 100){
        // Initialize the counter array
        for (int threadId=0; threadId < _numThreads; ++threadId) {
            new (&counterList[threadId]) int64_t(0);
        }
    }
    int64_t inc(int tid) {
        // Increment the counter you are assigned to 
        if(++counterList[tid].c >= flushThreshold) {
            gCounter += counterList[tid].c;
            counterList[tid].c = 0;
        }
    }
    int64_t read() {
        return gCounter;
    }
};


class CounterShardedLocked {
private:
    // list of counters equal to the max number of threads
    struct padded_counter {
        int64_t c; // Not atomic because we are locking it every time anyways
        char padding[64 - sizeof(int64_t)];
        std::mutex counterMutex;
    };
    padded_counter counterList[MAX_THREADS];
    int numThreads; // Used for getting only the amount of active threads there are.

public:
    CounterShardedLocked(int _numThreads) : numThreads(_numThreads) {
        // Initialize the counter array
        for (int threadId=0; threadId < _numThreads; ++threadId) {
            new (&counterList[threadId]) int64_t (0);
        }
    }
    int64_t inc(int tid) {
        counterList[tid].counterMutex.lock();
        counterList[tid].c++;
        counterList[tid].counterMutex.unlock();
    }
    int64_t read() {
        int64_t currentVal = 0;
        for (int threadId = 0; threadId < numThreads; ++threadId) {
            currentVal += counterList[threadId].c;
        }
        return currentVal;
    }
};


class CounterShardedWaitfree {
private:
    // list of counters equal to the max number of threads
    struct padded_counter {
        atomic<int> c; // Atomic because we need to fetch and add during the for loop
        char padding[64 - sizeof(atomic<int64_t>)];
    };
    padded_counter counterList[MAX_THREADS];
    atomic<int64_t> counter;
    char padding1 [64 - sizeof(atomic<int64_t>)]
    int numThreads;

public:
    CounterShardedWaitfree(int _numThreads) : numThreads(_numThreads) {
    // Initialize the counter array
        for (int threadId=0; threadId < _numThreads; ++threadId) {
            new (&counterList[threadId]) int64_t(0);
        }
    }
    int64_t inc(int tid) {
        counterList[tid].c++;
    }
    int64_t read() {
        for (int threadId = 0; threadId < numThreads; ++threadId) {
            counter += counterList[threadId].c;
        
        return counter;
        }
    }
};

#endif


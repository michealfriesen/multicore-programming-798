#ifndef COUNTERS_IMPL_H
#define COUNTERS_IMPL_H

#include <atomic>
#include <mutex>
#include <thread>

#define MAX_THREADS 256

class CounterNaive {
private:
    char padding0[64];
    int v;
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
    int counter;
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
        atomic<int> c;
        char padding[64-sizeof(atomic<int>)];
    };
    padded_counter counterList[MAX_THREADS];
    
    // global counter that is padded
    char padding1[64];
    atomic<int> gCounter;
    char padding2[64];

    // flush threshold
    int flushThreshold; //No padding because it is read only

public:
    CounterApproximate(int _numThreads) : gCounter(0), flushThreshold(_numThreads * 10){
        // Initialize the counter array
        for (int threadId=0; threadId < _numThreads; ++threadId) {
            new (&counterList[threadId]) atomic<int>(0);
        }
    }
    int64_t inc(int tid) {
        // Increment the counter you are assigned to 
        counterList[tid].c++;

        // If the threshold has been reached, fetch and add then flush the local counter to the global counter
        if(counterList[tid].c >= flushThreshold) {
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
        atomic<int> c; // Not atomic because we are locking it every time anyways
        char padding[64-sizeof(atomic<int>)];
        std::mutex counterMutex;
    };
    padded_counter counterList[MAX_THREADS];
    int numThreads; // Used for getting only the amount of active threads there are.

public:
    CounterShardedLocked(int _numThreads) : numThreads(0) {
        // Initialize the counter array
        for (int threadId=0; threadId < _numThreads; ++threadId) {
            new (&counterList[threadId]) atomic<int>(0);
        }
    }
    int64_t inc(int tid) {
        counterList[tid].counterMutex.lock();
        counterList[tid].c++;
        counterList[tid].counterMutex.unlock();
    }
    int64_t read() {
        int currentVal = 0;
        for (int threadId = 0; threadId < numThreads; ++threadId) {
            counterList[threadId].counterMutex.lock();
            currentVal += counterList[threadId].c;
            counterList[threadId].counterMutex.unlock();
        }
        return currentVal;
    }
};


class CounterShardedWaitfree {
private:
    // list of counters equal to the max number of threads
    struct padded_counter {
        atomic<int> c; // Not atomic because we are locking it every time anyways
        char padding[64-sizeof(atomic<int>)];
    };
    padded_counter counterList[MAX_THREADS];
    int numThreads;

public:
    CounterShardedWaitfree(int _numThreads) : numThreads(0) {
    // Initialize the counter array
        for (int threadId=0; threadId < _numThreads; ++threadId) {
            new (&counterList[threadId]) atomic<int>(0);
        }
    }
    int64_t inc(int tid) {
        counterList[tid].c++;
    }
    int64_t read() {
        int currentVal = 0;
        for (int threadId = 0; threadId < numThreads; ++threadId) {
            currentVal += counterList[threadId].c;
        }
        return currentVal;
    }
};

#endif


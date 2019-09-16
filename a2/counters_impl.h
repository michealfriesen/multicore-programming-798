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
    int flushThreshold;
    char padding3[64];

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
public:
    CounterShardedLocked(int _numThreads) {
    }
    int64_t inc(int tid) {
    }
    int64_t read() {
    }
};


class CounterShardedWaitfree {
private:
public:
    CounterShardedWaitfree(int _numThreads) {
    }
    int64_t inc(int tid) {
    }
    int64_t read() {
    }
};

#endif


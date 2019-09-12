#ifndef COUNTERS_IMPL_H
#define COUNTERS_IMPL_H

#include <atomic>
#include <mutex>

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
    char padding2[64];
    int counter;
    char padding3[64];

public:
    CounterLocked(int _numThreads) {}
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
    std::mutex counterMutex;
    char padding4[64];
    atomic<int> counter;
    char padding5[64];

public:
    CounterFetchAndAdd(int _numThreads) {}
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


class CounterApproximate {
private:
public:
    CounterApproximate(int _numThreads) {
    }
    int64_t inc(int tid) {
    }
    int64_t read() {
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


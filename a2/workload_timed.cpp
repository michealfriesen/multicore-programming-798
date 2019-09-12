#include <cstdio>
#include <cstring>
#include <vector>
#include <thread>
#include <atomic>
using namespace std;

#include "util.h"
#include "counters_impl.h"

template <class CounterType>
struct globals_t {
    char padding0[64]; // padding avoid any possible false sharing... wasting a small number of cache lines like this is totally fine
    
    // barrier used to ensure all threads start modifying the counter at approximately the same time
    Barrier * barrier;
    
    char padding1[64];
    
    // timer used to determine when threads should stop
    ElapsedTimer * timer;
    
    char padding2[64];
    
    // variable used to track the number of increment operations performed by all threads
    // note: for efficiency, each thread counts its own operations, then does fetch&add on this variable ONCE
    atomic<int64_t> incrementsPerformed;
    
    char padding3[64];
    
    CounterType * counter;
    
    char padding4[64];
    
    int64_t numThreads;
    int64_t millisToRun;
    
    char padding5[64];

    globals_t(int64_t numThreads, int64_t millisToRun) {
        barrier = new Barrier(1+numThreads);
        timer = new ElapsedTimer();
        incrementsPerformed = 0;
        counter = new CounterType(numThreads);
        this->numThreads = numThreads;
        this->millisToRun = millisToRun;
    }
    ~globals_t() {
        // manually free memory for objects allocated with "new"
        delete barrier;
        delete timer;
        delete counter;
    }
};

template <class CounterType>
void threadFunc(int tid, globals_t<CounterType> * g) {
    // wait for all threads to start
    g->barrier->wait();
    printf("thread %d start (counter=%ld)\n", tid, g->counter->read());

    // do increments
    int64_t last = 0;
    int64_t i;
    for (i=0; ; ++i) {
        last = g->counter->inc(tid);

        // check timer to see if we should terminate
        // (to reduce overhead of timing calls, do this only once every X increments)
        if ((i & 1023) == 0) if (g->timer->getElapsedMillis() >= g->millisToRun) break;
    }
    g->incrementsPerformed.fetch_add(i+1);
    printf("thread %d end (last counter value seen %ld)\n", tid, last);
}

template <class CounterType>
void runExperiment(globals_t<CounterType> * g) {
    
    // start all threads
    vector<thread *> threads;
    for (int tid=0; tid < g->numThreads; ++tid) {
        threads.push_back(new thread(threadFunc<CounterType>, tid, g));
    }
    
    g->timer->start();
    g->barrier->wait();
    
    // wait for all threads to terminate (and explicitly free memory allocated with "new" for the thread)
    for (auto t : threads) {
        t->join();
        delete t;
    }
    
    printf("\n");
    printf("final counter value after %ld increments is %ld\n", g->incrementsPerformed.load(), g->counter->read());
    printf("increments/s: %ld\n", g->incrementsPerformed.load() * 1000 / g->millisToRun);
    printf("\n");
}

int main(int argc, char ** argv) {
    // parse command line args
    if (argc != 4) {
        printf("USAGE: %s NUM_THREADS MILLIS_TO_RUN COUNTER_TYPE_NAME\n", argv[0]);
        printf("       where COUNTER_TYPE_NAME in {naive, lock, faa, approx, shard_lock, shard_wf}\n");
        return 1;
    }
    const int numThreads = atoll(argv[1]);
    const int millisToRun = atoll(argv[2]);

    // create the counter that threads will access and invoke runExperiment
    // (providing counter type information via templates -- orders of magnitude faster than polymorphism)
    if (!strcmp(argv[3], "naive")) {
        runExperiment(new globals_t<CounterNaive>(numThreads, millisToRun));
    } else if (!strcmp(argv[3], "lock")) {
        runExperiment(new globals_t<CounterLocked>(numThreads, millisToRun));
    } else if (!strcmp(argv[3], "faa")) {
        runExperiment(new globals_t<CounterFetchAndAdd>(numThreads, millisToRun));
    } else if (!strcmp(argv[3], "approx")) {
        runExperiment(new globals_t<CounterApproximate>(numThreads, millisToRun));
    } else if (!strcmp(argv[3], "shard_lock")) {
        runExperiment(new globals_t<CounterShardedLocked>(numThreads, millisToRun));
    } else if (!strcmp(argv[3], "shard_wf")) {
        runExperiment(new globals_t<CounterShardedWaitfree>(numThreads, millisToRun));
    } else {
        printf("ERROR: unexpected algorithm name %s\n", argv[3]);
        return 1;
    }
    return 0;
}

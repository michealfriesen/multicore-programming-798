/**
 * A simple array-based benchmark for HTM
 *      (threads repeatedly pick a random slot i,
 *       then atomically increment k slots starting from i)
 */

#include <thread>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <ctime>
#include <vector>
#include <immintrin.h>

#include "binding.h"
using namespace std;

class RNG {
private:
    unsigned int seed; 
    char padding[64 - sizeof(seed)];
public:
    RNG(void) : seed(0) {}
    void setSeed(int seed) { this->seed = seed; }
    unsigned int next() { seed ^= seed << 6; seed ^= seed >> 21; seed ^= seed << 7; return seed; }
};

class AtomicArrayK {
public:
    volatile char padding0[PADDING_BYTES];
    int64_t * data;
    volatile char padding1[PADDING_BYTES];
    const int size;
    const int K;
    volatile char padding2[PADDING_BYTES];
    TryLock lock;
    volatile char padding3[PADDING_BYTES];

    AtomicArrayK(const int _size, const int _K) : size(_size), K(_K) {
        data = new int64_t[_size+32]; // pad both ends of array
        data = data+16; // shift data pointer forward to effectively pad the start of the array
        for (int i=0;i<_size;++i) {
            data[i] = 0;
        }
    }
    
    ~AtomicArrayK() {
        data = data-16; // shift data pointer back before freeing (so the allocator actually recognizes the pointer as something it returned from a malloc()/new call)
        delete[] data;
    }
    
    void incrementRandomK(RNG & rng) {
        int retries = 40;
    retry:
        if (_xbegin() == _XBEGIN_STARTED) {
            if (lock.isHeld()) _xabort(1);
        } else {
            while (lock.isHeld()) { __builtin_ia32_pause(); /* wait */ }
            if (--retries) goto retry;
            lock.acquire();
        }
        
        int start = rng.next() % size; // index of first element
        // we will increment k consecutive slots starting at this location
        
        // read all slots into vals[...]
        int ix = start;
        int64_t vals[K];
        for (int i=0;i<K;++i) {
            vals[i] = data[ix];
            ix = (ix+1) % size;
        }

        // write new values to all slots
        ix = start;
        for (int i=0;i<K;++i) {
            data[ix] = 1+vals[i];
            ix = (ix+1) % size;
        }
        
        if (lock.isHeld()) {
            assert(!_xtest());
            lock.release();
        } else {
            _xend();
        }
    }
    
    long long getTotal() {
        int64_t result = 0;
        for (int i=0;i<size;++i) {
            result += data[i];
        }
        return result;
    }
};

struct globals_t {
    char padding0[PADDING_BYTES];
    RNG rngs[MAX_THREADS];
    char padding1[PADDING_BYTES];
    ElapsedTimer timer;
    char padding2[PADDING_BYTES];
    long elapsedMillis;
    char padding3[PADDING_BYTES];
    volatile bool done;
    char padding4[PADDING_BYTES];
    volatile bool start;        // used for a custom barrier implementation (should threads start yet?)
    char padding5[PADDING_BYTES];
    atomic_int running;         // used for a custom barrier implementation (how many threads are waiting?)
    char padding6[PADDING_BYTES];
    AtomicArrayK * aArray;
    debugCounter numSuccessfulOps;    // already has padding built in at the beginning and end
    int millisToRun;
    int totalThreads;
    char padding7[PADDING_BYTES];
    
    globals_t(int _millisToRun, int _totalThreads, int K, int arraySize) {
        for (int i=0;i<MAX_THREADS;++i) {
            rngs[i].setSeed(i+1); // +1 because we don't want thread 0 to get a seed of 0, since seeds of 0 usually mean all random numbers are zero...
        }
        elapsedMillis = 0;
        done = false;
        start = false;
        running = 0;
        aArray = new AtomicArrayK(arraySize, K);
        millisToRun = _millisToRun;
        totalThreads = _totalThreads;
    }
    ~globals_t() {
        delete aArray;
    }
} __attribute__((aligned(PADDING_BYTES)));

void runExperiment(int arraySize, int millisToRun, int totalThreads, int K) {
    // create globals struct that all threads will access (with padding to prevent false sharing on control logic meta data)
    auto g = new globals_t(millisToRun, totalThreads, K, arraySize);
    
    /**
     * 
     * RUN EXPERIMENT
     * 
     */
    
    // create and start threads
    thread * threads[MAX_THREADS]; // just allocate an array for max threads to avoid changing data layout (which can affect results) when varying thread count. the small amount of wasted space is not a big deal.
    for (int tid=0;tid<g->totalThreads;++tid) {
        threads[tid] = new thread([&, tid]() { /* access all variables by reference, except tid, which we copy (since we don't want our tid to be a reference to the changing loop variable) */
                const int OPS_BETWEEN_TIME_CHECKS = 50; // only check the current time (to see if we should stop) once every X operations, to amortize the overhead of time checking
                binding_bindThread(tid);
                
                // BARRIER WAIT
                g->running.fetch_add(1);
                while (!g->start) { TRACE TPRINT("waiting to start"<<endl); } // wait to start
                
                for (int cnt=0; !g->done; ++cnt) {
                    if ((cnt % OPS_BETWEEN_TIME_CHECKS) == 0                    // once every X operations
                        && g->timer.getElapsedMillis() >= g->millisToRun) {   // check how much time has passed
                            g->done = true; // set global "done" bit flag, so all threads know to stop on the next operation (first guy to stop dictates when everyone else stops --- at most one more operation is performed per thread!)
                            __sync_synchronize(); // flush the write to g->done so other threads see it immediately (mostly paranoia, since volatile writes should be flushed, and also our next step will be a fetch&add which is an implied flush on intel/amd)
                    }

                    g->aArray->incrementRandomK(g->rngs[tid]);

                    // Count successful and total kcas operations
                    g->numSuccessfulOps.inc(tid);
                }
                g->running.fetch_add(-1);
                //TPRINT("terminated"<<endl);
        });
    }
    
    while (g->running < g->totalThreads) {
        TRACE cout<<"main thread: waiting for threads to START running="<<g->running<<endl;
    } // wait for all threads to be ready
    
    if (!binding_isInjectiveMapping(g->totalThreads)) {
        std::cout<<"ERROR: thread pinning maps more than one thread to a single logical processor!"<<std::endl;
        exit(-1);
    }
    
    cout<<"main thread: starting timer..."<<endl;
    g->timer.startTimer();
    __sync_synchronize(); // prevent compiler from reordering "start = true;" before the timer start; this is mostly paranoia, since start is volatile, and nothing should be reordered around volatile reads/writes
    
    g->start = true; // release all threads from the barrier, so they can work

    // sleep the main thread for length of time the trial should run
    timespec ts;
    ts.tv_sec = g->millisToRun / 1000;
    ts.tv_nsec = 1000000 * (g->millisToRun % 1000);
    nanosleep(&ts, NULL);

    // wait for all threads to stop working */
    while (g->running > 0) { std::this_thread::yield(); }
    
    // measure and print elapsed time
    g->elapsedMillis = g->timer.getElapsedMillis();
    cout<<(g->elapsedMillis/1000.)<<"s"<<endl;
    
    // join all threads
    for (int tid=0;tid<g->totalThreads;++tid) {
        threads[tid]->join();
        delete threads[tid];
    }
    
    /**
     * Memory layout debugging information
     */
    
    cout<<endl;
    addrinfo_header();
    
    addrinfo(g);
    addrinfo(&g->rngs);
    addrinfo_array(&g->rngs, totalThreads);
    addrinfo(&g->timer);
    addrinfo(&g->elapsedMillis);
    addrinfo(&g->done);
    addrinfo(&g->start);
    addrinfo(&g->running);
    addrinfo(&g->aArray);
    addrinfo(&g->numSuccessfulOps);
    addrinfo(&g->millisToRun);
    addrinfo(&g->totalThreads);

    addrinfo(g->aArray);
    addrinfo(&g->aArray->data);
    addrinfo(&g->aArray->size);
    addrinfo(&g->aArray->K);
    addrinfo(&g->aArray->lock);
    addrinfo(&g->aArray->lock.state);

    addrinfo(&g->aArray->data[0]);
    addrinfo(&g->aArray->data[1]);
    addrinfo(&g->aArray->data[g->aArray->size-1]);
    
    /**
     * 
     * PRODUCE OUTPUT
     * 
     * 
     */
    
    auto successfulOps = g->numSuccessfulOps.getTotal();
    
    auto arrayChecksum = g->aArray->getTotal();
    auto threadsChecksum = successfulOps * g->aArray->K;
    bool valid = (threadsChecksum == arrayChecksum);

    //cout<<"Validation: arrayChecksum="<<arrayChecksum<<", and # successful incrementRandomK = "<<successfulOps<<" and K = "<<g->aArray->K<<" so array sum SHOULD be "<<threadsChecksum<<".";
    cout<<endl;
    cout<<"Validation:";
    cout<<(valid ? " OK." : " FAILED.")<<endl;
    cout<<"  arrayChecksum="<<arrayChecksum<<endl;
    cout<<"threadsChecksum="<<threadsChecksum<<"    (since # successful incrementRandomK = "<<successfulOps<<" and K = "<<g->aArray->K<<")"<<endl;
    cout<<endl;

    if (valid) {
        cout<<"successful ops       : "<<successfulOps<<endl;
        cout<<"throughput           : "<<(long long) (successfulOps * 1000. / g->elapsedMillis)<<endl;
        cout<<"elapsed milliseconds : "<<g->elapsedMillis<<endl;
        cout<<"fallback path count  : "<<g->aArray->lock.numberOfTimesAcquired()<<endl;
        cout<<endl;
    }
    
    if (threadsChecksum != arrayChecksum) {
        cout<<"ERROR: validation failed!"<<endl;
        exit(-1);
    }
    
    delete g;
}

int main(int argc, char** argv) {
    if (argc == 1) {
        cout<<"USAGE: "<<argv[0]<<" [options]"<<endl;
        cout<<"Options:"<<endl;
        cout<<"    -t [int]     milliseconds to run"<<endl;
        cout<<"    -s [int]     size of array that incrementRandomK will be performed on"<<endl;
        cout<<"    -n [int]     number of threads that will perform incrementRandomK"<<endl;
        cout<<"    -k [int]     how many slots each thread should operate on atomically"<<endl;
        cout<<"    -pin [pattern]  pin threads to logical processors according to [pattern], e.g., -pin 0-9,20-29,10-19,30-39"<<endl;
        cout<<"                    (this will pin the first thread to CPU 0, next thread to CPU 1, and so on, then the 11th thread to CPU 20, and so on)"<<endl;
        cout<<endl;
        cout<<"Example: LD_PRELOAD=../common/libjemalloc.so "<<argv[0]<<" -t 3000 -s 1000000 -k 16 -pin 0-9,20-29,10-19,30-39 -n 10"<<endl;
        return 1;
    }
    
    int millisToRun = -1;
    int arraySize = 0;
    int totalThreads = 0;
    int K = 0;
    
    // read command line args
    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i], "-s") == 0) {
            arraySize = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0) {
            totalThreads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0) {
            millisToRun = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-k") == 0) {
            K = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-pin") == 0) { // e.g., "-pin 1,2,3,8-11,4-7,0"
            binding_parseCustom(argv[++i]);
            std::cout<<"parsed custom binding: "<<argv[i]<<std::endl;
        } else {
            cout<<"bad arguments"<<endl;
            exit(1);
        }
    }
    
    // print command and args for debugging
    std::cout<<"Cmd:";
    for (int i=0;i<argc;++i) {
        std::cout<<" "<<argv[i];
    }
    std::cout<<std::endl;
    
    // print configuration for debugging
    PRINT(MAX_THREADS);
    PRINT(K);
    PRINT(millisToRun);
    PRINT(arraySize);
    PRINT(totalThreads);
    cout<<endl;
    
    // check for too large thread count
    if (totalThreads >= MAX_THREADS) {
        std::cout<<"ERROR: totalThreads="<<totalThreads<<" >= MAX_THREADS="<<MAX_THREADS<<std::endl;
        return 1;
    }
    
    // check for size too small
    if (arraySize < K) {
        std::cout<<"ERROR: arraySize="<<arraySize<<" < K="<<K<<std::endl;
        return 1;
    }
    
    // configure thread pinning/binding (according to command line args)
    // and run experiment
    binding_configurePolicy(totalThreads);
    runExperiment(arraySize, millisToRun, totalThreads, K);
    binding_deinit();
    
    return 0;
}

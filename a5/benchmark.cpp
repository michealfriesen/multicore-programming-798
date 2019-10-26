/**
 * A simple insert & delete benchmark for data structures that implement a set.
 */

#include <thread>
#include <cstdlib>
#include <atomic>
#include <string>
#include <cstring>
#include <iostream>

#include "defines.h"
#include "util.h"

#include "doubly_linked_list_kcas.h"
#include "doubly_linked_list_kcas_reclaim.h"

using namespace std;

template <class DataStructureType>
struct globals_t {
    PaddedRandom rngs[MAX_THREADS];
    volatile char padding0[PADDING_BYTES];
    ElapsedTimer timer;
    volatile char padding1[PADDING_BYTES];
    ElapsedTimer timerFromStart;
    volatile char padding3[PADDING_BYTES];
    volatile bool done;
    volatile char padding4[PADDING_BYTES];
    volatile bool start;        // used for a custom barrier implementation (should threads start yet?)
    volatile char padding5[PADDING_BYTES];
    atomic_int running;         // used for a custom barrier implementation (how many threads are waiting?)
    volatile char padding6[PADDING_BYTES];
    DataStructureType * ds;
    debugCounter numTotalOps;   // already has padding built in at the beginning and end
    debugCounter keyChecksum;
    debugCounter sizeChecksum;
    int millisToRun;
    int totalThreads;
    int keyRangeSize;
    volatile char padding7[PADDING_BYTES];
    size_t garbage; // garbage variable that will be useful for preventing some code from being optimized out
    volatile char padding8[PADDING_BYTES];
    
    globals_t(int _millisToRun, int _totalThreads, int _keyRangeSize, DataStructureType * _ds) {
        for (int i=0;i<MAX_THREADS;++i) {
            rngs[i].setSeed(i+1); // +1 because we don't want thread 0 to get a seed of 0, since seeds of 0 usually mean all random numbers are zero...
        }
        done = false;
        start = false;
        running = 0;
        ds = _ds;
        millisToRun = _millisToRun;
        totalThreads = _totalThreads;
        keyRangeSize = _keyRangeSize;
        garbage = -1;
    }
    ~globals_t() {
        delete ds;
    }
} __attribute__((aligned(PADDING_BYTES)));

void runTrial(auto g, const long millisToRun, double insertPercent, double deletePercent) {
    g->done = false;
    g->start = false;
    
    // create and start threads
    thread * threads[MAX_THREADS]; // just allocate an array for max threads to avoid changing data layout (which can affect results) when varying thread count. the small amount of wasted space is not a big deal.
    for (int tid=0;tid<g->totalThreads;++tid) {
        threads[tid] = new thread([&, tid]() { /* access all variables by reference, except tid, which we copy (since we don't want our tid to be a reference to the changing loop variable) */
            const int OPS_BETWEEN_TIME_CHECKS = 500; // only check the current time (to see if we should stop) once every X operations, to amortize the overhead of time checking
            size_t garbage = 0; // will prevent contains() calls from being optimized out
            
            // BARRIER WAIT
            g->running.fetch_add(1);
            while (!g->start) { TRACE TPRINT("waiting to start"<<endl); }               // wait to start
            
            int key = 0;                
            for (int cnt=0; !g->done; ++cnt) {
                if ((cnt % OPS_BETWEEN_TIME_CHECKS) == 0                                // once every X operations
                        && g->timer.getElapsedMillis() >= millisToRun)                  // check how much time has passed
                    g->done = true; // set global "done" bit flag, so all threads know to stop on the next operation (first guy to stop dictates when everyone else stops --- at most one more operation is performed per thread!)
                
                // flip a coin to decide: insert or erase?
                // generate a random double in [0, 100]
                double operationType = g->rngs[tid].nextNatural() / (double) numeric_limits<unsigned int>::max() * 100;
                
                // generate random key in [1, g->keyRangeSize]
                key = (int) (1 + (g->rngs[tid].nextNatural() % g->keyRangeSize));
                
                // insert or delete this key (50% probability of each)
                if (operationType < insertPercent) {
                    auto result = g->ds->insertIfAbsent(tid, key);
                    if (result) {
                        g->keyChecksum.add(tid, key);
                        g->sizeChecksum.add(tid, 1);
                    }
                } else if (operationType < insertPercent + deletePercent) {
                    auto result = g->ds->erase(tid, key);
                    if (result) {
                        g->keyChecksum.add(tid, -key);
                        g->sizeChecksum.add(tid, -1);
                    }
                } else {
                    auto result = g->ds->contains(tid, key);
                    garbage += result; // "use" the return value of contains, so contains isn't optimized out
                }
                
                g->numTotalOps.inc(tid);
            }
            
            g->running.fetch_add(-1);
            __sync_fetch_and_add(&g->garbage, garbage); // "use" the return values of all contains
        });
    }
    
    while (g->running < g->totalThreads) {
        TRACE cout<<"main thread: waiting for threads to START running="<<g->running<<endl;
    }
    g->timer.startTimer();    
    __sync_synchronize(); // prevent compiler from reordering "start = true;" before the timer start; this is mostly paranoia, since start is volatile, and nothing should be reordered around volatile reads/writes
    g->start = true; // release all threads from the barrier, so they can work
    
    while (g->running > 0) { /* wait for all threads to stop working */ }
    
    
    // join all threads
    for (int tid=0;tid<g->totalThreads;++tid) {
        threads[tid]->join();
        delete threads[tid];
    }
}

template <class DataStructureType>
void runExperiment(int keyRangeSize, int millisToRun, int totalThreads, double insertPercent, double deletePercent) {
    // create globals struct that all threads will access (with padding to prevent false sharing on control logic meta data)
    int minKey = 0;
    int maxKey = keyRangeSize;
    auto dataStructure = new DataStructureType(totalThreads, minKey, maxKey);
    auto g = new globals_t<DataStructureType>(millisToRun, totalThreads, keyRangeSize, dataStructure);
    
    /**
     * 
     * PREFILL DATA STRUCTURE TO CONTAIN HALF OF THE KEY RANGE
     * (or, if the ratio of insertions to deletions is not 50/50, 
     *  we will prefill the data structure to contain the appropriate fraction
     *  of the key range. for example, with 90% ins, 10% delete,
     *  we prefill to the steady state: 90% full.)
     * (with 0% insert and 0% delete, we prefill to half full.)
     *  
     */
    
    g->timerFromStart.startTimer();
    if(keyRangeSize > 2){
        for (int attempts=0;;++attempts) {
            double totalUpdatePercent = insertPercent + deletePercent;
            double prefillingInsertPercent = (totalUpdatePercent < 1e-6) ? 50 : (insertPercent / totalUpdatePercent) * 100;
            double prefillingDeletePercent = (totalUpdatePercent < 1e-6) ? 50 : (deletePercent / totalUpdatePercent) * 100;
            auto expectedSize = keyRangeSize * prefillingInsertPercent / 100;

            //cout<<"expectedSize="<<expectedSize<<" prefillingInsertPercent="<<prefillingInsertPercent<<" prefillingDeletePercent="<<prefillingDeletePercent<<endl;
            runTrial(g, 200, prefillingInsertPercent, prefillingDeletePercent);

            // measure and print elapsed time
            cout<<"prefilling round "<<attempts<<" ending size "<<g->sizeChecksum.getTotal()<<" total elapsed time="<<(g->timerFromStart.getElapsedMillis()/1000.)<<"s"<<endl;

            // check if prefilling is done
            if (g->sizeChecksum.getTotal() > 0.95 * expectedSize) {
                cout<<"prefilling completed to size "<<g->sizeChecksum.getTotal()<<" (within 5% of expected size "<<expectedSize<<" with key checksum "<<g->keyChecksum.getTotal()<<")"<<endl;
                break;
            } else if (attempts > 100) {
                cout<<"failed to prefill in a reasonable time to within an error of 5% of the expected size; final size "<<g->sizeChecksum.getTotal()<<" expected "<<expectedSize<<endl;
                exit(0);
            }
        }
        cout<<endl;
    }
    else {
        cout<<"Prefilling skipped for small key range..."<<endl;
    }
    
    /**
     * 
     * RUN EXPERIMENT
     * 
     */
    
    cout<<"main thread: experiment starting..."<<endl;
    runTrial(g, g->millisToRun, insertPercent, deletePercent);
    cout<<"main thread: experiment finished..."<<endl;
    cout<<endl;
    
    /**
     * 
     * PRODUCE OUTPUT
     * 
     */
    
    g->ds->printDebuggingDetails();
    
    auto numTotalOps = g->numTotalOps.getTotal();
    auto dsSumOfKeys = g->ds->getSumOfKeys();
    auto threadsSumOfKeys = g->keyChecksum.getTotal();
    cout<<"Validation: sum of keys according to the data structure = "<<dsSumOfKeys<<" and sum of keys according to the threads = "<<threadsSumOfKeys<<".";
    cout<<((threadsSumOfKeys == dsSumOfKeys) ? " OK." : " FAILED.")<<endl;
    cout<<"sizeChecksum="<<g->sizeChecksum.getTotal()<<endl;
    cout<<endl;

    cout<<"completedOperations="<<numTotalOps<<endl;
    cout<<"throughput="<<(long long) (numTotalOps * 1000. / g->millisToRun)<<endl;
    cout<<endl;
    
    if (threadsSumOfKeys != dsSumOfKeys) {
        cout<<"ERROR: validation failed!"<<endl;
        exit(0);
    }
    
    if (g->garbage == 0) cout<<endl; // "use" the g->garbage variable, so effectively the return values of all contains() are "used," so they can't be optimized out
    cout<<"total elapsed time="<<(g->timerFromStart.getElapsedMillis()/1000.)<<"s"<<endl;
    delete g;
}

int main(int argc, char** argv) {
    if (argc == 1) {
        cout<<"USAGE: "<<argv[0]<<" [options]"<<endl;
        cout<<"Options:"<<endl;
        cout<<"    -t [int]     milliseconds to run"<<endl;
        cout<<"    -s [int]     size of the key range that random keys will be drawn from (i.e., range [1, s])"<<endl;
        cout<<"    -n [int]     number of threads that will perform inserts and deletes"<<endl;
        cout<<"    -r           enables memory reclamation"<<endl;
        cout<<"    -i [double]  percent of operations that will be insert (example: 20)"<<endl;
        cout<<"    -d [double]  percent of operations that will be delete (example: 20)"<<endl;
        cout<<"                 (100 - i - d)% of operations will be contains"<<endl;
        cout<<endl;
        return 1;
    }
    
    int millisToRun = -1;
    int keyRangeSize = 0;
    int totalThreads = 0;
    double insertPercent = 0;
    double deletePercent = 0;
    bool reclaim = false;
    
    // read command line args
    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i], "-s") == 0) {
            keyRangeSize = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0) {
            totalThreads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0) {
            millisToRun = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0) {
            insertPercent = atof(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0) {
            deletePercent = atof(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0) {
            reclaim = true;
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
    PRINT(totalThreads);
    PRINT(keyRangeSize);
    PRINT(insertPercent);
    PRINT(deletePercent);
    PRINT(millisToRun);
    cout<<endl;
    
    // check for too large thread count
    if (totalThreads >= MAX_THREADS) {
        std::cout<<"ERROR: totalThreads="<<totalThreads<<" >= MAX_THREADS="<<MAX_THREADS<<std::endl;
        return 1;
    }
    if(reclaim){
        runExperiment<DoublyLinkedListReclaim>(keyRangeSize, millisToRun, totalThreads, insertPercent, deletePercent);
    }
    else {
        runExperiment<DoublyLinkedList>(keyRangeSize, millisToRun, totalThreads, insertPercent, deletePercent);
    }
    return 0;
}

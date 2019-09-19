#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
using namespace std;

#define MAX_THREADS 256

struct padded_subcounter {
    int64_t v;
    char padding[64 - sizeof(int64_t)];
};

struct globals {
    char padding0[64];
    int numThreads;
    atomic<bool> start;
    atomic<bool> done;
    padded_subcounter subcounters[MAX_THREADS];
    
    globals(int _numThreads) {
        numThreads = _numThreads;
        start = false;
        done = false;
    }
};

globals * g;

void threadFunc(int tid) {
    // wait for all threads to be started before letting any thread do "real" work
    while (!g->start) { /* busy wait */ }

    // increment my subcounter until the experiment is done
    while (true) {
        g->subcounters[tid].v++;
        if (g->done) break;
    }
}

int main(int argc, char ** argv) {
    // read command line args
    if (argc != 3) {
        cout<<"USAGE: "<<argv[0]<<" SECONDS_TO_RUN NUMBER_OF_THREADS"<<endl;
        return 1;
    }
    int secondsToRun = atoi(argv[1]);
    int numThreads = atoi(argv[2]);
    
    // initialize globals
    g = new globals(numThreads);
    
    // create and start threads
    vector<thread *> threads;
    for (int i=0;i<g->numThreads;++i) {
        threads.push_back(new thread(threadFunc, i));
    }
    
    // have threads perform increments for a fixed time then stop
    g->start = true;
    this_thread::sleep_for(chrono::seconds(secondsToRun));
    g->done = true;
    
    // join threads
    for (int i=0;i<g->numThreads;++i) {
        threads[i]->join();
        delete threads[i]; // free memory
    }
    
    // print: increments performed per second
    int64_t sum = 0;
    for (int i=0;i<g->numThreads;++i) {
        sum += g->subcounters[i].v;
    }
    cout<<"throughput (increments per second)="<<(sum / secondsToRun)<<endl;
    
    delete g; // free memory allocated for globals (and call destructor)
    return 0;
}

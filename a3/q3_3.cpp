#include <iostream>
#include <thread>
using namespace std;
#define TOTAL_INCREMENTS 100000000

__thread int tid;

class mutex_t {
private:
    bool enterArray[2];
    volatile int turn;
public:
    mutex_t() : turn(0) {
        enterArray[0] = false;
        enterArray[1] = false;  
    }
    void lock() {
        enterArray[tid] = true;
        __sync_synchronize(); 
        while(enterArray[1 - tid]) {
            __sync_synchronize();
            if(turn != tid){
                enterArray[tid] = false;
                __sync_synchronize(); 
                while(turn != tid){
                    // Do nothing... (Busy Wait)
                }
                enterArray[tid] = true;
            }
        } 
    }
    void unlock() {
        enterArray[tid] = false;
        turn = 1 - tid;
    }
};


class counter_locked {
private:
    mutex_t m;
    volatile int v;
public:
    counter_locked() : v(0){}
    
    void increment() {
        m.lock();
        __sync_synchronize(); 
        v++;
        m.unlock();
    }

    int get() {
        m.lock();
        auto result = v;
        m.unlock();
        return result;
    }
};

counter_locked c;

void threadFunc(int _tid) {
    tid = _tid;
    for (int i = 0; i < TOTAL_INCREMENTS / 2; ++i) {
        c.increment();
    }
}

int main(void) {
    // create and start threads
    thread t0(threadFunc, 0);
    thread t1(threadFunc, 1);
    t0.join();
    t1.join();
    cout<<c.get()<<endl;
    return 0;
}

#ifndef UTIL_H
#define UTIL_H

#include <atomic>
#include <chrono>

class ElapsedTimer {
private:
    char padding0[64];
    bool calledStart = false;
    char padding1[64];
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    char padding2[64];
public:
    void start() {
        calledStart = true;
        startTime = std::chrono::high_resolution_clock::now();
    }
    int64_t getElapsedMillis() {
        if (!calledStart) {
            printf("ERROR: called getElapsedMillis without calling startTimer\n");
            exit(1);
        }
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    }
};

class Barrier {
private:
    char padding0[64];
    const int releaseValue;
    char padding1[64];
    std::atomic<int> v;
    char padding2[64];
public:
    Barrier(int _releaseValue) : releaseValue(_releaseValue), v(0) {}
    void wait() {
        v++;
        while (v < releaseValue) {}
    }
};

#endif /* UTIL_H */

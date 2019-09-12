Bootstrapping framework for the coding component of assignment 2
in the course Multicore Programming at the University of Waterloo.

Place your counter implementations in: counters_impl.h
Benchmark where threads perform as much work as possible in a period of time: workload_timed.cpp

Compilation:
    make

Usage:
    ./workload_timed.out NUM_THREADS MILLISECONDS_TO_RUN COUNTER_TYPE_NAME
    e.g.,
    ./workload_timed.out 4 3000 naive

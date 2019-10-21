GPP = g++
FLAGS = -g
FLAGS += -std=c++17
FLAGS += -fopenmp
LDFLAGS = -lpthread

all: benchmark benchmark_debug

.PHONY: benchmark
benchmark:
	$(GPP) $(FLAGS) -o $@.out $@.cpp $(LDFLAGS) -DNDEBUG #### NOTE: THIS DISABLES ASSERTIONS!!!

.PHONY: benchmark_debug
benchmark_debug:
	$(GPP) $(FLAGS) -o $@.out benchmark.cpp -DTRACE=if\(1\) $(LDFLAGS)

clean:
	rm -f *.out 

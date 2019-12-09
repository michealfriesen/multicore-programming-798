GPP = g++
FLAGS = -O3 -g -mrtm
FLAGS += -I../common
#FLAGS += -fno-omit-frame-pointer
#FLAGS += -DNDEBUG
LDFLAGS = -pthread

PROGRAMS = benchmark benchmark_sanitize

all: $(PROGRAMS)

build:
	mkdir -p $@

benchmark: build
	$(GPP) $(FLAGS) -MMD -MP -MF build/$@.d -o $@ $@.cpp $(LDFLAGS)
	
benchmark_sanitize:
	$(GPP) $(FLAGS) -MMD -MP -MF build/$@.d -o $@ benchmark.cpp $(LDFLAGS) -fsanitize=address -static-libasan
	
-include $(addprefix build/,$(addsuffix .d, $(PROGRAMS)))

clean:
	rm -rf $(PROGRAMS) build

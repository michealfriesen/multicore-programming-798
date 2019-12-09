GPP = g++
FLAGS = -O3 -g -mrtm
FLAGS += -I../common
#FLAGS += -DNDEBUG
LDFLAGS = -pthread

PROGRAMS = benchmark

all: $(PROGRAMS)

build:
	mkdir -p $@

benchmark: build
	$(GPP) $(FLAGS) -MMD -MP -MF build/$@.d -o $@ $@.cpp $(LDFLAGS)
	
	
-include $(addprefix build/,$(addsuffix .d, $(PROGRAMS)))

clean:
	rm -rf $(PROGRAMS) build



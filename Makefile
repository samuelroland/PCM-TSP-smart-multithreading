#CPPFLAGS=-O3 -pthread
CPPFLAGS=-O3 -pthread -g
#CPPFLAGS=-g
#CPPFLAGS=-std=c++20
LDFLAGS=-latomic

TARGETS=tsp tspprint intvecsort

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

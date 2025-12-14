CPPFLAGS=-O3
CPPFLAGS+=-g
#CPPFLAGS+=-fsanitize=address -fno-omit-frame-pointer
#CPPFLAGS=-std=c++20
#LDFLAGS=-latomic

TARGETS=tsp tspprint intvecsort

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

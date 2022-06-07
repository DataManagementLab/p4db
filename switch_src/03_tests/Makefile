CXX=g++
CFLAGS=-std=c++2a -O0 -g -MD -Wall -Wextra -fmax-errors=1 -I. -I../../src/ -fsanitize=address 
# CFLAGS=-std=c++2a -O3 -g -MD -Wall -Wextra -fmax-errors=1 -I. -I../../src/
LIBS=-pthread


BINS = micro_recirc lock_manager ycsb smallbank tpcc

SRCS = ../../src/db/hex_dump.cpp
OBJS := $(notdir $(SRCS:.cpp=.o))



all: $(BINS)


$(OBJS): $(SRCS) #$(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)


%.o: %.cpp #$(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)



$(BINS): %: %.o $(OBJS)
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)


.PHONY: clean
clean:
	rm -f *.o *.d *~ $(BINS)

-include $(OBJS:.o=.d)

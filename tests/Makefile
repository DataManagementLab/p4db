CXX=g++
CFLAGS=-std=c++2a -O0 -g -MD -Wall -Wextra -fmax-errors=1 -I. -I../src/ -fsanitize=address 
# CFLAGS=-std=c++2a -O3 -g -MD -Wall -Wextra -fmax-errors=1 -I. -I../src
LIBS=-pthread


OBJS = #main.o


BINS = access_dist nolock_test nurand shared_lock tpcc_neworder zipf


all: $(BINS)


%.o: %.cpp #$(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)

# main: $(OBJS)
# 	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)


$(BINS): %: %.o $(OBJS)
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)


.PHONY: clean
clean:
	rm -f *.o *.d *~ $(BINS)

-include $(OBJS:.o=.d)

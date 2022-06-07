CXX=g++
CFLAGS=-std=c++2a -O0 -g -MD -Wall -Wextra -fmax-errors=1 -I. -I../../src/ -I./MQLib/include/ 
# CFLAGS=-std=c++2a -O3 -g -MD -Wall -Wextra -fmax-errors=1 -I. -I../../src
LIBS=-pthread -L./MQLib/bin -l:MQLib.a -fsanitize=address 


#OBJS = test.o declustered_layout.o transaction.o
SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))


%.o: %.cpp #$(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)

test: $(OBJS)
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)


.PHONY: clean

clean:
	rm -f *.o *.d *~ test *.dot *.png

-include $(OBJS:.o=.d)

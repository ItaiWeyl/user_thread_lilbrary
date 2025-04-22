CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -fPIC
AR = ar
ARFLAGS = rcs
LIB = libuthreads.a

OBJS = scheduler.o thread.o uthreads.o

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(LIB)

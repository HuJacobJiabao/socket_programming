# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -std=c++11

TARGETS = serverA serverM serverP serverQ client


SRCS = $(addsuffix .cpp, $(TARGETS))

OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGETS)

%: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# Clean target
.PHONY: clean all
clean:
	rm -f $(TARGETS) *.o

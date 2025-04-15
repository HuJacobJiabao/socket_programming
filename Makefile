CXX = g++
CXXFLAGS = -Wall -std=c++11

SRCS = serverA.cpp serverM.cpp serverP.cpp serverQ.cpp client.cpp
OBJS = $(SRCS:.cpp=)

all: serverA serverP serverQ serverM client

serverA: serverA.cpp
	$(CXX) $(CXXFLAGS) -o serverA serverA.cpp

serverP: serverP.cpp
	$(CXX) $(CXXFLAGS) -o serverP serverP.cpp

serverQ: serverQ.cpp
	$(CXX) $(CXXFLAGS) -o serverQ serverQ.cpp

serverM: serverM.cpp
	$(CXX) $(CXXFLAGS) -o serverM serverM.cpp

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp

clean:
	rm -f serverA serverM serverP serverQ client
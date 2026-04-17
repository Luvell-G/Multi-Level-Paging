CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -g

all: sim

sim: main.o simulator.o
	$(CXX) $(CXXFLAGS) -o sim main.o simulator.o

main.o: main.cpp simulator.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

simulator.o: simulator.cpp simulator.hpp
	$(CXX) $(CXXFLAGS) -c simulator.cpp

clean:
	rm -f *.o sim
CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: scn

scn: scn.cpp ../tool_common.hpp
	$(CXX) $(CXXFLAGS) scn.cpp -o scn

clean:
	rm -f scn

CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: mrg

mrg: mrg.cpp ../tool_common.hpp
	$(CXX) $(CXXFLAGS) mrg.cpp -o mrg

clean:
	rm -f mrg

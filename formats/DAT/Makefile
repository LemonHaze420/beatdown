CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: dat

dat: dat.cpp ../tool_common.hpp
	$(CXX) $(CXXFLAGS) dat.cpp -o dat

clean:
	rm -f dat

CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: lgp

lgp: lgp.cpp ../tool_common.hpp
	$(CXX) $(CXXFLAGS) lgp.cpp -o lgp

clean:
	rm -f lgp

CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: ol

ol: ol.cpp ../tool_common.hpp
	$(CXX) $(CXXFLAGS) ol.cpp -o ol

clean:
	rm -f ol

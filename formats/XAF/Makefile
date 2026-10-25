CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2

SOURCES := xaf.cpp xaf_bin.cpp
HEADERS := xaf.h xaf_format.h ../common.h

.PHONY: all clean

all: xaf

xaf: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o xaf

clean:
	rm -f xaf

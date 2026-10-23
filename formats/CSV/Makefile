CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2

SOURCES := csv.cpp csv_bin.cpp
HEADERS := csv.h

.PHONY: all clean

all: csv

csv: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o csv

clean:
	rm -f csv

CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2

SOURCES := text_table.cpp text_table_bin.cpp
HEADERS := text_table.h ../common.h

.PHONY: all clean

all: text_table

text_table: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o text_table

clean:
	rm -f text_table

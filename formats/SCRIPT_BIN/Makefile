CXX ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2

SOURCES = script.cpp script_bin.cpp source_parser.cpp
HEADERS = script.h script_bin.h source_parser.h event_isa.h ../common.h

all: script_bin

script_bin: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES)

clean:
	rm -f script script_bin

.PHONY: all clean

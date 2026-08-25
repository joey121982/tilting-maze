# Builds the host oracle: the firmware's own maze core, compiled natively.
#
# host_json_dump links Core/Src/maze_core.cpp unchanged, so its output is byte-for-byte
# what the board transmits. That is what makes the companion app testable, and the wire
# format verifiable, without any hardware attached.
#
#   make            build
#   make run        build and print one maze (seed 42)
#   make clean

FIRMWARE := ../../../firmware/main

CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I$(FIRMWARE)/Core/Inc

TARGET   := host_json_dump
SOURCES  := host_json_dump.cpp $(FIRMWARE)/Core/Src/maze_core.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(FIRMWARE)/Core/Inc/maze_core.h
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@

run: $(TARGET)
	./$(TARGET) 42

clean:
	rm -f $(TARGET)

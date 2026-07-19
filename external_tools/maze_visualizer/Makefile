CXX = g++
CXXFLAGS = -std=c++17 -O3

FIRMWARE_DIR = ../../firmware/main
INC_DIR = $(FIRMWARE_DIR)/Core/Inc
SRC_CORE = $(FIRMWARE_DIR)/Core/Src/maze_core.cpp

SRC_MAIN = maze_console_main.cpp
TARGET = maze_generator

run: $(TARGET)
	python3 vis.py

$(TARGET): $(SRC_MAIN) $(SRC_CORE)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) $(SRC_MAIN) $(SRC_CORE) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clean
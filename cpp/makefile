# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lgtest -lgtest_main

# Directories
SRC_DIR	= src
INC_DIR = include
BUILD_DIR = build

# Sources and Objects
LIB_SRCS = $(SRC_DIR)/entry.cpp \
		   $(SRC_DIR)/log.cpp \
		   $(SRC_DIR)/kv.cpp \

# OS detection
ifeq ($(OS), Windows_NT)
	LIB_SRCS += $(SRC_DIR)/platform_windows.cpp
	CXXFLAGS += -DPLATFORM_WINDOWS
else
	UNAME := $(shell uname -s)
	ifeq ($(UNAME), Linux)
		LIB_SRCS += $(SRC_DIR)/platform_unix.cpp
		CXXFLAGS += -pthread -DPLATFORM_LINUX
		LDFLAGS += -pthread
	else ifeq ($(UNAME), Darwin)
		LIB_SRCS += $(SRC_DIR)/platform_unix.cpp
		CXXFLAGS += -pthread -D_MACOS
		LDFLAGS += -pthread
	else
		$(error Unsupported OS: $(UNAME))
	endif
endif

# Objects and target
LIB_OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(LIB_SRCS))

TEST_SRC = test_kv.cpp
TARGET = $(BUILD_DIR)/kv_test

HDR = database.h

# Default rule
all: $(TARGET)

# Link test executable
$(TARGET): $(BUILD_DIR)/test_kv.o $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Compile library sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile test source
$(BUILD_DIR)/test_kv.o: $(TEST_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create build directory if missing
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run the tests
test: $(TARGET)
	./$(TARGET)

# Memory check using Valgrind
memcheck: $(TARGET)
	valgrind --leak-check=full ./$(TARGET)

# Generate Doxygen documentation
doc:
	doxygen Doxyfile

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) html/ latex/

.PHONY: all test memcheck doc clean
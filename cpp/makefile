# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -lgtest -lgtest_main -pthread

# Targets
TARGET = kv_test
SRC = test_kv.cpp
HDR = database.h

# Default rule
all: $(TARGET)

# Link the test executable
$(TARGET): $(SRC) $(HDR)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

# Run the tests
test: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET)

.PHONY: all test clean
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
	rm -rf html/ latex/

# Memory check using Valgrind
memcheck: $(TARGET)
	valgrind --leak-check=full ./$(TARGET)

.PHONY: all test clean memcheck

# Generate Doxygen documentation
doc:
	doxygen Doxyfile

.PHONY: doc
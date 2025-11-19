# XINIM Test Suite Makefile
# Week 7: IPC Validation Tests

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -I../include -I../src
LDFLAGS =

# Test binaries
TESTS = ipc_basic_test

.PHONY: all clean run

all: $(TESTS)

ipc_basic_test: ipc_basic_test.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TESTS) *.o

run: all
	@echo "========================================="
	@echo "Running XINIM Test Suite"
	@echo "========================================="
	@for test in $(TESTS); do \
		echo "\nRunning $$test..."; \
		./$$test || exit 1; \
	done
	@echo "\n========================================="
	@echo "All tests passed!"
	@echo "========================================="

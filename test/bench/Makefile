# Makefile for XINIM Lock Framework Benchmarks

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -pthread
INCLUDES = -I../../src/kernel

# Targets
all: lock_benchmark

lock_benchmark: lock_benchmark.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

clean:
	rm -f lock_benchmark

run: lock_benchmark
	@echo "Running lock benchmarks with default settings..."
	./lock_benchmark

run-quick: lock_benchmark
	@echo "Running quick benchmarks (10k iterations)..."
	./lock_benchmark 10000 1

run-intensive: lock_benchmark
	@echo "Running intensive benchmarks (1M iterations)..."
	./lock_benchmark 1000000 1

help:
	@echo "XINIM Lock Framework Benchmark Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build lock_benchmark (default)"
	@echo "  clean        - Remove built binaries"
	@echo "  run          - Run benchmarks with default settings (100k iterations, 1us CS)"
	@echo "  run-quick    - Quick test (10k iterations)"
	@echo "  run-intensive - Intensive test (1M iterations)"
	@echo ""
	@echo "Usage:"
	@echo "  ./lock_benchmark [iterations] [critical_section_us]"
	@echo ""
	@echo "Examples:"
	@echo "  ./lock_benchmark           # 100k iterations, 1us critical section"
	@echo "  ./lock_benchmark 50000     # 50k iterations, 1us critical section"
	@echo "  ./lock_benchmark 100000 5  # 100k iterations, 5us critical section"

.PHONY: all clean run run-quick run-intensive help

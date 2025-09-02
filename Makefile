# XINIM Makefile
# Simple build interface for common tasks

.PHONY: all build clean test audit inventory format help

# Default target
all: build

# Build using xmake
build:
	@echo "Building XINIM..."
	@xmake

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf build
	@rm -rf .xmake
	@rm -f xinim_build
	@echo "Clean complete"

# Run tests
test:
	@echo "Running tests..."
	@xmake build -g test
	@xmake run -g test

# Run code quality audit
audit:
	@echo "Running code quality audit..."
	@./tools/code_audit.sh

# Generate repository inventory
inventory:
	@echo "Generating repository inventory..."
	@python3 tools/repo_inventory.py

# Format code
format:
	@echo "Formatting C++ code..."
	@find . -name "*.cpp" -o -name "*.hpp" -o -name "*.c" -o -name "*.h" | \
		grep -v third_party | grep -v build | head -100 | \
		xargs clang-format -i -style=file

# Show statistics
stats:
	@echo "Repository Statistics:"
	@echo "======================"
	@echo "Total files: $$(find . -type f | wc -l)"
	@echo "C++ files: $$(find . -name '*.cpp' -o -name '*.hpp' | wc -l)"
	@echo "C files: $$(find . -name '*.c' -o -name '*.h' | wc -l)"
	@echo "Lines of code: $$(find . -name '*.cpp' -o -name '*.hpp' -o -name '*.c' -o -name '*.h' | xargs wc -l | tail -1)"

# Help target
help:
	@echo "XINIM Build System"
	@echo "=================="
	@echo ""
	@echo "Available targets:"
	@echo "  make build     - Build the project using xmake"
	@echo "  make clean     - Remove all build artifacts"
	@echo "  make test      - Build and run tests"
	@echo "  make audit     - Run code quality audit"
	@echo "  make inventory - Generate repository inventory"
	@echo "  make format    - Format source code"
	@echo "  make stats     - Show repository statistics"
	@echo "  make help      - Show this help message"
	@echo ""
	@echo "Build modes:"
	@echo "  xmake f -m debug    - Configure for debug build"
	@echo "  xmake f -m release  - Configure for release build"
	@echo "  xmake f -m profile  - Configure for profiling"
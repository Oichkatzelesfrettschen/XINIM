# Makefile for MINIX Boot Image Builder
# Modern C++23 implementation with comprehensive build options

# Compiler configuration
CXX := clang++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -Werror -O2 \
    -march=x86-64-v1 -mtune=generic
CXXFLAGS += -Wno-unused-parameter -Wno-missing-field-initializers

# Debug configuration
DEBUG_FLAGS := -g -DDEBUG -fsanitize=address -fsanitize=undefined
RELEASE_FLAGS := -DNDEBUG -flto

# Project structure
SRCDIR := .
BUILDDIR := build
SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS := $(SOURCES:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)
TARGET := $(BUILDDIR)/build

# Default target
.PHONY: all clean debug release format lint test install

all: release

# Create build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Object files
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Release build
release: CXXFLAGS += $(RELEASE_FLAGS)
release: $(TARGET)

# Debug build
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)

# Link target
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR)

# Format code using clang-format
format:
	clang-format -i $(SOURCES) $(wildcard $(SRCDIR)/*.hpp)

# Lint with clang-tidy
lint:
	clang-tidy $(SOURCES) -- $(CXXFLAGS)

# Install to system location
install: release
	install -m 755 $(TARGET) /usr/local/bin/minix-build

# Dependencies
-include $(OBJECTS:.o=.d)

# Generate dependency files
$(BUILDDIR)/%.d: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -MM -MT $(@:.d=.o) $< > $@


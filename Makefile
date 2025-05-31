# MINIX Filesystem Tools Makefile
# Modern C++17 build configuration with comprehensive options

# Compiler and tools configuration
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS := -I. -MMD -MP
LDFLAGS := 
LIBS := 

# Build modes
DEBUG_FLAGS := -g3 -O0 -DDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer
RELEASE_FLAGS := -O3 -DNDEBUG -flto -march=native
PROFILE_FLAGS := -O2 -g -pg

# Platform-specific settings
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LIBS += -pthread
    CPPFLAGS += -D_GNU_SOURCE
endif
ifeq ($(UNAME_S),Darwin)
    CPPFLAGS += -D_DARWIN_C_SOURCE
endif

# Directories
SRCDIR := .
OBJDIR := obj
BINDIR := bin
DEPDIR := $(OBJDIR)/.deps

# Source files
SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS := $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEPENDS := $(SOURCES:$(SRCDIR)/%.cpp=$(DEPDIR)/%.d)

# Targets
TARGETS := fsck

# Default build mode
BUILD_MODE ?= debug

# Build mode selection
ifeq ($(BUILD_MODE),debug)
    CXXFLAGS += $(DEBUG_FLAGS)
else ifeq ($(BUILD_MODE),release)
    CXXFLAGS += $(RELEASE_FLAGS)
else ifeq ($(BUILD_MODE),profile)
    CXXFLAGS += $(PROFILE_FLAGS)
else
    $(error Invalid BUILD_MODE: $(BUILD_MODE). Use debug, release, or profile)
endif

# Default target
.PHONY: all
all: $(TARGETS)

# Create directories
$(OBJDIR) $(BINDIR) $(DEPDIR):
	@mkdir -p $@

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR) $(DEPDIR)
	@echo "Compiling $<"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MF $(DEPDIR)/$*.d -c $< -o $@

# Link executable
$(BINDIR)/fsck: $(OBJDIR)/fsck.o $(OBJDIR)/diskio.o | $(BINDIR)
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)

# Convenience target
fsck: $(BINDIR)/fsck

# Install target
.PHONY: install
install: $(BINDIR)/fsck
	@echo "Installing to /usr/local/bin"
	@sudo cp $< /usr/local/bin/
	@sudo chmod 755 /usr/local/bin/fsck

# Clean targets
.PHONY: clean
clean:
	@echo "Cleaning build artifacts"
	@rm -rf $(OBJDIR) $(BINDIR)

.PHONY: distclean
distclean: clean
	@echo "Cleaning all generated files"
	@rm -f core core.* *.core vgcore.* gmon.out

# Development targets
.PHONY: check
check: $(BINDIR)/fsck
	@echo "Running basic checks"
	@./$(BINDIR)/fsck --help >/dev/null
	@echo "Basic checks passed"

.PHONY: lint
lint:
	@echo "Running static analysis"
	@cppcheck --enable=all --std=c++17 --suppress=missingIncludeSystem $(SRCDIR)/*.cpp $(SRCDIR)/*.hpp

.PHONY: format
format:
	@echo "Formatting source code"
	@clang-format -i $(SRCDIR)/*.cpp $(SRCDIR)/*.hpp

.PHONY: docs
docs:
	@echo "Generating documentation"
	@doxygen Doxyfile

# Debug and analysis targets
.PHONY: debug
debug:
	@$(MAKE) BUILD_MODE=debug

.PHONY: release
release:
	@$(MAKE) BUILD_MODE=release

.PHONY: profile
profile:
	@$(MAKE) BUILD_MODE=profile

.PHONY: valgrind
valgrind: debug
	@echo "Running valgrind memory check"
	@valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
		--track-origins=yes --verbose ./$(BINDIR)/fsck --help

.PHONY: gdb
gdb: debug
	@echo "Starting GDB"
	@gdb ./$(BINDIR)/fsck

# Help target
.PHONY: help
help:
	@echo "MINIX Filesystem Tools Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all         Build all targets (default)"
	@echo "  fsck        Build fsck utility"
	@echo "  clean       Remove build artifacts"
	@echo "  distclean   Remove all generated files"
	@echo "  install     Install to system directories"
	@echo "  check       Run basic functionality checks"
	@echo "  lint        Run static analysis"
	@echo "  format      Format source code"
	@echo "  docs        Generate documentation"
	@echo "  valgrind    Run memory check with valgrind"
	@echo "  gdb         Start debugger"
	@echo "  help        Show this help message"
	@echo ""
	@echo "Build modes (BUILD_MODE variable):"
	@echo "  debug       Debug build with sanitizers (default)"
	@echo "  release     Optimized release build"
	@echo "  profile     Profiling build"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Debug build"
	@echo "  make BUILD_MODE=release # Release build"
	@echo "  make clean install      # Clean and install"

# Include dependency files
-include $(DEPENDS)


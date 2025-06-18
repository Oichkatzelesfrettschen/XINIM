# Makefile for math libraries (Quaternion, Octonion, Sedenion)

CXX := g++
# Use C++23 standard, enable warnings, optimization, and position-independent code for library
CXXFLAGS := -std=c++23 -Wall -Wextra -pedantic -O2 -fPIC -I.
AR := ar
ARFLAGS := rcs

# Source files
SOURCES := quaternion.cpp octonion.cpp sedenion.cpp

# Object files: replace .cpp with .o
OBJECTS := $(SOURCES:.cpp=.o)

# Target static library
TARGET_LIB := libmathalgebras.a

# Default target
all: $(TARGET_LIB)

$(TARGET_LIB): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)
	@echo "Math algebra library $(TARGET_LIB) created."

# Rule to compile C++ source files into object files
%.o: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to compile C++ source files into object files (if hpp is not explicit, e.g. only hpp for header-only)
# This is a more general rule, the one above is more specific if hpp must exist.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


clean:
	rm -f $(OBJECTS) $(TARGET_LIB)
	@echo "Cleaned math algebra objects and library."

.PHONY: all clean

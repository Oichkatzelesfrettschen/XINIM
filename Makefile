# Use clang as the default C compiler for all builds
CC ?= clang

# Pass the selected compiler to CMake
CMAKE_FLAGS := -DCMAKE_C_COMPILER=$(CC)
ifdef DRIVER_AT
CMAKE_FLAGS += -DDRIVER_AT=$(DRIVER_AT)
endif
ifdef DRIVER_PC
CMAKE_FLAGS += -DDRIVER_PC=$(DRIVER_PC)
endif
ifdef CROSS_PREFIX
CMAKE_FLAGS += -DCROSS_COMPILE_X86_64=ON -DCROSS_PREFIX=$(CROSS_PREFIX)
endif

all:
	cmake -B build -S . $(CMAKE_FLAGS)
	cmake --build build

clean:
	rm -rf build


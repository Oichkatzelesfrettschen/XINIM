CMAKE_FLAGS :=
ifdef DRIVER_AT
CMAKE_FLAGS += -DDRIVER_AT=$(DRIVER_AT)
endif
ifdef DRIVER_PC
CMAKE_FLAGS += -DDRIVER_PC=$(DRIVER_PC)
endif

all:
	cmake -B build -S . $(CMAKE_FLAGS)
	cmake --build build

clean:
	rm -rf build


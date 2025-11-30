CXX = g++
CXXFLAGS = -O3 -std=c++17 -fopenmp -mavx2
BUILD_DIR = build
SRC_DIR = src

TARGETS = $(BUILD_DIR)/sudoku_serial $(BUILD_DIR)/sudoku_omp $(BUILD_DIR)/sudoku_simd $(BUILD_DIR)/sudoku_omp_simd \
          $(BUILD_DIR)/sudoku_serial_16 $(BUILD_DIR)/sudoku_omp_16 $(BUILD_DIR)/sudoku_simd_16 $(BUILD_DIR)/sudoku_omp_simd_16

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 9x9 Targets
$(BUILD_DIR)/sudoku_serial: $(SRC_DIR)/sudoku_serial.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/sudoku_omp: $(SRC_DIR)/sudoku_omp.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/sudoku_simd: $(SRC_DIR)/sudoku_simd.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/sudoku_omp_simd: $(SRC_DIR)/sudoku_omp_simd.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# 16x16 Targets
$(BUILD_DIR)/sudoku_serial_16: $(SRC_DIR)/sudoku_serial.cpp
	$(CXX) $(CXXFLAGS) -DN=16 -DSQRT_N=4 -o $@ $<

$(BUILD_DIR)/sudoku_omp_16: $(SRC_DIR)/sudoku_omp.cpp
	$(CXX) $(CXXFLAGS) -DN=16 -DSQRT_N=4 -DCUTOFF_DEPTH=7 -o $@ $<

$(BUILD_DIR)/sudoku_simd_16: $(SRC_DIR)/sudoku_simd.cpp
	$(CXX) $(CXXFLAGS) -DN=16 -DSQRT_N=4 -o $@ $<

$(BUILD_DIR)/sudoku_omp_simd_16: $(SRC_DIR)/sudoku_omp_simd.cpp
	$(CXX) $(CXXFLAGS) -DN=16 -DSQRT_N=4 -DCUTOFF_DEPTH=2 -o $@ $<

clean:
	rm -rf $(BUILD_DIR)

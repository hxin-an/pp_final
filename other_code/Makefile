# ----------------------------------------------------------------------
# Makefile for Sudoku Solvers (OpenMP, CUDA, MPI, Pthreads)
# ----------------------------------------------------------------------

# 1. 變數定義 (Variables)
# C/C++ 編譯器
CXX      = g++
MPICXX   = mpicxx
NVCC     = nvcc

# 編譯選項
CXXFLAGS = -std=c++14 -O3 
LDFLAGS  =

# OpenMP 旗標
OMP_FLAGS = -fopenmp

# Pthreads 旗標
PTHREAD_FLAGS = -lpthread

# CUDA 旗標
# 使用 -arch=sm_xx 替換成您的 GPU 架構，例如 sm_75 (Turing/RTX 20 Series) 或 sm_86 (Ampere/RTX 30 Series)
CUDA_ARCH = -arch=sm_75
CUDA_FLAGS = -rdc=true

# 目標執行檔名稱
TARGETS = sudoku_omp sudoku_cuda sudoku_mpi sudoku_pthread

.PHONY: all clean omp cuda mpi pthread

# 2. 預設目標 (Default Target)
all: $(TARGETS)

# 3. 規則定義 (Rules)

# OpenMP 規則 (sudoku_omp)
sudoku_omp: bit_omp.cpp
	$(CXX) $(CXXFLAGS) $(OMP_FLAGS) $< -o $@


# MPI 規則 (sudoku_mpi)
sudoku_mpi: bit_mpi.cpp
	$(MPICXX) $(CXXFLAGS) $< -o $@

# Pthreads/std::thread 規則 (sudoku_pthread)
sudoku_pthread: bit_pthread.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(PTHREAD_FLAGS)
# 4. 清理目標 (Clean Target)
clean:
	rm -f $(TARGETS) *.o

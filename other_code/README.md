Parallel Sudoku Solver — Bitset Version

This project implements a parallelized Sudoku solver using different parallel programming models:
OpenMP, Pthread, and MPI, with a shared bitset-optimized core solver.
Benchmark scripts and result logs are provided.

📂 Project Structure
bitset_parallel/
│
├── Makefile                    # Build all solvers
├── run_benchmark.py            # Run benchmark for serial & parallel versions
├── run_bitset_parallel.py      # Run parallel benchmark only
│
├── generic_bitset.cpp          # Core bitset-based Sudoku solver (serial)
├── bit_omp.cpp                 # OpenMP parallel solver
├── bit_pthread.cpp             # Pthread parallel solver
├── bit_mpi.cpp                 # MPI parallel solver
│
├── sudoku_omp                  # Compiled OpenMP solver
├── sudoku_pthread              # Compiled Pthread solver
├── sudoku_mpi                  # Compiled MPI solver
│
├── benchmark_results.txt       # Serial benchmark results
└── benchmark_parallel_results.txt  # Parallel benchmark results

🚀 Build Instructions

Compile all solvers:

make


Compile individually:

make omp
make pthread
make mpi


Clean binaries:

make clean

▶️ Run Solvers
OpenMP Version
./sudoku_omp puzzles/9x9_medium.txt

Pthread Version
./sudoku_pthread puzzles/9x9_medium.txt

MPI Version
mpirun -np 4 ./sudoku_mpi puzzles/9x9_medium.txt


📊 Running Benchmarks
Full benchmark (serial + parallel):
python3 run_benchmark.py

Parallel-only benchmark:
python3 run_bitset_parallel.py


Results will be logged to:

benchmark_results.txt

benchmark_parallel_results.txt

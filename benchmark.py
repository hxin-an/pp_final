import subprocess
import time
import random
import copy
import os

# Output file
OUTPUT_FILE = "benchmark_results.txt"

def log(message=""):
    print(message)
    with open(OUTPUT_FILE, "a") as f:
        f.write(str(message) + "\n")

# 9x9 Solvers
SOLVERS_9 = [
    "build/sudoku_serial",
    "build/sudoku_omp",
    "build/sudoku_simd",
    "build/sudoku_omp_simd"
]

# 16x16 Solvers
SOLVERS_16 = [
    "build/sudoku_serial_16",
    "build/sudoku_omp_16",
    "build/sudoku_simd_16",
    "build/sudoku_omp_simd_16"
]

DIFFICULTIES_9 = {
    "Easy": 35,    
    "Medium": 45,  
    "Hard": 52,    
    "Expert": 58   
}

# 16x16 has 256 cells.
# Easy: remove ~40% (100)
# Medium: remove ~50% (128)
# Hard: remove ~60% (150)
# Expert: remove ~70% (180)
DIFFICULTIES_16 = {
    "Easy": 100,
    "Medium": 128,
    "Hard": 150,
    "Expert": 180
}

THREADS_TO_TEST = [1, 2, 4, 8, 12, 16, 24]

def generate_sudoku(base, remove_count):
    side = base * base

    # pattern for a baseline valid solution
    def pattern(r,c): return (base*(r%base)+r//base+c)%side

    # randomize rows, columns and numbers (of valid base pattern)
    from random import sample
    def shuffle(s): return sample(s,len(s)) 
    rBase = range(base) 
    rows  = [ g*base + r for g in shuffle(rBase) for r in shuffle(rBase) ] 
    cols  = [ g*base + c for g in shuffle(rBase) for c in shuffle(rBase) ]
    nums  = shuffle(range(1,base*base+1))

    # produce board using randomized baseline pattern
    board = [ [nums[pattern(r,c)] for c in cols] for r in rows ]

    # Remove numbers to create puzzle
    squares = side*side
    empties = remove_count
    for p in sample(range(squares), empties):
        board[p//side][p%side] = 0

    return board

def grid_to_string(grid):
    return "\n".join(" ".join(map(str, row)) for row in grid)

def run_solver(solver_path, grid_str, num_threads=1):
    if not os.path.exists(solver_path):
        return -1.0
        
    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = str(num_threads)

    try:
        process = subprocess.Popen(
            [solver_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env
        )
        # Set a timeout to prevent hanging (e.g. 10 seconds)
        try:
            stdout, stderr = process.communicate(input=grid_str, timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            return -2.0 # Timeout code

        for line in stdout.splitlines():
            if "ms" in line:
                return float(line.split()[0])
        return 0.0
    except Exception as e:
        log(f"Error running {solver_path}: {e}")
        return -1.0

def run_benchmark(size_name, base, difficulties, solvers):
    log(f"\n=== {size_name} Sudoku Benchmark ===")
    
    # Header
    header = f"{'Difficulty':<10} | {'Solver':<25} | {'Threads':<8} | {'Time (ms)':<10}"
    log(header)
    log("-" * len(header))

    # Structure: results[diff][solver][threads] = [time1, time2, ...]
    results = {}
    for diff in difficulties:
        results[diff] = {}
        for solver in solvers:
            results[diff][solver] = {}
            for t in THREADS_TO_TEST:
                results[diff][solver][t] = []
    
    NUM_TRIALS = 5

    for diff_name, remove_count in difficulties.items():
        log(f"Running {diff_name} tests ({NUM_TRIALS} trials)...")
        
        for i in range(NUM_TRIALS):
            grid = generate_sudoku(base, remove_count)
            grid_str = grid_to_string(grid)
            
            for solver in solvers:
                # If serial, only run with 1 thread (conceptually)
                is_serial = "serial" in solver or "simd" in solver and "omp" not in solver
                
                test_threads = [1] if is_serial else THREADS_TO_TEST
                
                for t in test_threads:
                    time_ms = run_solver(solver, grid_str, t)
                    if time_ms >= 0:
                        results[diff_name][solver][t].append(time_ms)
                    elif time_ms == -2.0:
                        # Timeout
                        pass

    log("\n" + "="*80)
    log(f"{'Difficulty':<10} | {'Solver':<25} | {'Threads':<8} | {'Avg Time (ms)':<15}")
    log("="*80)
    
    for diff_name in difficulties:
        best_time = float('inf')
        best_solver = ""
        best_thread = -1
        
        for solver in solvers:
            is_serial = "serial" in solver or "simd" in solver and "omp" not in solver
            test_threads = [1] if is_serial else THREADS_TO_TEST
            
            for t in test_threads:
                times = results[diff_name][solver][t]
                solver_name = os.path.basename(solver)
                
                if times:
                    avg_time = sum(times) / len(times)
                    log(f"{diff_name:<10} | {solver_name:<25} | {t:<8} | {avg_time:.4f}")
                    
                    if avg_time < best_time:
                        best_time = avg_time
                        best_solver = solver_name
                        best_thread = t
                else:
                    log(f"{diff_name:<10} | {solver_name:<25} | {t:<8} | Failed/Timeout")
        
        log("-" * 80)
        if best_solver:
            log(f"Winner for {diff_name}: {best_solver} (Threads: {best_thread}) ({best_time:.4f} ms)")
        else:
            log(f"Winner for {diff_name}: None")
        log("="*80)

def main():
    # Clear log file
    with open(OUTPUT_FILE, "w") as f:
        f.write(f"Benchmark Run at {time.ctime()}\n")

    # Run 9x9
    run_benchmark("9x9", 3, DIFFICULTIES_9, SOLVERS_9)
    
    # Run 16x16
    run_benchmark("16x16", 4, DIFFICULTIES_16, SOLVERS_16)

if __name__ == "__main__":
    main()

import subprocess
import time
import os
import glob

# Output file
OUTPUT_FILE = "benchmark_real_results.txt"

def log(message=""):
    print(message)
    with open(OUTPUT_FILE, "a") as f:
        f.write(str(message) + "\n")

# 9x9 Solvers
SOLVERS = [
    "build/sudoku_serial",
    "build/sudoku_omp",
    "build/sudoku_simd",
    "build/sudoku_omp_simd"
]

THREADS_TO_TEST = [1, 2, 4, 8, 12, 16, 24]

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
        # Set a timeout (e.g. 5 seconds for 9x9 should be plenty)
        try:
            stdout, stderr = process.communicate(input=grid_str, timeout=5)
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

def main():
    # Clear log file
    with open(OUTPUT_FILE, "w") as f:
        f.write(f"Real Benchmark Run at {time.ctime()}\n")

    problem_dir = "/mnt/data/NYCU/pp/pp25_final/sodoku_constraint/problem"
    difficulties = ["easy", "medium", "hard"]
    
    # Structure: results[diff][solver][threads] = [time1, time2, ...]
    results = {}

    for diff in difficulties:
        results[diff] = {}
        files = glob.glob(os.path.join(problem_dir, diff, "*.txt"))
        files.sort()
        
        log(f"Running {diff} tests ({len(files)} files)...")

        for file_path in files:
            with open(file_path, 'r') as f:
                grid_str = f.read()
            
            for solver in SOLVERS:
                if solver not in results[diff]:
                    results[diff][solver] = {}

                is_serial = "serial" in solver or ("simd" in solver and "omp" not in solver)
                test_threads = [1] if is_serial else THREADS_TO_TEST
                
                for t in test_threads:
                    if t not in results[diff][solver]:
                        results[diff][solver][t] = []

                    time_ms = run_solver(solver, grid_str, t)
                    if time_ms >= 0:
                        results[diff][solver][t].append(time_ms)

    log("\n" + "="*80)
    log(f"{'Difficulty':<10} | {'Solver':<25} | {'Threads':<8} | {'Avg Time (ms)':<15}")
    log("="*80)

    for diff in difficulties:
        for solver in SOLVERS:
            is_serial = "serial" in solver or ("simd" in solver and "omp" not in solver)
            test_threads = [1] if is_serial else THREADS_TO_TEST
            
            for t in test_threads:
                times = results.get(diff, {}).get(solver, {}).get(t, [])
                solver_name = os.path.basename(solver)
                
                if times:
                    avg_time = sum(times) / len(times)
                    log(f"{diff:<10} | {solver_name:<25} | {t:<8} | {avg_time:.4f}")
                else:
                    log(f"{diff:<10} | {solver_name:<25} | {t:<8} | N/A")
        log("-" * 80)

if __name__ == "__main__":
    main()

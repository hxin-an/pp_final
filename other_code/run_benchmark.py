import subprocess
import time
import random
import os

OUTPUT_FILE = "benchmark_results.txt"


def log(message=""):
    print(message)
    with open(OUTPUT_FILE, "a") as f:
        f.write(str(message) + "\n")


# ======================
#  Solvers (Generic only)
# ======================

SOLVERS = [
    "build/generic_backtrack",
    "build/generic_bitset",
    "build/generic_csp",
    "build/generic_pruning",
    "build/generic_dlx",
]

# ======================
# Difficulty settings
# ======================

# 9×9 difficulties (# of removed cells)
DIFF_9 = {
    "Easy": 35,
    "Medium": 45,
    "Hard": 52,
    "Expert": 58
}

# 16×16 (256 cells)
DIFF_16 = {
    "Easy": 100,
    "Medium": 128,
    "Hard": 150,
    "Expert": 180
}

# 25×25 (625 cells)
DIFF_25 = {
    "Easy": 180,
    "Medium": 250,
    "Hard": 300,
    "Expert": 350
}

# ======================
# Sudoku generator
# ======================

def generate_sudoku(base, remove_count):
    side = base * base

    def pattern(r, c): return (base * (r % base) + r // base + c) % side
    from random import sample
    def shuffle(s): return sample(s, len(s))

    rBase = range(base)
    rows = [g * base + r for g in shuffle(rBase) for r in shuffle(rBase)]
    cols = [g * base + c for g in shuffle(rBase) for c in shuffle(rBase)]
    nums = shuffle(range(1, side + 1))

    board = [[nums[pattern(r, c)] for c in cols] for r in rows]

    squares = side * side
    for p in sample(range(squares), remove_count):
        board[p // side][p % side] = 0

    return board


# Convert grid to compact puzzle string
def grid_to_puzzle_string(grid):
    side = len(grid)
    chars = []
    for r in range(side):
        for c in range(side):
            v = grid[r][c]
            if v == 0:
                chars.append("0")
            elif v < 10:
                chars.append(str(v))
            else:
                # 10 -> A, 11 -> B, ...
                chars.append(chr(ord('A') + v - 10))
    return "".join(chars)


# ======================
# Solver execution
# ======================

def run_solver(exe_path, size, puzzle_str):
    if not os.path.exists(exe_path):
        return -1.0

    try:
        # Execute with argv: solver size puzzle_string
        process = subprocess.Popen(
            [exe_path, str(size), puzzle_str],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        try:
            stdout, stderr = process.communicate(timeout=20)
        except subprocess.TimeoutExpired:
            process.kill()
            return -2.0  # timeout

        # find "ms" output
        for line in stdout.splitlines():
            if "ms" in line:
                try:
                    return float(line.split()[0])
                except:
                    return 0.0

        return 0.0

    except Exception as e:
        log(f"Error running {exe_path}: {e}")
        return -1.0


# ======================
# Benchmark runner
# ======================

def run_benchmark(name, base, difficulties):
    side = base * base

    log("\n" + "=" * 28)
    log(f"   Benchmark {name}")
    log("=" * 28)
    log(f"{'Difficulty':<10} | {'Solver':<25} | {'Time (ms)':<10}\n")

    NUM_TRIALS = 5

    # results[difficulty][solver] = list of times
    results = {d: {s: [] for s in SOLVERS} for d in difficulties}

    for diff_name, remove_count in difficulties.items():
        log(f"Difficulty: {diff_name} ({remove_count} removals)")
        log("-" * 50)

        for trial in range(NUM_TRIALS):
            grid = generate_sudoku(base, remove_count)
            puzzle = grid_to_puzzle_string(grid)

            for solver in SOLVERS:
                t = run_solver(solver, side, puzzle)
                if t > 0:
                    results[diff_name][solver].append(t)

        # Print averages
        for solver in SOLVERS:
            solver_name = os.path.basename(solver)
            times = results[diff_name][solver]

            if len(times) == 0:
                log(f"{diff_name:<10} | {solver_name:<25} | Failed/Timeout")
            else:
                avg = sum(times) / len(times)
                log(f"{diff_name:<10} | {solver_name:<25} | {avg:.4f}")

        log("")


# ======================
# Main
# ======================

def main():
    with open(OUTPUT_FILE, "w") as f:
        f.write(f"Benchmark Run at {time.ctime()}\n")

    # 9×9
    run_benchmark("9x9", 3, DIFF_9)

    # 16×16
    run_benchmark("16x16", 4, DIFF_16)

    # 25×25
    run_benchmark("25x25", 5, DIFF_25)


if __name__ == "__main__":
    main()


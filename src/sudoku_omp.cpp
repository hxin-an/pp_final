#include <omp.h>
#include "sudoku_common.h"

#ifndef CUTOFF_DEPTH
#define CUTOFF_DEPTH 2
#endif

// Global flag to stop other threads when solution is found
bool global_solved = false;

struct SudokuState {
    int grid[N][N];
};

// Helper to copy grid to state
SudokuState make_state(int grid[N][N]) {
    SudokuState s;
    memcpy(s.grid, grid, sizeof(s.grid));
    return s;
}

bool solve_omp(SudokuState state, int depth) {
    if (global_solved) return true; // Early exit

    // Cutoff to serial for deeper levels to avoid excessive task creation overhead
    if (depth > CUTOFF_DEPTH) { 
        if (solve_serial(state.grid)) {
            #pragma omp atomic write
            global_solved = true;
            return true;
        }
        return false;
    }

    // We work on the local copy 'state' directly
    SudokuState backup = state; // Struct copy is clean

    if (!propagate(state.grid)) {
        return false;
    }

    int min_candidates = N + 1;
    int best_r = -1, best_c = -1;
    int best_mask = 0;

    bool solved = true;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (state.grid[i][j] == 0) {
                solved = false;
                int mask = get_candidates(state.grid, i, j);
                if (mask == 0) {
                    return false;
                }
                
                int count = 0;
                int temp = mask;
                while (temp) { temp &= (temp - 1); count++; }

                if (count < min_candidates) {
                    min_candidates = count;
                    best_r = i;
                    best_c = j;
                    best_mask = mask;
                }
            }
        }
    }

    if (solved) {
        #pragma omp atomic write
        global_solved = true;
        return true;
    }

    // Parallelize the branching
    bool found = false;
    
    // Collect all valid moves
    vector<int> moves;
    for (int val = 1; val <= N; val++) {
        if (best_mask & (1 << (val - 1))) {
            moves.push_back(val);
        }
    }

    // If only 1 move, no need to spawn task
    if (moves.size() == 1) {
        state.grid[best_r][best_c] = moves[0];
        if (solve_omp(state, depth + 1)) return true;
    } else {
        #pragma omp taskgroup
        {
            for (int val : moves) {
                if (global_solved) break;
                
                // Use firstprivate(state) to automatically copy the struct
                #pragma omp task firstprivate(state) shared(global_solved, found) priority(1)
                {
                    if (!global_solved) {
                        state.grid[best_r][best_c] = val;
                        if (solve_omp(state, depth + 1)) {
                            #pragma omp atomic write
                            global_solved = true;
                            
                            #pragma omp critical
                            {
                                found = true;
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (global_solved) return true; // Someone found it

    return false;
}

int main() {
    int grid[N][N];
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (!(cin >> grid[i][j])) return 0;
        }
    }

    auto start = chrono::high_resolution_clock::now();
    
    bool result = false;
    SudokuState initial_state = make_state(grid);

    #pragma omp parallel
    {
        #pragma omp single
        {
            result = solve_omp(initial_state, 0);
        }
    }

    if (global_solved) { // Use the flag as the truth
        auto end = chrono::high_resolution_clock::now();
        chrono::duration<double, std::milli> elapsed = end - start;
        cout << elapsed.count() << " ms" << endl;
    } else {
        cout << "No solution found." << endl;
    }

    return 0;
}

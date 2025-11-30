#include <omp.h>
#include "sudoku_simd.h"

#ifndef CUTOFF_DEPTH
#define CUTOFF_DEPTH 2
#endif

struct SudokuState {
    int grid[N][N];
};

bool global_solved = false;

// Helper to copy grid to state
SudokuState make_state(int grid[N][N]) {
    SudokuState s;
    memcpy(s.grid, grid, sizeof(s.grid));
    return s;
}

bool solve_simd_serial_abortable(int grid[N][N]) {
    if (global_solved) return true;

    int backup[N][N];
    memcpy(backup, grid, sizeof(backup));

    if (!propagate_simd(grid)) {
        memcpy(grid, backup, sizeof(backup));
        return false;
    }

    int min_candidates = N + 1;
    int best_r = -1, best_c = -1;
    int best_mask = 0;

    bool solved = true;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (grid[i][j] == 0) {
                solved = false;
                int mask = get_candidates_simd(grid, i, j);
                if (mask == 0) {
                    memcpy(grid, backup, sizeof(backup));
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

    if (solved) return true;

    for (int val = 1; val <= N; val++) {
        if (global_solved) return true;
        if (best_mask & (1 << (val - 1))) {
            grid[best_r][best_c] = val;
            if (solve_simd_serial_abortable(grid)) return true;
        }
    }

    memcpy(grid, backup, sizeof(backup)); 
    return false;
}

bool solve_omp_simd(SudokuState state, int depth) {
    if (global_solved) return true;

    if (depth > CUTOFF_DEPTH) { 
        if (solve_simd_serial_abortable(state.grid)) {  
            #pragma omp atomic write
            global_solved = true;
            return true;
        }
        return false;
    }

    // We work on the local copy 'state' directly
    SudokuState backup = state; // Struct copy is clean

    if (!propagate_simd(state.grid)) { 
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
                int mask = get_candidates_simd(state.grid, i, j); 
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

    bool found = false;
    vector<int> moves;
    for (int val = 1; val <= N; val++) {
        if (best_mask & (1 << (val - 1))) {
            moves.push_back(val);
        }
    }

    if (moves.size() == 1) {
        state.grid[best_r][best_c] = moves[0];
        if (solve_omp_simd(state, depth + 1)) return true;
    } else {
        #pragma omp taskgroup
        {
            for (int val : moves) {
                if (global_solved) break;
                
                // Use firstprivate(state) to automatically copy the struct
                // This is much cleaner and safer than manual memcpy
                #pragma omp task firstprivate(state) shared(global_solved, found) priority(1)
                {
                    if (!global_solved) {
                        state.grid[best_r][best_c] = val;
                        if (solve_omp_simd(state, depth + 1)) {
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
    
    return global_solved;
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
            if (omp_get_num_threads() == 1) {
                result = solve_simd_serial(initial_state.grid);
                global_solved = result;
            } else {
                result = solve_omp_simd(initial_state, 0);
            }
        }
    }

    if (global_solved) {
        auto end = chrono::high_resolution_clock::now();
        chrono::duration<double, std::milli> elapsed = end - start;
        cout << elapsed.count() << " ms" << endl;
    } else {
        cout << "No solution found." << endl;
    }

    return 0;
}

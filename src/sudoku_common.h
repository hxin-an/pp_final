#ifndef SUDOKU_COMMON_H
#define SUDOKU_COMMON_H

#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <chrono>

using namespace std;

#ifndef N
#define N 9
#endif

#ifndef SQRT_N
#define SQRT_N 3
#endif

// Helper to get possible values for a cell
inline int get_candidates(int grid[N][N], int r, int c) {
    int used = 0;
    for (int k = 0; k < N; k++) {
        if (grid[r][k] != 0) used |= (1 << (grid[r][k] - 1));
        if (grid[k][c] != 0) used |= (1 << (grid[k][c] - 1));
    }
    int br = (r / SQRT_N) * SQRT_N;
    int bc = (c / SQRT_N) * SQRT_N;
    for (int i = 0; i < SQRT_N; i++) {
        for (int j = 0; j < SQRT_N; j++) {
            int val = grid[br + i][bc + j];
            if (val != 0) used |= (1 << (val - 1));
        }
    }
    return used ^ ((1 << N) - 1);
}

// Propagate constraints: fill naked singles
inline bool propagate(int grid[N][N]) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                if (grid[i][j] == 0) {
                    int candidates = get_candidates(grid, i, j);
                    if (candidates == 0) return false;
                    
                    if ((candidates & (candidates - 1)) == 0) {
                        int val = 0;
                        while ((candidates & 1) == 0) {
                            candidates >>= 1;
                            val++;
                        }
                        grid[i][j] = val + 1;
                        changed = true;
                    }
                }
            }
        }
    }
    return true;
}

// Serial solve function (backtracking with MRV)
inline bool solve_serial(int grid[N][N]) {
    int backup[N][N];
    memcpy(backup, grid, sizeof(backup));

    if (!propagate(grid)) {
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
                int mask = get_candidates(grid, i, j);
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
        if (best_mask & (1 << (val - 1))) {
            grid[best_r][best_c] = val;
            if (solve_serial(grid)) return true;
        }
    }

    memcpy(grid, backup, sizeof(backup)); 
    return false;
}

#endif

#ifndef SUDOKU_SIMD_H
#define SUDOKU_SIMD_H

#include <immintrin.h>
#include "sudoku_common.h"

// --- SIMD Helpers Start ---
inline int h_or(__m256i v) {
    __m128i vlow = _mm256_castsi256_si128(v);
    __m128i vhigh = _mm256_extracti128_si256(v, 1);
    vlow = _mm_or_si128(vlow, vhigh);
    __m128i vshuf = _mm_shuffle_epi32(vlow, _MM_SHUFFLE(1, 0, 3, 2));
    vlow = _mm_or_si128(vlow, vshuf);
    vshuf = _mm_shuffle_epi32(vlow, _MM_SHUFFLE(2, 3, 0, 1));
    vlow = _mm_or_si128(vlow, vshuf);
    return _mm_cvtsi128_si32(vlow);
}

inline int get_candidates_simd(int grid[N][N], int r, int c) {
    int used = 0;
    __m256i v_used = _mm256_setzero_si256();
    __m256i v_ones = _mm256_set1_epi32(1);
    __m256i v_zero = _mm256_setzero_si256();

    int k = 0;
    for (; k <= N - 8; k += 8) {
        __m256i v_vals = _mm256_loadu_si256((__m256i*)&grid[r][k]);
        __m256i v_mask = _mm256_cmpgt_epi32(v_vals, v_zero);
        __m256i v_shifts = _mm256_sub_epi32(v_vals, v_ones);
        __m256i v_bits = _mm256_sllv_epi32(v_ones, v_shifts);
        v_bits = _mm256_and_si256(v_bits, v_mask);
        v_used = _mm256_or_si256(v_used, v_bits);
    }
    for (; k < N; k++) {
        if (grid[r][k] != 0) used |= (1 << (grid[r][k] - 1));
    }

    for (int k = 0; k < N; k++) {
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

    used |= h_or(v_used);
    return used ^ ((1 << N) - 1);
}

inline bool propagate_simd(int grid[N][N]) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                if (grid[i][j] == 0) {
                    int candidates = get_candidates_simd(grid, i, j);
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

inline bool solve_simd_serial(int grid[N][N]) {
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
        if (best_mask & (1 << (val - 1))) {
            grid[best_r][best_c] = val;
            if (solve_simd_serial(grid)) return true;
        }
    }

    memcpy(grid, backup, sizeof(backup)); 
    return false;
}
// --- SIMD Helpers End ---

#endif

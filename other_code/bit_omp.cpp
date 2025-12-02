#include <iostream>
#include <chrono>
#include <cstring>
#include <string>
#include <omp.h>
#include <vector>
#include <atomic>
#include <iomanip>
using namespace std;

// OpenMP Accelerated Generic Sudoku Solver
// Supports 4x4, 9x9, 16x16, 25x25

int SIZE;
int BLOCK_SIZE;
int* initial_grid;
int* final_grid;
atomic<bool> solved(false);

inline int getBox(int row, int col) {
    return (row / BLOCK_SIZE) * BLOCK_SIZE + (col / BLOCK_SIZE);
}

// Thread-local solver state
struct SolverState {
    int* grid;
    unsigned long long* rowMask;
    unsigned long long* colMask;
    unsigned long long* boxMask;
    long long backtracks;
    
    SolverState() {
        grid = new int[SIZE * SIZE];
        rowMask = new unsigned long long[SIZE];
        colMask = new unsigned long long[SIZE];
        boxMask = new unsigned long long[SIZE];
        backtracks = 0;
    }
    
    ~SolverState() {
        delete[] grid;
        delete[] rowMask;
        delete[] colMask;
        delete[] boxMask;
    }
    
    void init(int* input_grid) {
        memcpy(grid, input_grid, SIZE * SIZE * sizeof(int));
        for (int i = 0; i < SIZE; i++) {
            rowMask[i] = 0;
            colMask[i] = 0;
            boxMask[i] = 0;
        }
        
        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                if (grid[i * SIZE + j] != 0) {
                    unsigned long long bit = 1ULL << grid[i * SIZE + j];
                    rowMask[i] |= bit;
                    colMask[j] |= bit;
                    boxMask[getBox(i, j)] |= bit;
                }
            }
        }
    }
};

bool solve_recursive(SolverState& state) {
    if (solved.load(memory_order_relaxed)) return true;
    
    int row = -1, col = -1;
    int minCount = SIZE + 1;
    
    // MRV heuristic
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (state.grid[i * SIZE + j] == 0) {
                unsigned long long used = state.rowMask[i] | state.colMask[j] | state.boxMask[getBox(i, j)];
                unsigned long long available = ((1ULL << (SIZE + 1)) - 2) & ~used;
                
                int count = __builtin_popcountll(available);
                
                if (count == 0) return false;
                
                if (count < minCount) {
                    minCount = count;
                    row = i;
                    col = j;
                }
            }
        }
    }
    
    if (row == -1) {
        // Found solution!
        if (!solved.exchange(true)) {
            memcpy(final_grid, state.grid, SIZE * SIZE * sizeof(int));
        }
        return true;
    }
    
    unsigned long long used = state.rowMask[row] | state.colMask[col] | state.boxMask[getBox(row, col)];
    unsigned long long available = ((1ULL << (SIZE + 1)) - 2) & ~used;
    
    int box = getBox(row, col);
    
    while (available) {
        if (solved.load(memory_order_relaxed)) return true;
        
        unsigned long long bit = available & -available;
        available ^= bit;
        
        int num = __builtin_ctzll(bit);
        
        state.grid[row * SIZE + col] = num;
        state.rowMask[row] |= bit;
        state.colMask[col] |= bit;
        state.boxMask[box] |= bit;
        
        if (solve_recursive(state)) return true;
        
        state.grid[row * SIZE + col] = 0;
        state.rowMask[row] ^= bit;
        state.colMask[col] ^= bit;
        state.boxMask[box] ^= bit;
        state.backtracks++;
    }
    
    return false;
}

// Parallel entry point
void solve_parallel() {
    // Find the first empty cell with MRV to branch on
    // We need to find a cell with enough candidates to distribute work
    
    // 1. Initialize a temporary state to find branching point
    SolverState tempState;
    tempState.init(initial_grid);
    
    int row = -1, col = -1;
    int minCount = SIZE + 1;
    
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (tempState.grid[i * SIZE + j] == 0) {
                unsigned long long used = tempState.rowMask[i] | tempState.colMask[j] | tempState.boxMask[getBox(i, j)];
                unsigned long long available = ((1ULL << (SIZE + 1)) - 2) & ~used;
                int count = __builtin_popcountll(available);
                
                if (count == 0) return; // Impossible
                
                if (count < minCount) {
                    minCount = count;
                    row = i;
                    col = j;
                }
            }
        }
    }
    
    if (row == -1) {
        // Already solved?
        solved = true;
        memcpy(final_grid, initial_grid, SIZE * SIZE * sizeof(int));
        return;
    }
    
    // Get candidates for the branching cell
    unsigned long long used = tempState.rowMask[row] | tempState.colMask[col] | tempState.boxMask[getBox(row, col)];
    unsigned long long available = ((1ULL << (SIZE + 1)) - 2) & ~used;
    
    vector<int> candidates;
    while (available) {
        unsigned long long bit = available & -available;
        available ^= bit;
        candidates.push_back(__builtin_ctzll(bit));
    }
    
    // Parallelize the first level of recursion
    #pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < candidates.size(); i++) {
        if (solved.load(memory_order_relaxed)) continue;
        
        int num = candidates[i];
        
        // Create thread-local state
        SolverState localState;
        localState.init(initial_grid);
        
        // Apply the move
        unsigned long long bit = 1ULL << num;
        int box = getBox(row, col);
        
        localState.grid[row * SIZE + col] = num;
        localState.rowMask[row] |= bit;
        localState.colMask[col] |= bit;
        localState.boxMask[box] |= bit;
        
        // Continue search
        solve_recursive(localState);
    }
}

int charToNum(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return 0;
}

void printGrid(int* g) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            int val = g[i * SIZE + j];
            if (SIZE <= 9) {
                cout << val << " ";
            } else if (val == 0) {
                cout << "0 ";
            } else if (val < 10) {
                cout << val << " ";
            } else {
                cout << (char)('A' + val - 10) << " ";
            }
        }
        cout << endl;
    }
}

int main(int argc, char* argv[]) {


    auto start = chrono::high_resolution_clock::now();

    solve_parallel();

    auto end = chrono::high_resolution_clock::now();
    double elapsed_ms = chrono::duration<double, milli>(end - start).count();

    if (solved) {
        cout << fixed << setprecision(4) << elapsed_ms << " ms" << endl; 
    } else {
        cout << "0.0000 ms" << endl;
    }

    // -------------------------------------------------------------------
    
    delete[] initial_grid;
    delete[] final_grid;

    return 0;
}

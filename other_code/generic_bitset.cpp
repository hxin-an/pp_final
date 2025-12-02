#include <iostream>
#include <chrono>
#include <cstring>
#include <string>
#include <cmath>
using namespace std;

// Generic Sudoku solver using bit manipulation
// Supports 4x4, 9x9, 16x16, 25x25

int SIZE;
int BLOCK_SIZE;
int* grid;
unsigned long long* rowMask;
unsigned long long* colMask;
unsigned long long* boxMask;
long long backtracks = 0;

inline int getBox(int row, int col) {
    return (row / BLOCK_SIZE) * BLOCK_SIZE + (col / BLOCK_SIZE);
}

void initMasks() {
    for (int i = 0; i < SIZE; i++) {
        rowMask[i] = 0;
        colMask[i] = 0;
        boxMask[i] = 0;
    }
    
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            int v = grid[i * SIZE + j];
            if (v != 0) {
                unsigned long long bit = 1ULL << v;
                rowMask[i] |= bit;
                colMask[j] |= bit;
                boxMask[getBox(i, j)] |= bit;
            }
        }
    }
}

bool solve() {
    int row = -1, col = -1;
    int minCount = SIZE + 1;
    
    // Find MRV cell
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (grid[i * SIZE + j] == 0) {
                unsigned long long used = rowMask[i] | colMask[j] | boxMask[getBox(i, j)];
                unsigned long long available =
                    ((1ULL << (SIZE + 1)) - 2) & ~used;

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

    if (row == -1) return true;

    unsigned long long used = rowMask[row] | colMask[col] | boxMask[getBox(row, col)];
    unsigned long long available = ((1ULL << (SIZE + 1)) - 2) & ~used;
    int box = getBox(row, col);

    while (available) {
        unsigned long long bit = available & -available;
        available ^= bit;

        int num = __builtin_ctzll(bit);

        grid[row * SIZE + col] = num;
        rowMask[row] |= bit;
        colMask[col] |= bit;
        boxMask[box] |= bit;

        if (solve()) return true;

        grid[row * SIZE + col] = 0;
        rowMask[row] ^= bit;
        colMask[col] ^= bit;
        boxMask[box] ^= bit;
        backtracks++;
    }

    return false;
}

int charToNum(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        return 1;
    }

    SIZE = atoi(argv[1]);
    string puzzle = argv[2];

    // Determine block size
    double r = sqrt(SIZE);
    BLOCK_SIZE = round(r);
    if (BLOCK_SIZE * BLOCK_SIZE != SIZE) {
        return 1;
    }

    if (puzzle.length() != (size_t)(SIZE * SIZE)) {
        return 1;
    }

    // Allocate memory
    grid = new int[SIZE * SIZE];
    rowMask = new unsigned long long[SIZE];
    colMask = new unsigned long long[SIZE];
    boxMask = new unsigned long long[SIZE];

    for (int i = 0; i < SIZE * SIZE; i++) {
        grid[i] = charToNum(puzzle[i]);
    }

    auto start = chrono::high_resolution_clock::now();

    initMasks();
    bool solved = solve();

    auto end = chrono::high_resolution_clock::now();
    double ms = chrono::duration<double, milli>(end - start).count();

    // Only output "<time> ms"
    cout << ms << " ms\n";

    delete[] grid;
    delete[] rowMask;
    delete[] colMask;
    delete[] boxMask;

    return 0;
}

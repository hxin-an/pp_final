#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <iomanip>

using namespace std;

// --- 全域參數 (Global Parameters) ---
int SIZE;
int BLOCK_SIZE;
int* initial_grid = nullptr;
int* final_grid = nullptr;
atomic<bool> solved(false);
mutex final_grid_mutex; // 用於保護寫入 final_grid

// --- 輔助函數 (Utility Functions) ---

inline int getBox(int row, int col) {
    return (row / BLOCK_SIZE) * BLOCK_SIZE + (col / BLOCK_SIZE);
}

int charToNum(char c) {
    if (c >= '0' && c <= '9') return c - '0';          // '0' -> 0 (空), '1'..'9' -> 1..9
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;     // 'A' -> 10 ...
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return 0;
}

// 可以留著 debug 用，但 benchmark 不會呼叫
void printGrid(const int* g) {
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

// --- 求解器狀態 (Solver State) ---
struct SolverState {
    // 使用最大 25x25 的固定陣列，實際只用到 SIZE*SIZE 部分
    int grid[25 * 25];
    unsigned long long rowMask[25];
    unsigned long long colMask[25];
    unsigned long long boxMask[25];

    void init(const int* input_grid) {
        memcpy(grid, input_grid, SIZE * SIZE * sizeof(int));
        memset(rowMask, 0, SIZE * sizeof(unsigned long long));
        memset(colMask, 0, SIZE * sizeof(unsigned long long));
        memset(boxMask, 0, SIZE * sizeof(unsigned long long));

        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                int v = grid[i * SIZE + j];
                if (v != 0) {
                    unsigned long long bit = 1ULL << v;   // bit 1..SIZE 使用
                    rowMask[i] |= bit;
                    colMask[j] |= bit;
                    boxMask[getBox(i, j)] |= bit;
                }
            }
        }
    }
};

// --- 核心回溯函數 (Core Backtracking Function) ---
bool solve_recursive(SolverState& state) {
    // 搶先式終止檢查
    if (solved.load(memory_order_relaxed)) return true;

    int row = -1, col = -1;
    int minCount = SIZE + 1;

    // 1. MRV 啟發式尋找最佳空單元格
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (state.grid[i * SIZE + j] == 0) {
                unsigned long long used = state.rowMask[i] |
                                          state.colMask[j] |
                                          state.boxMask[getBox(i, j)];
                // bits 1..SIZE 有效 (bit 0 保留不用)
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
        // 找到解決方案
        if (!solved.exchange(true)) {
            lock_guard<mutex> lock(final_grid_mutex);
            memcpy(final_grid, state.grid, SIZE * SIZE * sizeof(int));
        }
        return true;
    }

    // 2. 嘗試所有候選值
    unsigned long long used = state.rowMask[row] |
                              state.colMask[col] |
                              state.boxMask[getBox(row, col)];
    unsigned long long available = ((1ULL << (SIZE + 1)) - 2) & ~used;
    int box = getBox(row, col);

    while (available) {
        if (solved.load(memory_order_relaxed)) return true;

        unsigned long long bit = available & -available;
        available ^= bit;
        int num = __builtin_ctzll(bit);

        // 嘗試填入 num
        state.grid[row * SIZE + col] = num;
        state.rowMask[row] |= bit;
        state.colMask[col] |= bit;
        state.boxMask[box] |= bit;

        if (solve_recursive(state)) return true;

        // 回溯
        state.grid[row * SIZE + col] = 0;
        state.rowMask[row] ^= bit;
        state.colMask[col] ^= bit;
        state.boxMask[box] ^= bit;
    }

    return false;
}

// --- 執行緒入口點 (Thread Entry Point) ---
void thread_entry(SolverState localState) {
    solve_recursive(localState);
}

// --- 平行入口點 (Parallel Entry Point) ---
void solve_parallel_pthreads() {
    SolverState tempState;
    tempState.init(initial_grid);

    int row = -1, col = -1;
    int minCount = SIZE + 1;

    // 使用 MRV 找到分支點
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (tempState.grid[i * SIZE + j] == 0) {
                unsigned long long used = tempState.rowMask[i] |
                                          tempState.colMask[j] |
                                          tempState.boxMask[getBox(i, j)];
                unsigned long long available = ((1ULL << (SIZE + 1)) - 2) & ~used;
                int count = __builtin_popcountll(available);

                if (count == 0) return; // 無解

                if (count < minCount) {
                    minCount = count;
                    row = i;
                    col = j;
                }
            }
        }
    }

    if (row == -1) {
        // 已經是完整解
        solved.store(true);
        memcpy(final_grid, initial_grid, SIZE * SIZE * sizeof(int));
        return;
    }

    // 取得分支點候選
    unsigned long long used = tempState.rowMask[row] |
                              tempState.colMask[col] |
                              tempState.boxMask[getBox(row, col)];
    unsigned long long available = ((1ULL << (SIZE + 1)) - 2) & ~used;

    vector<thread> threads;
    int box = getBox(row, col);

    while (available) {
        unsigned long long bit = available & -available;
        available ^= bit;
        int num = __builtin_ctzll(bit);

        // 以 tempState 為基礎複製一份 localState
        SolverState localState = tempState;

        localState.grid[row * SIZE + col] = num;
        localState.rowMask[row] |= bit;
        localState.colMask[col] |= bit;
        localState.boxMask[box] |= bit;

        threads.emplace_back(thread_entry, localState);
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}

// --- Main 函數 ---
// 介面： ./sudoku_pthread SIZE PUZZLE_STRING
// 成功： <time> ms
// 失敗或輸入錯誤： 0.0000 ms
int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc < 3) {
        // benchmark 用：不要丟非 0 code，印出 0.0000 ms 即可
        cerr << "Usage: " << argv[0] << " <size> <puzzle>\n";
        cout << "0.0000 ms" << endl;
        return 0;
    }

    SIZE = atoi(argv[1]);
    string puzzle = argv[2];

    if (SIZE == 4) BLOCK_SIZE = 2;
    else if (SIZE == 9) BLOCK_SIZE = 3;
    else if (SIZE == 16) BLOCK_SIZE = 4;
    else if (SIZE == 25) BLOCK_SIZE = 5;
    else {
        cerr << "Unsupported size: " << SIZE << endl;
        cout << "0.0000 ms" << endl;
        return 0;
    }

    if ((int)puzzle.length() != SIZE * SIZE) {
        cerr << "Error: Puzzle length (" << puzzle.length()
             << ") does not match size^2 (" << SIZE * SIZE << ").\n";
        cout << "0.0000 ms" << endl;
        return 0;
    }

    initial_grid = new int[SIZE * SIZE];
    final_grid = new int[SIZE * SIZE];

    for (int i = 0; i < SIZE * SIZE; i++) {
        int v = charToNum(puzzle[i]);
        if (v < 0 || v > SIZE) v = 0;   // 安全處理
        initial_grid[i] = v;
        final_grid[i] = 0;
    }

    solved.store(false);

    auto start = chrono::high_resolution_clock::now();

    solve_parallel_pthreads();

    auto end = chrono::high_resolution_clock::now();
    double elapsed_ms = chrono::duration<double, milli>(end - start).count();

    if (solved.load()) {
        cout << fixed << setprecision(4) << elapsed_ms << " ms" << endl;
        // 如要 debug，你可以暫時打開：
        // printGrid(final_grid);
    } else {
        cout << "0.0000 ms" << endl;
    }

    delete[] initial_grid;
    delete[] final_grid;

    return 0;
}


#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>
#include <mpi.h>
#include <iomanip>

using namespace std;

// --- 全域參數 ---
int SIZE;
int BLOCK_SIZE;

// MPI 訊息標籤
#define TAG_TASK      1   // Master -> Worker: 傳一個起始狀態 (grid)
#define TAG_SOLUTION  2   // Worker -> Master: 回傳解
#define TAG_DONE      3   // Worker -> Master: 該 task 無解，要求下一個
#define TAG_TERMINATE 4   // Master -> Worker: 結束

// --- 輔助函數 ---
inline int getBox(int row, int col) {
    return (row / BLOCK_SIZE) * BLOCK_SIZE + (col / BLOCK_SIZE);
}

int charToNum(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return 0;
}

// --- 求解器狀態 ---
struct SolverState {
    int grid[25 * 25];
    unsigned long long rowMask[25];
    unsigned long long colMask[25];
    unsigned long long boxMask[25];

    void init(const int* input_grid) {
        memcpy(grid, input_grid, SIZE * SIZE * sizeof(int));
        memset(rowMask, 0, sizeof(rowMask));
        memset(colMask, 0, sizeof(colMask));
        memset(boxMask, 0, sizeof(boxMask));

        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                int v = grid[i * SIZE + j];
                if (v != 0) {
                    unsigned long long bit = 1ULL << v;   // bit 1..SIZE
                    rowMask[i] |= bit;
                    colMask[j] |= bit;
                    boxMask[getBox(i, j)] |= bit;
                }
            }
        }
    }
};

// --- 回溯 ---
bool solve_recursive(SolverState& state) {
    int row = -1, col = -1;
    int minCount = SIZE + 1;

    // MRV：選一個候選數最少的空格
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (state.grid[i * SIZE + j] == 0) {
                unsigned long long used =
                    state.rowMask[i] | state.colMask[j] | state.boxMask[getBox(i, j)];

                unsigned long long available =
                    ((1ULL << (SIZE + 1)) - 2) & ~used;     // bits 1..SIZE

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

    // 沒空格了 → solved
    if (row == -1) return true;

    unsigned long long used =
        state.rowMask[row] | state.colMask[col] | state.boxMask[getBox(row, col)];
    unsigned long long available =
        ((1ULL << (SIZE + 1)) - 2) & ~used;

    int box = getBox(row, col);

    while (available) {
        unsigned long long bit = available & -available;
        available ^= bit;

        int num = __builtin_ctzll(bit);

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

// --- Master：負責切第一層分支 + 分配 tasks 給 workers ---
bool master_process(int num_workers, const int* initial_grid, int* final_solution) {
    SolverState temp;
    temp.init(initial_grid);

    // 找一個空格（這裡可以用 MRV，也可以直接找第一個）
    int row = -1, col = -1;
    for (int i = 0; i < SIZE * SIZE; i++) {
        if (temp.grid[i] == 0) {
            row = i / SIZE;
            col = i % SIZE;
            break;
        }
    }

    // 沒有空格 → 已經 solved
    if (row == -1) {
        memcpy(final_solution, initial_grid, SIZE * SIZE * sizeof(int));
        return true;
    }

    // 該格的候選數字
    unsigned long long used =
        temp.rowMask[row] | temp.colMask[col] | temp.boxMask[getBox(row, col)];
    unsigned long long available =
        ((1ULL << (SIZE + 1)) - 2) & ~used;

    vector<SolverState> tasks;
    while (available) {
        unsigned long long bit = available & -available;
        available ^= bit;
        int num = __builtin_ctzll(bit);

        SolverState s = temp;
        int box = getBox(row, col);

        s.grid[row * SIZE + col] = num;
        s.rowMask[row] |= bit;
        s.colMask[col] |= bit;
        s.boxMask[box] |= bit;

        tasks.push_back(s);
    }

    int total_tasks = (int)tasks.size();
    if (total_tasks == 0) return false;

    int next_task = 0;
    int active_workers = 0;

    // 初始派發：每個 worker 先拿一個 task 或直接 TERMINATE
    for (int rank = 1; rank <= num_workers; rank++) {
        if (next_task < total_tasks) {
            MPI_Send(tasks[next_task].grid, SIZE * SIZE, MPI_INT, rank, TAG_TASK, MPI_COMM_WORLD);
            next_task++;
            active_workers++;
        } else {
            MPI_Send(nullptr, 0, MPI_INT, rank, TAG_TERMINATE, MPI_COMM_WORLD);
        }
    }

    int recv_grid[25 * 25];
    MPI_Status status;
    bool solved_flag = false;
    int solver_rank = -1;

    while (active_workers > 0 && !solved_flag) {
        // 等待任何 worker 的消息
        MPI_Recv(recv_grid, SIZE * SIZE, MPI_INT, MPI_ANY_SOURCE,
                 MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        int src = status.MPI_SOURCE;

        if (status.MPI_TAG == TAG_SOLUTION) {
            // 有人找到解
            memcpy(final_solution, recv_grid, SIZE * SIZE * sizeof(int));
            solved_flag = true;
            solver_rank = src;
            break;
        }

        if (status.MPI_TAG == TAG_DONE) {
            // 該 worker 完成了一個 task，看看有沒有新的 task
            active_workers--;

            if (next_task < total_tasks) {
                MPI_Send(tasks[next_task].grid, SIZE * SIZE, MPI_INT,
                         src, TAG_TASK, MPI_COMM_WORLD);
                next_task++;
                active_workers++;
            } else {
                MPI_Send(nullptr, 0, MPI_INT, src, TAG_TERMINATE, MPI_COMM_WORLD);
            }
        }
    }

    // 找到解 → 通知所有 worker 結束
    if (solved_flag) {
        for (int rank = 1; rank <= num_workers; rank++) {
            if (rank == solver_rank) continue;  // 已停止
            MPI_Send(nullptr, 0, MPI_INT, rank, TAG_TERMINATE, MPI_COMM_WORLD);
        }
        return true;
    }

    // 沒有任何一個分支找到解
    return false;
}

// --- Worker：不停接 Task，做完就回報 DONE，若有解就回 SOLUTION ---
void worker_process() {
    int grid_buf[25 * 25];
    MPI_Status status;

    while (true) {
        MPI_Recv(grid_buf, SIZE * SIZE, MPI_INT, 0,
                 MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == TAG_TERMINATE) {
            break;
        }

        if (status.MPI_TAG == TAG_TASK) {
            SolverState s;
            s.init(grid_buf);

            if (solve_recursive(s)) {
                // 找到解，送回去
                MPI_Send(s.grid, SIZE * SIZE, MPI_INT, 0, TAG_SOLUTION, MPI_COMM_WORLD);
                // 找到解就直接退出，master 會終止其他 worker
                break;
            } else {
                int dummy = 0;
                MPI_Send(&dummy, 1, MPI_INT, 0, TAG_DONE, MPI_COMM_WORLD);
            }
        }
    }
}

// --- Main ---
// 介面： mpirun -np N ./sudoku_mpi SIZE PUZZLE_STRING
// rank 0：成功 → 印 "<time> ms"，失敗 → "0.0000 ms"
// 其他 rank：不印任何東西
int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if (argc < 3) {
        if (rank == 0) {
            cout << "0.0000 ms" << endl;
        }
        MPI_Finalize();
        return 0;
    }

    SIZE = atoi(argv[1]);
    if (SIZE == 4) BLOCK_SIZE = 2;
    else if (SIZE == 9) BLOCK_SIZE = 3;
    else if (SIZE == 16) BLOCK_SIZE = 4;
    else if (SIZE == 25) BLOCK_SIZE = 5;
    else {
        if (rank == 0) cout << "0.0000 ms" << endl;
        MPI_Finalize();
        return 0;
    }

    string puzzle = argv[2];
    if ((int)puzzle.size() != SIZE * SIZE) {
        if (rank == 0) cout << "0.0000 ms" << endl;
        MPI_Finalize();
        return 0;
    }

    // worker 只需要 SIZE / BLOCK_SIZE；不需要 puzzle
    if (rank != 0) {
        worker_process();
        MPI_Finalize();
        return 0;
    }

    // rank 0：解析 puzzle，呼叫 master
    int* initial_grid = new int[SIZE * SIZE];
    for (int i = 0; i < SIZE * SIZE; i++) {
        int v = charToNum(puzzle[i]);
        if (v < 0 || v > SIZE) v = 0;
        initial_grid[i] = v;
    }

    int final_solution[25 * 25];

    auto start = chrono::high_resolution_clock::now();
    bool ok = master_process(nprocs - 1, initial_grid, final_solution);
    auto end = chrono::high_resolution_clock::now();

    if (ok) {
        double elapsed_ms = chrono::duration<double, milli>(end - start).count();
        cout << fixed << setprecision(4) << elapsed_ms << " ms" << endl;
    } else {
        cout << "0.0000 ms" << endl;
    }

    delete[] initial_grid;

    MPI_Finalize();
    return 0;
}


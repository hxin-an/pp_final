#include "sudoku_common.h"

int main() {
    int grid[N][N];
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (!(cin >> grid[i][j])) return 0;
        }
    }

    auto start = chrono::high_resolution_clock::now();
    if (solve_serial(grid)) {
        auto end = chrono::high_resolution_clock::now();
        chrono::duration<double, std::milli> elapsed = end - start;
        cout << elapsed.count() << " ms" << endl;
    } else {
        cout << "No solution found." << endl;
    }

    return 0;
}

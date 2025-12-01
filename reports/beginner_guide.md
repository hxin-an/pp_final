# 數獨解題程式新手指南 (Sudoku Solver Beginner's Guide)

這份指南將帶你一步步了解這個數獨解題專案的程式碼。我們會從最基礎的部分開始，解釋每個檔案的功能，並附上詳細的程式碼註解。

## 0. 專案總覽與演算法核心 (High-Level Overview)

在深入程式碼之前，我們先來了解這個專案到底在做什麼，以及它是如何辦到的。

### 核心目標
這個專案的目標是寫出一個 **超快速** 的數獨解題程式。為了達到這個目標，我們不只是單純的暴力破解，而是結合了多種演算法和電腦底層的優化技術。

### 演算法核心邏輯 (Algorithm Summary)

我們使用的解題策略可以分為三個層次，為了讓你更好理解，我們用「**走迷宮**」來比喻：

#### 1. 約束傳播 (Constraint Propagation) - "把死路封起來"
*   **情境**：你站在迷宮的一個路口，發現有 3 條路。但仔細一看，其中 2 條路已經明顯被石頭堵住了（違反數獨規則）。
*   **行動**：既然只剩 1 條路能走，那就**不用猶豫，直接走過去**！
*   **程式碼對應**：`propagate` 函式。它會把所有「只剩一個選擇」的格子填滿。這通常會引發連鎖反應，讓你一口氣前進好幾步。

#### 2. 啟發式搜尋 (Heuristics) - "挑最簡單的路口猜"
*   **情境**：現在你來到一個新的路口，這次沒有死路，有 3 條路看起來都可以走。同時，遠方還有另一個路口有 5 條路可以走。
*   **行動**：如果要猜，當然先猜**只有 2 條路**的那個路口！因為猜對的機率是 50%，比猜 5 條路（機率 20%）容易多了。而且就算猜錯，也只要退回來試另一條就好，代價比較小。
*   **程式碼對應**：**MRV (Minimum Remaining Values)** 策略。程式會掃描整個盤面，找出「候選數最少」的那個格子來開始嘗試。

#### 3. 回溯法 (Backtracking) - "走錯了就回頭"
*   **情境**：你選了一條路走進去，結果走到底發現是死胡同（無解）。
*   **行動**：沒關係，**拿出你的「存檔」，回到上一個路口**，這次改走另一條路試試看。
*   **程式碼對應**：`solve` 函式的遞迴呼叫。每次要猜之前，我們先用 `memcpy` 備份盤面（存檔）。如果發現走不通（回傳 `false`），就用備份還原盤面（讀檔），然後試下一個數字。

---

**總結來說，程式的運作流程是這樣的：**
1.  先用 **約束傳播** 把所有確定的格子填滿。
2.  如果還有空格，就用 **啟發式搜尋** 挑一個最軟的柿子（選擇最少的格子）。
3.  用 **回溯法** 試著填入一個數字，然後重複步驟 1。
4.  如果發現無解，就**回頭**換個數字再試。

### 優化技術 (Optimization Techniques)

為了讓程式跑得更快，我們用了以下黑科技：

*   **位元遮罩 (Bitmask)**：用一個整數 (例如 `00101`) 代表候選數集合，讓檢查「能不能填」變成超快的位元運算 (`&`, `|`)。
*   **SIMD (單指令多資料流)**：一次處理 8 個格子，而不是一個一個處理。這利用了 CPU 的 AVX2 指令集。
*   **OpenMP (多執行緒)**：叫電腦裡所有的 CPU 核心一起來幫忙猜。當遇到分岔路口時，派不同的核心去試不同的路。

---

## 專案結構簡介

這個專案包含了多種不同版本的數獨解題程式，目的是為了比較不同優化技術（如平行運算）的效果。

- **基礎版**: `sudoku_serial.cpp` (單執行緒，最簡單的版本)
- **通用工具**: `sudoku_common.h` (大家共用的函式)
- **SIMD 優化版**: `sudoku_simd.cpp` (使用向量指令加速)
- **OpenMP 平行版**: `sudoku_omp.cpp` (使用多核心平行運算)
- **終極優化版**: `sudoku_omp_simd.cpp` (結合 SIMD 和 OpenMP)

---

## 1. 基礎核心：`src/sudoku_common.h`

這個檔案定義了所有解題程式都會用到的基本功能，比如「檢查某個格子能填哪些數字」。

### 關鍵概念：位元遮罩 (Bitmask)
我們不使用 `[1, 2, 3]` 這樣的陣列來儲存候選數字，而是用一個整數的「位元」來表示。
- 例如：二進位的 `000000101` (十進位 5)，表示數字 1 和 3 是可能的（第 0 位和第 2 位是 1）。
- 好處：運算非常快，檢查「有沒有重複」只需要做位元運算 (`&`, `|`)。

### 程式碼詳解

```cpp
#ifndef SUDOKU_COMMON_H
#define SUDOKU_COMMON_H

#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring> // 用於 memcpy
#include <chrono>  // 用於計時

using namespace std;

// 定義數獨的大小，預設是 9x9
#ifndef N
#define N 9
#endif

// 定義宮格的大小，9x9 的數獨，宮格是 3x3
#ifndef SQRT_N
#define SQRT_N 3
#endif

// ---------------------------------------------------------
// 函式：get_candidates
// 目的：找出 (r, c) 這個位置可以填入哪些數字
// 回傳：一個整數 (mask)，如果第 k 個 bit 是 1，表示數字 k+1 可以填
// ---------------------------------------------------------
inline int get_candidates(int grid[N][N], int r, int c) {
    int used = 0; // 用來記錄已經被用掉的數字

    // 1. 檢查同一列 (Row) 和同一行 (Column)
    for (int k = 0; k < N; k++) {
        // 如果同一列有數字，將該數字對應的 bit 設為 1
        // (1 << (val - 1)) 是把 1 向左移 val-1 位
        if (grid[r][k] != 0) used |= (1 << (grid[r][k] - 1));
        
        // 如果同一行有數字，也標記起來
        if (grid[k][c] != 0) used |= (1 << (grid[k][c] - 1));
    }

    // 2. 檢查所在的 3x3 宮格 (Block)
    // 計算宮格左上角的座標
    int br = (r / SQRT_N) * SQRT_N;
    int bc = (c / SQRT_N) * SQRT_N;
    
    for (int i = 0; i < SQRT_N; i++) {
        for (int j = 0; j < SQRT_N; j++) {
            int val = grid[br + i][bc + j];
            if (val != 0) used |= (1 << (val - 1));
        }
    }

    // `used` 紀錄了所有「不能用」的數字
    // 我們要回傳「可以用」的數字，所以做反相 (NOT) 運算
    // ((1 << N) - 1) 產生 N 個 1 的遮罩 (例如 9 個 1)，確保不超過範圍
    return used ^ ((1 << N) - 1);
}

// ---------------------------------------------------------
// 函式：propagate (約束傳播)
// 目的：填入那些「只有一個選擇」的格子 (Naked Singles)
// 原理：如果某個格子只能填 '5'，那就直接填上去，不用猜。
//      填上去後可能會讓其他格子也變成只剩一個選擇，所以要重複做 (while loop)。
// ---------------------------------------------------------
inline bool propagate(int grid[N][N]) {
    bool changed = true;
    while (changed) { // 如果上一輪有填入數字，就繼續檢查
        changed = false;
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                if (grid[i][j] == 0) { // 只看空格
                    int candidates = get_candidates(grid, i, j);
                    
                    if (candidates == 0) return false; // 無解：有空格但沒數字可填
                    
                    // 檢查是否只有一個 bit 是 1 (Power of 2 check)
                    // (x & (x - 1)) == 0 表示 x 是 2 的次方，也就是只有一個 bit 是 1
                    if ((candidates & (candidates - 1)) == 0) {
                        int val = 0;
                        // 找出那個 bit 是第幾位
                        while ((candidates & 1) == 0) {
                            candidates >>= 1;
                            val++;
                        }
                        grid[i][j] = val + 1; // 填入數字
                        changed = true; // 標記有變動，需要再跑一輪
                    }
                }
            }
        }
    }
    return true; // 傳播成功，沒有發現矛盾
}

// ---------------------------------------------------------
// 函式：solve_serial (序列版解題)
// 目的：使用「回溯法 (Backtracking)」解數獨
// ---------------------------------------------------------
inline bool solve_serial(int grid[N][N]) {
    // 1. 備份目前的盤面
    // 因為如果猜錯了，我們需要還原 (Backtrack)
    int backup[N][N];
    memcpy(backup, grid, sizeof(backup));

    // 2. 先做約束傳播，填滿那些顯而易見的格子
    if (!propagate(grid)) {
        memcpy(grid, backup, sizeof(backup)); // 失敗了，還原
        return false;
    }

    // 3. 尋找「候選數最少」的格子 (MRV 啟發式演算法)
    // 我們不想隨便找個格子猜，而是找那個「最容易猜中」的 (選擇最少的)
    int min_candidates = N + 1;
    int best_r = -1, best_c = -1;
    int best_mask = 0;

    bool solved = true; // 假設已經解完
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (grid[i][j] == 0) {
                solved = false; // 還有空格，沒解完
                int mask = get_candidates(grid, i, j);
                if (mask == 0) { // 死路：有空格但沒數字可填
                    memcpy(grid, backup, sizeof(backup)); // 還原
                    return false;
                }
                
                // 計算有幾個候選數 (計算 mask 裡有幾個 1)
                int count = 0;
                int temp = mask;
                while (temp) { temp &= (temp - 1); count++; }

                // 更新最佳選擇
                if (count < min_candidates) {
                    min_candidates = count;
                    best_r = i;
                    best_c = j;
                    best_mask = mask;
                }
            }
        }
    }

    if (solved) return true; // 如果沒有空格，表示解完了！

    // 4. 開始嘗試填入數字 (猜測)
    for (int val = 1; val <= N; val++) {
        // 檢查 val 是否在候選名單中
        if (best_mask & (1 << (val - 1))) {
            grid[best_r][best_c] = val; // 試著填入 val
            
            // 遞迴呼叫：繼續解剩下的格子
            if (solve_serial(grid)) return true; // 如果成功，就回傳 true
        }
    }

    // 5. 如果所有數字都試過了都不行，表示這條路是錯的
    memcpy(grid, backup, sizeof(backup)); // 還原盤面
    return false; // 回傳失敗，讓上一層去試別的數字
}

#endif
```

---

## 2. 程式入口：`src/sudoku_serial.cpp`

這是最基本的執行檔，負責讀取輸入、呼叫解題函式、並計算時間。

```cpp
#include "sudoku_common.h" // 引入剛剛解釋的通用函式

int main() {
    int grid[N][N];
    
    // 1. 讀取輸入
    // 從標準輸入 (cin) 讀取 81 個數字
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (!(cin >> grid[i][j])) return 0; // 讀取失敗就結束
        }
    }

    // 2. 開始計時
    auto start = chrono::high_resolution_clock::now();
    
    // 3. 呼叫解題函式
    if (solve_serial(grid)) {
        // 4. 結束計時
        auto end = chrono::high_resolution_clock::now();
        chrono::duration<double, std::milli> elapsed = end - start;
        
        // 輸出花費的毫秒數
        cout << elapsed.count() << " ms" << endl;
    } else {
        cout << "No solution found." << endl;
    }

    return 0;
}
```

---

## 3. SIMD 優化：`src/sudoku_simd.h`

這個檔案使用 **AVX2 指令集** 來加速運算。
CPU 通常一次只能處理一個數字，但 SIMD (Single Instruction, Multiple Data) 讓 CPU 可以一次處理多個數字 (例如一次處理 8 個整數)。

### 關鍵概念：AVX2
- `__m256i`: 一個特殊的變數型態，可以塞進 8 個 32-bit 的整數 (總共 256 bits)。
- `_mm256_loadu_si256`: 一次把 8 個數字從記憶體讀進來。
- `_mm256_and_si256`: 一次對 8 個數字做 AND 運算。

### 程式碼詳解

```cpp
#ifndef SUDOKU_SIMD_H
#define SUDOKU_SIMD_H

#include <immintrin.h> // 引入 SIMD 指令集
#include "sudoku_common.h"

// ---------------------------------------------------------
// 函式：h_or (Horizontal OR)
// 目的：把一個向量 (vector) 裡的所有數字做 OR 運算，合併成一個數字
// ---------------------------------------------------------
inline int h_or(__m256i v) {
    // 這裡用了很底層的技巧，把 256-bit 的向量不斷對折做 OR
    // 就像把一張紙對折再對折，最後剩下一個點
    __m128i vlow = _mm256_castsi256_si128(v);
    __m128i vhigh = _mm256_extracti128_si256(v, 1);
    vlow = _mm_or_si128(vlow, vhigh);
    __m128i vshuf = _mm_shuffle_epi32(vlow, _MM_SHUFFLE(1, 0, 3, 2));
    vlow = _mm_or_si128(vlow, vshuf);
    vshuf = _mm_shuffle_epi32(vlow, _MM_SHUFFLE(2, 3, 0, 1));
    vlow = _mm_or_si128(vlow, vshuf);
    return _mm_cvtsi128_si32(vlow);
}

// ---------------------------------------------------------
// 函式：get_candidates_simd
// 目的：用 SIMD 加速找出候選數
// ---------------------------------------------------------
inline int get_candidates_simd(int grid[N][N], int r, int c) {
    int used = 0;
    __m256i v_used = _mm256_setzero_si256(); // 初始化為 0
    __m256i v_ones = _mm256_set1_epi32(1);   // 裝滿 1 的向量
    __m256i v_zero = _mm256_setzero_si256();

    int k = 0;
    // 一次處理 8 個格子 (因為 AVX2 可以處理 8 個 int)
    for (; k <= N - 8; k += 8) {
        // 1. 讀取這一列的 8 個數字
        __m256i v_vals = _mm256_loadu_si256((__m256i*)&grid[r][k]);
        
        // 2. 檢查哪些格子不是 0 (有填數字)
        __m256i v_mask = _mm256_cmpgt_epi32(v_vals, v_zero);
        
        // 3. 計算位移量：(1 << (val - 1))
        // 先把數值減 1
        __m256i v_shifts = _mm256_sub_epi32(v_vals, v_ones);
        // 然後做左移運算
        __m256i v_bits = _mm256_sllv_epi32(v_ones, v_shifts);
        
        // 4. 只保留那些原本不是 0 的格子的結果
        v_bits = _mm256_and_si256(v_bits, v_mask);
        
        // 5. 把結果合併到 v_used
        v_used = _mm256_or_si256(v_used, v_bits);
    }
    // 處理剩下不足 8 個的部分 (例如 N=9，剩下 1 個)
    for (; k < N; k++) {
        if (grid[r][k] != 0) used |= (1 << (grid[r][k] - 1));
    }

    // 同一行的檢查 (因為記憶體不連續，SIMD 效果不好，所以用普通迴圈)
    for (int k = 0; k < N; k++) {
        if (grid[k][c] != 0) used |= (1 << (grid[k][c] - 1));
    }

    // 宮格檢查 (同上)
    int br = (r / SQRT_N) * SQRT_N;
    int bc = (c / SQRT_N) * SQRT_N;
    for (int i = 0; i < SQRT_N; i++) {
        for (int j = 0; j < SQRT_N; j++) {
            int val = grid[br + i][bc + j];
            if (val != 0) used |= (1 << (val - 1));
        }
    }

    // 把 SIMD 計算的結果合併回來
    used |= h_or(v_used);
    
    return used ^ ((1 << N) - 1);
}

// (propagate_simd 和 solve_simd_serial 與普通版邏輯相同，只是改呼叫 get_candidates_simd)
// ... (略過重複邏輯) ...

#endif
```

---

## 4. OpenMP 平行化：`src/sudoku_omp.cpp`

這個檔案使用 **OpenMP** 來讓多個 CPU 核心同時幫忙解題。
我們使用「任務平行 (Task Parallelism)」：當遇到分岔路口時（有很多數字可以填），就派不同的執行緒去試不同的路。

### 為什麼要用「任務 (Task)」而不是「迴圈 (For Loop)」？
一般的 OpenMP 教學通常會教 `#pragma omp parallel for`，那是用來把一個很大的迴圈切成好幾塊給大家分。
但在數獨裡，我們不知道「這條路會走多久」，也不知道「有多少條路」。這就像是在探險，隨時可能發現新岔路。
所以我們用 **Task**：每當發現一個岔路，就丟出一個「任務卷軸」，誰有空誰就撿去跑。

### 關鍵技術詳解

#### 1. 狀態複製 (State Copying) - `firstprivate`
這是平行化最困難的地方。
*   **問題**：如果所有人都共用同一個 `grid` 棋盤，A 執行緒填了數字，B 執行緒也會看到，這樣就亂套了！
*   **解決**：每個人都要有自己的「影分身」。
*   **實作**：我們定義了一個 `struct SudokuState` 來包裝棋盤。
    ```cpp
    struct SudokuState {
        int grid[N][N];
    };
    ```
    然後在產生任務時使用 `firstprivate(state)`：
    ```cpp
    #pragma omp task firstprivate(state) ...
    ```
    這句話的意思是：**「OpenMP 大哥，請幫我把目前的 state 複製一份，交給接這個任務的人。」** 這樣每個人改自己的棋盤，就不會打架了。

#### 2. 任務群組 (Taskgroup) - `taskgroup`
*   **問題**：我們怎麼知道分出去的任務做完了沒？
*   **解決**：使用 `#pragma omp taskgroup`。
*   **實作**：
    ```cpp
    #pragma omp taskgroup
    {
        // 在這裡面產生的所有任務 (包含任務又生出來的子任務)
        // 都要全部做完，程式才會離開這個大括號
    }
    ```
    這確保了我們不會在還沒試完所有可能性的時候就誤以為無解。

#### 3. 提早結束 (Early Exit) - `global_solved`
*   **問題**：如果 A 執行緒已經找到答案了，B、C、D 執行緒還在拚命算，不是很浪費電嗎？
*   **解決**：用一個全域變數 `global_solved` 來通知大家。
*   **實作**：
    *   有人找到答案時：`#pragma omp atomic write global_solved = true;` (原子操作，確保寫入安全)
    *   其他人做每一件事之前：先檢查 `if (global_solved) return;` (如果有人解完了，我就下班)

### 程式碼詳解

```cpp
#include <omp.h>
#include "sudoku_common.h"

// 設定遞迴深度限制，太深就不再產生新任務，避免任務太多反而變慢
// 就像公司裡，經理會把工作分給組長，組長分給組員，但組員就不會再分給工讀生了，自己做比較快
#ifndef CUTOFF_DEPTH
#define CUTOFF_DEPTH 2
#endif

// 全域變數：標記是否已經找到答案
bool global_solved = false;

struct SudokuState {
    int grid[N][N];
};

// ---------------------------------------------------------
// 函式：solve_omp (平行解題)
// ---------------------------------------------------------
bool solve_omp(SudokuState state, int depth) {
    if (global_solved) return true; // 如果別人已經解出來了，我就不用做了

    // 如果遞迴太深 (depth > 2)，就轉回序列版 (serial)
    // 因為建立 Task 也是有成本的 (要複製記憶體、要排程)，太碎的任務反而變慢
    if (depth > CUTOFF_DEPTH) { 
        if (solve_serial(state.grid)) {
            #pragma omp atomic write // 安全地寫入全域變數
            global_solved = true;
            return true;
        }
        return false;
    }

    // ... (中間的約束傳播和尋找最佳格子邏輯與序列版相同) ...
    // ... (假設我們找到了 best_r, best_c 和候選數字 moves) ...

    // 這裡開始平行化！
    if (moves.size() == 1) {
        // 如果只有一條路，就直接走，不用開新任務
        state.grid[best_r][best_c] = moves[0];
        if (solve_omp(state, depth + 1)) return true;
    } else {
        // 如果有多條路 (例如可以填 1, 3, 5)，就分給不同的人 (Task) 去走
        #pragma omp taskgroup // 等待這組任務完成
        {
            for (int val : moves) {
                if (global_solved) break; // 再次檢查，省一點時間
                
                // 建立一個新任務
                // firstprivate(state): 自動複製一份 state 給這個任務 (關鍵！)
                // priority(1): 設為高優先權 (選擇性優化)
                #pragma omp task firstprivate(state) shared(global_solved) priority(1)
                {
                    if (!global_solved) {
                        state.grid[best_r][best_c] = val; // 填入數字
                        if (solve_omp(state, depth + 1)) { // 遞迴呼叫
                            #pragma omp atomic write
                            global_solved = true; // 找到答案了！
                        }
                    }
                }
            }
        }
    }
    
    if (global_solved) return true;
    return false;
}
```

---

## 5. 終極合體：`src/sudoku_omp_simd.cpp`

這個檔案是我們的「最終兵器」，它結合了 **OpenMP (人海戰術)** 和 **SIMD (單兵強化)** 的優點。

### 結合策略：特種部隊模式
我們不是隨便把兩段程式碼貼在一起就好，而是有策略地分工：

1.  **宏觀層面 (Macro Level) - OpenMP 指揮官**
    *   負責處理「回溯法 (Backtracking)」產生的大量分岔路徑。
    *   當需要猜測時，產生 Task 讓不同的 CPU 核心去跑。

2.  **微觀層面 (Micro Level) - SIMD 士兵**
    *   負責處理「約束傳播 (Constraint Propagation)」和「尋找候選數」的繁重計算。
    *   在每一個 Task 內部，當 CPU 要檢查某個格子能填什麼數字時，使用 AVX2 指令一次檢查 8 個。

### 程式碼逐行詳解

這個檔案有兩個核心函式，我們一個一個來看。

#### 1. 可中斷的序列解題 (`solve_simd_serial_abortable`)

為什麼需要這個？因為在 OpenMP 平行運算中，如果別的執行緒已經找到答案了 (`global_solved = true`)，我們正在跑的序列運算也應該要馬上停下來，不要浪費時間。

```cpp
// 這是一個「會看臉色」的序列解題函式
bool solve_simd_serial_abortable(int grid[N][N]) {
    // 1. 隨時檢查：有沒有人已經解完了？
    if (global_solved) return true; 

    // 2. 備份盤面 (跟之前一樣)
    int backup[N][N];
    memcpy(backup, grid, sizeof(backup));

    // 3. 使用 SIMD 加速的約束傳播 (注意這裡呼叫的是 propagate_simd)
    if (!propagate_simd(grid)) {
        memcpy(grid, backup, sizeof(backup));
        return false;
    }

    // ... (中間尋找最佳格子的邏輯省略，跟之前一樣) ...

    // 4. 使用 SIMD 加速尋找候選數
    // int mask = get_candidates_simd(grid, i, j); 
    
    // ... (省略) ...

    // 5. 遞迴嘗試
    for (int val = 1; val <= N; val++) {
        // 再次檢查：遞迴每一層都要檢查有沒有人解完了
        if (global_solved) return true;
        
        if (best_mask & (1 << (val - 1))) {
            grid[best_r][best_c] = val;
            
            // 遞迴呼叫自己 (Abortable 版本)
            if (solve_simd_serial_abortable(grid)) return true;
        }
    }

    memcpy(grid, backup, sizeof(backup)); 
    return false;
}
```

#### 2. 平行解題主函式 (`solve_omp_simd`)

這是程式的入口，負責分派任務。

```cpp
bool solve_omp_simd(SudokuState state, int depth) {
    // 1. 檢查是否已解決
    if (global_solved) return true;

    // 2. 深度限制 (Cutoff)
    // 如果遞迴太深，就切換成上面的「可中斷序列版」
    // 這是為了避免產生太多細碎的任務 (Task Overhead)
    if (depth > CUTOFF_DEPTH) { 
        if (solve_simd_serial_abortable(state.grid)) {  
            #pragma omp atomic write
            global_solved = true; // 找到答案，通知大家
            return true;
        }
        return false;
    }

    // 3. 狀態管理 (State Management)
    // 因為是平行化，我們直接操作傳進來的 state 副本
    SudokuState backup = state; 

    // 使用 SIMD 加速傳播
    if (!propagate_simd(state.grid)) { 
        return false;
    }

    // ... (尋找最佳格子，使用 get_candidates_simd) ...

    // 4. 任務分派 (Task Creation)
    if (moves.size() == 1) {
        state.grid[best_r][best_c] = moves[0];
        if (solve_omp_simd(state, depth + 1)) return true;
    } else {
        #pragma omp taskgroup // 等待小組任務完成
        {
            for (int val : moves) {
                if (global_solved) break;
                
                // 產生新任務，並給予獨立的 state 副本 (firstprivate)
                #pragma omp task firstprivate(state) shared(global_solved) priority(1)
                {
                    if (!global_solved) {
                        state.grid[best_r][best_c] = val;
                        // 遞迴呼叫自己
                        if (solve_omp_simd(state, depth + 1)) {
                            #pragma omp atomic write
                            global_solved = true;
                        }
                    }
                }
            }
        }
    }
    
    return global_solved;
}
```

---

## 6. 自動化測試：`benchmark.py`

這個 Python 腳本用來自動執行所有版本的程式，並測量時間。

### 關鍵概念
- `subprocess`: 讓 Python 可以執行 C++ 編譯出來的執行檔。
- `os.environ`: 設定環境變數 (例如 `OMP_NUM_THREADS` 控制執行緒數量)。

### 程式碼詳解

```python
import subprocess
import time
import os

# ... (省略產生數獨的程式碼) ...

# ---------------------------------------------------------
# 函式：run_solver
# 目的：執行指定的解題程式，並回傳執行時間
# ---------------------------------------------------------
def run_solver(solver_path, grid_str, num_threads=1):
    if not os.path.exists(solver_path):
        return -1.0
        
    # 設定環境變數，告訴 OpenMP 要用幾個執行緒
    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = str(num_threads)

    try:
        # 啟動子行程 (Subprocess) 執行 C++ 程式
        process = subprocess.Popen(
            [solver_path],
            stdin=subprocess.PIPE,  # 準備輸入資料
            stdout=subprocess.PIPE, # 準備接收輸出
            stderr=subprocess.PIPE,
            text=True,
            env=env
        )
        
        # 把數獨題目 (grid_str) 傳給 C++ 程式，並設定 10 秒超時
        stdout, stderr = process.communicate(input=grid_str, timeout=10)

        # 解析輸出，尋找 "ms" 結尾的行
        for line in stdout.splitlines():
            if "ms" in line:
                return float(line.split()[0]) # 回傳時間
        return 0.0
    except Exception as e:
        print(f"Error: {e}")
        return -1.0
```

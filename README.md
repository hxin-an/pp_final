# Sudoku Solver with Constraint Propagation (Parallelized)

這個目錄包含了一個基於 Constraint Propagation (約束傳播) 和 Backtracking (回溯法) 的數獨解題程式。我們實作了四種版本來探討不同的優化策略：Serial (序列), OpenMP (多執行緒), SIMD (向量化), 以及 OpenMP + SIMD (混合平行)。

## 檔案結構說明

- **`src/`**: 原始碼目錄
    - **`sudoku_common.h`**: 定義通用的資料結構與輔助函式 (`get_candidates`, `propagate`, `solve_serial`)。
    - **`sudoku_serial.cpp`**: 序列版本主程式。
    - **`sudoku_omp.cpp`**: 純 OpenMP 平行版本主程式。
    - **`sudoku_simd.h`**: 定義 AVX2 SIMD 輔助函式 (`get_candidates_simd`, `propagate_simd`, `solve_simd_serial`)。
    - **`sudoku_simd.cpp`**: 純 SIMD 序列版本主程式。
    - **`sudoku_omp_simd.cpp`**: OpenMP + SIMD 混合版本主程式 (最佳效能)。
- **`problem/`**: 測試題目目錄 (包含 easy, medium, hard 分類)。
- **`reports/`**: 實驗報告目錄
    - **`difficulties_report.md`**: 平行化困難與解決方案報告。
- **`Makefile`**: 編譯腳本。
- **`benchmark.py`**: 自動化效能測試腳本 (產生隨機題目)。
- **`benchmark_real.py`**: 真實題目測試腳本 (讀取 `problem/` 目錄)。
- **`benchmark_results.txt`**: `benchmark.py` 的測試結果。
- **`benchmark_real_results.txt`**: `benchmark_real.py` 的測試結果。

---

## 編譯與執行

### 編譯
```bash
make              # 編譯所有版本 (9x9 和 16x16)
make clean        # 清除編譯結果
```

編譯後會在 `build/` 目錄下產生以下執行檔：
- **9x9 版本**: `sudoku_serial`, `sudoku_omp`, `sudoku_simd`, `sudoku_omp_simd`
- **16x16 版本**: `sudoku_serial_16`, `sudoku_omp_16`, `sudoku_simd_16`, `sudoku_omp_simd_16`

### 執行範例
```bash
# 9x9 題目
./build/sudoku_serial < problem/9x9/easy/1.txt
./build/sudoku_simd < problem/9x9/medium/1.txt
./build/sudoku_omp_simd < problem/9x9/hard/1.txt

# 16x16 題目
./build/sudoku_serial_16 < problem/16x16/easy/1.txt
OMP_NUM_THREADS=12 ./build/sudoku_omp_16 < problem/16x16/hard/1.txt
OMP_NUM_THREADS=24 ./build/sudoku_omp_simd_16 < problem/16x16/expert/1.txt
```

### 效能測試
```bash
python3 benchmark.py          # 隨機題目測試 (產生隨機數獨)
python3 benchmark_real.py     # 真實題目測試 (讀取 problem/ 目錄)
```

---

## 平行化與優化實作詳解

本專案的核心目標是透過平行化技術加速數獨解題，特別是針對計算量巨大的 16x16 題目。以下是我們採用的關鍵技術與實作細節：

### 1. 基礎演算法 (Constraint Propagation + Backtracking)
所有版本都基於相同的核心邏輯：
- **Minimum Remaining Values (MRV)**: 每次選擇候選數最少的格子進行嘗試，以減少搜尋空間。
- **Bitmask**: 使用整數的位元 (bit) 來表示候選數 (例如第 0 bit 為 1 代表數字 1 是候選)，加速集合運算。
- **Constraint Propagation**: 在填入一個數字後，立即檢查相關聯的行、列、宮，如果發現某格只剩下一個候選數 (Naked Single)，則立即填入，並連鎖反應。

### 2. SIMD 向量化 (`src/sudoku_simd.h`)
利用 **AVX2 指令集** 加速「計算候選數」的過程 (`get_candidates`)。這是整個演算法中最頻繁呼叫的熱點。

- **實作原理**:
    - 數獨盤面的一列有 N 個數字 (9 或 16)。我們可以一次載入 8 個整數 (`__m256i`) 進行平行處理。
    - 使用 `_mm256_loadu_si256` 載入 8 個格子。
    - 使用 `_mm256_cmpgt_epi32` 快速找出哪些格子有填數字。
    - 使用 `_mm256_sllv_epi32` 平行計算位移量 (`1 << (v-1)`)。
    - 最後透過 `_mm256_or_si256` 與 Shuffle 操作 (`h_or`) 將結果合併。
- **效能分析**:
    - 雖然 AVX2 理論頻寬是 8 倍，但受限於數獨的 Column 和 Box 資料在記憶體中不連續 (Non-contiguous)，無法完全向量化讀取，實際加速比約為 **1.5x - 2x**。

### 3. OpenMP 平行搜尋 (`src/sudoku_omp.cpp`)
利用 **Task Parallelism (任務平行)** 來平行化搜尋樹的探索。

- **Task Creation**: 當演算法選擇了一個格子並有多個候選數時，針對每一個候選數的嘗試 (Branch) 產生一個 OpenMP Task (`#pragma omp task`)。
- **Cutoff Strategy (截斷策略)**:
    - 為了避免產生過多細微的任務導致 Overhead 過大，我們設定了 `CUTOFF_DEPTH` (針對 16x16 設為 2)。
    - 當遞迴深度超過 2 層時，切換回序列執行 (`solve_serial`)。這確保了每個 Task 都有足夠的運算量 (Coarse-grained)。
- **State Management (狀態管理)**:
    - 使用 `firstprivate(state)` 讓 OpenMP 自動為每個 Task 建立盤面副本 (Copy Constructor)，避免 Race Condition 與 False Sharing。

### 4. OpenMP + SIMD 混合 (`src/sudoku_omp_simd.cpp`)
這是本專案效能最強的版本，結合了上述技術並解決了關鍵的效能瓶頸。

- **全面 SIMD 化**: 確保在 OpenMP 的每個 Task 中，以及 Leaf Node 的序列解題過程中，都呼叫 SIMD 優化的函式 (`propagate_simd`, `get_candidates_simd`)。
- **Abortable Serial Solver (可中斷的序列解題)**:
    - **問題**: 在平行搜尋中，如果某個執行緒進入了一個極深且無解的子樹，傳統的遞迴解題會一直執行直到該子樹窮盡。這會導致即使其他執行緒已經找到解了，該執行緒仍佔用資源。
    - **解法**: 實作了 `solve_simd_serial_abortable`。在序列遞迴的每一層，都會檢查 `global_solved` 原子變數。
    ```cpp
    if (global_solved) return true; // Early exit
    ```
    - **效益**: 這項改動是效能突破的關鍵。在 16x16 Expert 題目中，它讓所有執行緒在全域解出現的瞬間能夠立即停止。這創造了 **超線性加速 (Super-linear Speedup)**，因為平行搜尋能比序列搜尋更早「猜對」路徑。
- **Single Thread Optimization**:
    - 當 `OMP_NUM_THREADS=1` 時，直接呼叫序列 SIMD 解題，完全避開 OpenMP Task 的建立與排程 Overhead。這保證了在單核心或簡單題目 (9x9) 下不會變慢。

---

## 效能分析摘要

| 題目類型 | 最佳 Solver | 加速比 (vs Serial) | 原因 |
| :--- | :--- | :--- | :--- |
| **9x9 (所有難度)** | SIMD (1 Thread) | ~1.5x - 2x | 題目太小 (<0.1ms)，多執行緒 Overhead 過大。SIMD 提供穩定加速。 |
| **16x16 Hard** | OpenMP+SIMD (12 Threads) | ~7x | 題目有一定運算量，平行化收益大於 Overhead。 |
| **16x16 Expert** | OpenMP+SIMD (24 Threads) | **~1800x** | **平行搜尋優勢**：多執行緒同時探索不同分支，大幅縮短找到解的時間。 |

詳細的開發困難與解決方案請參考 `reports/difficulties_report.md`。

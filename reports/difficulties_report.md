# 平行化困難與解決方案報告

本報告詳細記錄了在將數獨解題程式平行化的過程中遇到的主要困難、根本原因分析以及最終的解決方案。

## 1. 任務粒度與排程開銷 (Task Granularity & Scheduling Overhead)

### 困難描述
在初期的 OpenMP 實作中，我們發現對於簡單的 9x9 數獨，平行版本的速度反而比序列版本慢。甚至在 16x16 的簡單題目中，多執行緒也未能帶來顯著加速。

### 原因分析
- **Overhead 過大**: 數獨的遞迴搜尋樹 (Search Tree) 非常龐大，但每個節點的運算量 (Workload) 很小。建立一個 OpenMP Task 的成本 (記憶體配置、排程) 可能遠大於該節點本身的運算時間。
- **任務過多**: 如果對搜尋樹的每一層都產生 Task，會瞬間產生數百萬個微小任務，導致 Runtime 忙於管理任務而非執行運算。

### 解決方案
- **Cutoff Strategy (截斷策略)**: 引入 `CUTOFF_DEPTH` 參數。只在搜尋樹的前幾層 (例如深度 < 2) 產生 Task，超過此深度後直接呼叫序列解題函式 (`solve_serial`)。這確保了每個 Task 都有足夠的運算量 (Coarse-grained)。
- **Tuning**: 針對 16x16 題目，我們實驗後將 `CUTOFF_DEPTH` 設為 2，這是在「足夠的平行度」與「過多的任務開銷」之間的最佳平衡點。

## 2. 記憶體管理與 False Sharing

### 困難描述
在平行搜尋中，每個執行緒需要維護獨立的盤面狀態 (Grid State)。如果管理不當，會導致 Race Condition 或嚴重的效能下降。

### 原因分析
- **Race Condition**: 若多個執行緒共用同一個 Grid 指標，會互相覆蓋數據。
- **False Sharing**: 若不同執行緒頻繁寫入相鄰的記憶體位址 (Cache Line)，會導致 CPU Cache Coherency Protocol 頻繁運作，大幅降低效能。

### 解決方案
- **Struct Encapsulation**: 定義 `struct SudokuState` 將二維陣列包裝起來。
- **Firstprivate**: 利用 OpenMP 的 `firstprivate` 子句。這會讓編譯器自動為每個 Task 呼叫 Copy Constructor 建立私有的狀態副本。這比手動 `malloc/memcpy` 更高效且安全，也避免了 False Sharing，因為每個 Task 的 Stack 變數通常位於不同的 Cache Line。

## 3. 搜尋延遲與無效運算 (The "Abort" Problem)

### 困難描述
在 16x16 Expert 等高難度題目測試中，我們觀察到一個嚴重問題：即使某個執行緒已經找到了解，其他執行緒仍然在繼續執行，甚至主程式遲遲無法結束。在某些情況下，Parallel 版本甚至比 Serial 版本更容易「卡住」在無解的深層搜尋中。

### 原因分析
- **缺乏中斷機制**: 傳統的遞迴解題 (`solve_serial`) 是設計給單執行緒的，它會堅持跑完整個子樹直到找到解或證明無解。但在平行環境下，如果 Thread A 進入了一個極深且無解的子樹，而 Thread B 已經找到了全域解，Thread A 仍然會繼續浪費 CPU 時間跑完它的子樹。
- **資源佔用**: 這些「殭屍」執行緒佔用了 CPU 資源，影響了系統整體的反應速度。

### 解決方案
- **Abortable Serial Solver**: 我們實作了 `solve_simd_serial_abortable`。
- **Check Flag**: 在遞迴的每一層 (或關鍵迴圈中)，加入對 `global_solved` 原子變數的檢查。
    ```cpp
    if (global_solved) return true; // Early exit
    ```
- **效益**: 這項改動是效能突破的關鍵。它讓所有執行緒在全域解出現的瞬間 (微秒級誤差內) 能夠立即停止手邊的工作。在 Expert 測試中，這將執行時間從「超時 (>30s)」縮短到了「1ms」。

## 4. SIMD 整合的陷阱

### 困難描述
最初的 `sudoku_omp_simd` 版本效能不如預期，甚至比純 OpenMP 版本還慢。

### 原因分析
- **錯誤的函式呼叫**: 經過 Code Review，發現雖然我們啟用了 SIMD，但在 OpenMP 的 Task 內部以及 Cutoff 後的序列執行部分，仍然呼叫了舊的純量函式 (`propagate`, `get_candidates`)，而非 SIMD 版本 (`propagate_simd`, `get_candidates_simd`)。這導致 SIMD 指令集完全沒有被用到。

### 解決方案
- **全面替換**: 將 `sudoku_omp_simd.cpp` 中所有的輔助函式呼叫替換為 `_simd` 後綴的版本。
- **驗證**: 修復後，單執行緒效能提升了約 2 倍，驗證了 SIMD 的生效。

## 5. 小規模題目的效能極限

### 困難描述
在 9x9 題目中，無論如何優化，平行版本都無法超越序列版本。

### 原因分析
- **Amdahl's Law**: 9x9 數獨的解題時間極短 (通常 < 0.05ms)。在這種時間尺度下，建立 Thread/Task 的固定開銷 (Overhead) 佔比過高。平行化帶來的加速無法抵消這些開銷。

### 解決方案
- **Single Thread Optimization**: 在程式中加入判斷，若 `OMP_NUM_THREADS=1`，則完全跳過 OpenMP 相關程式碼，直接執行序列解題。這確保了在單核心環境下不會有額外的 Overhead。

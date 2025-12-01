# [Slide 1] Title
# Parallelism on Sudoku Problem (數獨問題的平行化)

<br>
<br>
<br>

---
# [Slide 2] Problem Statement: 數獨規則
---

- **目標**: 在 N x N 的網格中填入數字 1 到 N。
- **約束**: 每一行、每一列、每一個 $\sqrt{N} \times \sqrt{N}$ 的子方格 (Box) 內，數字不得重複。
- **輸入**: 一個部分填寫的數獨盤面。

<br>
<br>
<br>

---
# [Slide 3] Problem Statement: 專案目標
---

- **規模**: 針對標準 **9x9** 以及更複雜的 **16x16** 數獨進行優化。
- **核心挑戰**: 16x16 的搜尋空間是指數級增長的，傳統回溯法 (Backtracking) 難以在合理時間內解出高難度題目。
- **效能指標**: 最小化找到「第一個」有效解的時間 (Latency)。

<br>
<br>
<br>

---
# [Slide 4] Proposed Solution: 核心思路
---

-   基於 **Constraint Satisfaction Problem (CSP)** 的框架。
-   結合 **Backtracking (回溯法)** 與 **Constraint Propagation (約束傳播)**。
-   利用平行化技術 (SIMD + OpenMP) 加速搜尋過程。

<br>
<br>
<br>

---
# [Slide 5] Proposed Solution: The Sequential Approach
---

1.  **Bitmask (位元遮罩)**:
    -   使用 `uint16_t` 的位元來表示候選數集合 (例如: `0010` 代表候選數 2)。
    -   優點: 集合的交集 (AND)、聯集 (OR) 運算只需一個 CPU 指令。
2.  **MRV (Minimum Remaining Values)**:
    -   啟發式搜尋: 每次選擇「候選數最少」的格子進行填寫 (Fail-first principle)。
    -   效果: 大幅減少分支數量，儘早觸發 Backtracking。
3.  **Constraint Propagation (約束傳播)**:
    -   當某格填入數字後，立即更新同行、列、宮的候選數。
    -   **Naked Single**: 若某格只剩一個候選數，立即填入並繼續傳播。

<br>
<br>
<br>

---
# [Slide 6] Parallel Strategy 1: SIMD Data Parallelism
---

-   **目標**: 加速 `get_candidates` (計算候選數)，這是最頻繁的熱點函式。
-   **核心邏輯**:
    -   使用 **AVX2 (256-bit)** 指令集，一次處理 8 個整數 (`__m256i`)。
    -   **Row/Col 檢查**:
        -   `_mm256_loadu_si256`: 載入 8 個格子。
        -   `_mm256_cmpgt_epi32`: 檢查格子是否非空 (大於 0)。
        -   `_mm256_sllv_epi32`: 平行計算位移量 `1 << (val - 1)`。
        -   `_mm256_or_si256`: 將結果合併到 `used` 遮罩中。
    -   **Block 檢查**: 雖然記憶體不連續，但仍透過計算索引進行部分向量化。
-   **效益**: 雖然記憶體存取不連續限制了頻寬，但減少了大量的分支指令 (Branch Instructions)。

<br>
<br>
<br>

---
# [Slide 7] Parallel Strategy 2: OpenMP Task Parallelism
---

-   **動態任務生成**:
    -   在遞迴分支點 (Branching) 對每個候選數使用 `#pragma omp task`。
    -   利用 OpenMP Runtime 的 Work Stealing 機制自動平衡負載。
-   **關鍵優化**:
    -   **Cutoff Strategy**: 設定 `CUTOFF_DEPTH = 2`。只在前兩層產生 Task，避免產生數百萬個微小任務 (Granularity Control)。
    -   **Firstprivate State**: 使用 `#pragma omp task firstprivate(state)`。
        -   自動呼叫 Copy Constructor 複製 `SudokuState`。
        -   確保每個執行緒擁有獨立的盤面副本，完全消除 Race Condition 與 False Sharing。

<br>
<br>
<br>

---
# [Slide 8] Challenge 1: Task Granularity (任務粒度)
---

-   **困難**: 數獨遞迴樹過深，若每層都開 Task，會產生數百萬個微小任務，排程開銷 (Overhead) 遠大於運算時間。
-   **解決**: **Cutoff Strategy (截斷策略)**
    -   只在搜尋樹的前 2 層產生 Task，之後切換回序列解題。
    -   確保每個 Task 都有足夠的運算量。

<br>
<br>
<br>

---
# [Slide 9] Challenge 2: Memory Contention (記憶體競爭)
---

-   **困難**: 多執行緒共用盤面會導致 Race Condition 與 False Sharing。
-   **解決**: **Firstprivate & Copying**
    -   使用 OpenMP 的 `firstprivate` 子句。
    -   每個 Task 自動複製一份獨立的盤面狀態 (`SudokuState`)，避免鎖與競爭。

<br>
<br>
<br>

---
# [Slide 10] Challenge 3: The "Abort" Problem (無效運算)
---

-   **困難**: 在尋找「第一個解」時，若 Thread A 找到解，Thread B 仍會繼續搜尋無解的深層子樹 (Zombie Threads)。
-   **解決**: **Abort Mechanism (提早退出)**
    -   引入 `atomic<bool> global_solved`。
    -   所有執行緒在迴圈中檢查此旗標，一旦為真立即 return。

<br>
<br>
<br>

---
# [Slide 11] Related Work: References
---

-   **Sudoku as a Constraint Problem**
    -   Helmut Simonis, "Sudoku as a Constraint Problem", *CP Workshop on Modeling and Reformulating Constraint Satisfaction Problems*, 2005.
    -   [Link](https://ai.dmi.unibas.ch/_files/teaching/fs13/ki/material/ki10-sudoku-inference.pdf)
    -   探討將數獨建模為 CSP 問題，並比較不同傳播技術 (Propagation Techniques) 的效能。

<br>
<br>
<br>

---
# [Slide 12] Performance Results (效能分析)
---

(To be completed by other members)

<br>
<br>
<br>

---
# [Slide 13] Conclusion: 專案總結
---

1.  **混合平行 (Hybrid Parallelism)**: 結合 SIMD (微觀優化) 與 OpenMP (巨觀搜尋) 效果最佳。
2.  **任務管理**: Cutoff Strategy 對於遞迴演算法的平行化至關重要。
3.  **提早退出**: 針對「尋找任意解」的問題，必須實作 Abort 機制才能發揮多核心優勢。


<br>
<br>
<br>

---
# [Slide 14] Conclusion: 未來展望
---

-   優化記憶體佈局 (Structure of Arrays) 以提升 SIMD 效率。
-   實作 Work Stealing 以更佳地平衡負載。

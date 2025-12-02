%%
%% This is file `sample-sigconf.tex',
%% generated with the docstrip utility.
%%
%% The original source files were:
%%
%% samples.dtx  (with options: `all,proceedings,bibtex,sigconf')
%% 
%% IMPORTANT NOTICE:
%% 
%% For the copyright see the source file.
%% 
%% Any modified versions of this file must be renamed
%% with new filenames distinct from sample-sigconf.tex.
%% 
%% For distribution of the original source see the terms
%% for copying and modification in the file samples.dtx.
%% 
%% This generated file may be distributed as long as the
%% original source files, as listed above, are part of the
%% same distribution. (The sources need not necessarily be
%% in the same archive or directory.)
%%
%%
%% Commands for TeXCount
%TC:macro \cite [option:text,text]
%TC:macro \citep [option:text,text]
%TC:macro \citet [option:text,text]
%TC:envir table 0 1
%TC:envir table* 0 1
%TC:envir tabular [ignore] word
%TC:envir displaymath 0 word
%TC:envir math 0 word
%TC:envir comment 0 0
%%
%% The first command in your LaTeX source must be the \documentclass
%% command.
%%
%% For submission and review of your manuscript please change the
%% command to \documentclass[manuscript, screen, review]{acmart}.
%%
%% When submitting camera ready or to TAPS, please change the command
%% to \documentclass[sigconf]{acmart} or whichever template is required
%% for your publication.
%%
%%
\documentclass[sigconf]{acmart}
%%
%% \BibTeX command to typeset BibTeX logo in the docs
\AtBeginDocument{%
  \providecommand\BibTeX{{%
    Bib\TeX}}}

%% Rights management information.  This information is sent to you
%% when you complete the rights form.  These commands have SAMPLE
%% values in them; it is your responsibility as an author to replace
%% the commands and values with those provided to you when you
%% complete the rights form.
\setcopyright{acmlicensed}
\copyrightyear{2025}
\acmYear{2025}
\acmDOI{XXXXXXX.XXXXXXX}
%% These commands are for a PROCEEDINGS abstract or paper.
\acmConference[Conference acronym 'XX]{Make sure to enter the correct
  conference title from your rights confirmation email}{June 03--05,
  2018}{Woodstock, NY}
%%
%%  Uncomment \acmBooktitle if the title of the proceedings is different
%%  from ``Proceedings of ...''!
%%
%%\acmBooktitle{Woodstock '18: ACM Symposium on Neural Gaze Detection,
%%  June 03--05, 2018, Woodstock, NY}
\acmISBN{978-1-4503-XXXX-X/2018/06}


%%
%% Submission ID.
%% Use this when submitting an article to a sponsored event. You'll
%% receive a unique submission ID from the organizers
%% of the event, and this ID should be used as the parameter to this command.
%%\acmSubmissionID{123-A56-BU3}

%%
%% For managing citations, it is recommended to use bibliography
%% files in BibTeX format.
%%
%% You can then either use BibTeX with the ACM-Reference-Format style,
%% or BibLaTeX with the acmnumeric or acmauthoryear sytles, that include
%% support for advanced citation of software artefact from the
%% biblatex-software package, also separately available on CTAN.
%%
%% Look at the sample-*-biblatex.tex files for templates showcasing
%% the biblatex styles.
%%

%%
%% The majority of ACM publications use numbered citations and
%% references.  The command \citestyle{authoryear} switches to the
%% "author year" style.
%%
%% If you are preparing content for an event
%% sponsored by ACM SIGGRAPH, you must use the "author year" style of
%% citations and references.
%% Uncommenting
%% the next command will enable that style.
%%\citestyle{acmauthoryear}


%%
%% end of the preamble, start of the body of the document source.
\begin{document}

%%
%% The "title" command has an optional parameter,
%% allowing the author to define a "short title" to be used in page headers.
\title{Parallelization on Sudoku Problems: Accelerating Search with SIMD and OpenMP}

\author{Liang-Yu Cheng}
\affiliation{%
  \institution{National Yang Ming Chiao Tung University}
  \city{Hsinchu}
  \country{Taiwan}
}
\email{joey60209@gmail.com}

\author{Sin-An Huang}
\affiliation{%
  \institution{National Yang Ming Chiao Tung University}
  \city{Hsinchu}
  \country{Taiwan}
}
\email{hxinan2002@gmail.com}

\author{Pai-Kuan Chang}
\affiliation{%
  \institution{National Yang Ming Chiao Tung University}
  \city{Hsinchu}
  \country{Taiwan}
}
\email{quan787887@gmail.com}

\renewcommand{\shortauthors}{Cheng et al.}

\begin{abstract}
Sudoku is a classic Constraint Satisfaction Problem (CSP) that becomes computationally intensive as the grid size increases. While 9x9 puzzles are trivial, 16x16 Sudoku presents a significant challenge due to its exponentially larger search space. This project explores and compares two distinct parallelization strategies for solving 16x16 Sudoku puzzles. The first approach, a \textbf{Hybrid Solver}, combines SIMD vectorization for efficient constraint checking with OpenMP task parallelism. The second approach, a \textbf{Lightweight Bitwise Solver}, focuses on minimizing memory footprint and synchronization overhead using a compact state representation and thread-based parallelism. We evaluate both methods on a range of difficulty levels. Our results show that while the Lightweight Solver offers excellent performance on easier instances due to low overhead, the Hybrid Solver achieves superior scalability on the hardest "Expert" instances, delivering super-linear speedups of up to 1800x.
\end{abstract}

\maketitle

\section{Introduction}
Sudoku is a logic-based combinatorial puzzle that serves as an excellent benchmark for backtracking algorithms and parallel search strategies. For large variants like $16 \times 16$, the search space is vast, requiring highly optimized solvers.

In this project, we investigate two contrasting philosophies for parallelizing the Sudoku solver:
\begin{enumerate}
    \item \textbf{Heavy Optimization & Dynamic Scheduling}: Leveraging hardware-specific features (AVX2 SIMD) and dynamic task scheduling (OpenMP) to maximize throughput per core and load balance.
    \item \textbf{Lightweight State & Static Partitioning}: Minimizing the cost of state management and context switching by using a highly compact data structure and straightforward thread parallelism (Pthreads/OpenMP).
\end{enumerate}
We implement both approaches and conduct a comparative analysis to understand their trade-offs in terms of latency, scalability, and implementation complexity.

\section{Problem Statement}
The goal is to find a single valid assignment of digits to an $N \times N$ grid (where $N=16$) such that every row, column, and $\sqrt{N} \times \sqrt{N}$ box contains distinct digits from 1 to $N$. The solver must handle "Expert" level puzzles where the search tree is extremely deep and unbalanced.

\section{Methods}
We developed two independent solvers to explore different parallelization paradigms.

\subsection{Method A: Hybrid SIMD + OpenMP Solver}
This approach aims to accelerate the "hot path" of the solver using vectorization while using a sophisticated task scheduler for parallelism.

\subsubsection{SIMD Data Parallelism}
We identified `get\_candidates` as the most compute-intensive function. We utilized AVX2 (256-bit) intrinsics to vectorize the constraint checking process. By loading 8 integer cells at a time and using bitwise logic (`\_mm256\_or\_si256`, `\_mm256\_sllv\_epi32`) instead of conditional branches, we significantly reduced branch mispredictions and increased instruction throughput.

\subsubsection{OpenMP Tasking}
We employed OpenMP's tasking model (`\#pragma omp task`) to handle the irregular nature of the Sudoku search tree.
\begin{itemize}
    \item \textbf{Dynamic Cutoff}: Tasks are only spawned at the top levels of the recursion (depth $\le$ 2) to prevent granular overhead.
    \item \textbf{Firstprivate State}: Each task receives a private copy of the full `SudokuState`. While this consumes more memory, it completely eliminates false sharing and lock contention.
    \item \textbf{Abort Mechanism}: An atomic flag is used to signal all threads to stop immediately once a solution is found.
\end{itemize}

\subsection{Method B: Lightweight Bitwise Solver}
This approach prioritizes minimizing the memory footprint and management overhead, based on the observation that memory allocation and copying can become bottlenecks in parallel search.

\subsubsection{Compact State Representation}
Instead of maintaining a full grid object with complex metadata, this solver uses a minimal state consisting of:
\begin{itemize}
    \item Three arrays of 64-bit integers: `rowMask`, `colMask`, `boxMask`.
    \item A flattened 1D array for the grid.
\end{itemize}
This compact representation fits easily into CPU caches, reducing cache misses during the frequent state updates of the backtracking process.

\subsubsection{Thread-Based Parallelism}
Parallelism is applied at the first branching point of the search tree. The solver uses the Minimum Remaining Values (MRV) heuristic to find the cell with the fewest candidates, then spawns a thread (using Pthreads or OpenMP) for each candidate.
\begin{itemize}
    \item \textbf{Low Overhead}: Because the state is so small, copying it to a new thread is extremely fast.
    \item \textbf{Stack Allocation}: State copies are often allocated on the stack, avoiding heap allocation overheads (`new`/`delete`).
\end{itemize}

\section{Experimental Methodology}
We evaluated both solvers on a Linux workstation with an Intel CPU. We used a dataset of 16x16 Sudoku puzzles classified into Easy, Medium, Hard, and Expert. We measured the wall-clock time to find the first solution.

\section{Experimental Results}
Table \ref{tab:results} compares the performance of the Serial baseline, Method A (Hybrid), and Method B (Lightweight).

\begin{table}[h]
  \caption{Execution Time (ms) for 16x16 Sudoku}
  \label{tab:results}
  \begin{tabular}{lcccc}
    \toprule
    Difficulty & Serial & Method A (Hybrid) & Method B (Lightweight) \\
    \midrule
    Easy & 0.0295 & 0.4832 & \textbf{0.2103} \\
    Medium & 0.1343 & 0.5607 & \textbf{0.2517} \\
    Hard & 21.14 & \textbf{3.48} & 6.74 \\
    Expert & 2066.79 & \textbf{1.15} & N/A (Timeout) \\
    \bottomrule
  \end{tabular}
\end{table}

\subsection{Analysis}
\begin{itemize}
    \item \textbf{Easy/Medium Cases}: Method B (Lightweight) outperforms Method A. The overhead of OpenMP task creation and SIMD loading in Method A is not amortized when the problem is solved in microseconds. Method B's minimal state copy is advantageous here.
    \item \textbf{Hard/Expert Cases}: Method A (Hybrid) dominates. For "Expert" puzzles, the search tree is massive. Method A's SIMD optimizations significantly speed up the millions of node expansions, and the dynamic task stealing of OpenMP handles the unbalanced tree better than the static threading model of Method B. The super-linear speedup (1.15ms vs 2066ms) in Method A suggests its search order coupled with parallel exploration allowed it to find a "lucky" branch very early.
\end{itemize}

\section{Contributions}
\begin{itemize}
    \item \textbf{Liang-Yu Cheng}: Designed and implemented \textbf{Method B (Lightweight Bitwise Solver)}, optimizing state representation and thread management.
    \item \textbf{Pai-Kuan Chang}: Designed and implemented \textbf{Method A (Hybrid Solver)}, integrating SIMD vectorization and OpenMP tasking.
    \item \textbf{Sin-An Huang}: Conducted performance benchmarking, data analysis, and compiled the final comparative report.
\end{itemize}

\section{Conclusion}
We successfully explored two parallelization strategies for Sudoku. Our findings suggest a trade-off: for simple problems, a lightweight, memory-efficient approach (Method B) is superior due to low overhead. However, for computationally demanding instances, the robust, hardware-optimized approach (Method A) delivers the necessary throughput and scalability. Future work could involve combining Method B's compact state with Method A's dynamic scheduling to get the best of both worlds.
\begin{thebibliography}{10}

\bibitem{simonis2005sudoku}
Helmut Simonis.
\newblock Sudoku as a Constraint Problem.
\newblock In {\em CP Workshop on Modeling and Reformulating Constraint Satisfaction Problems}, pages 13--27, 2005.
\newblock \url{https://www.researchgate.net/publication/220958936_Sudoku_as_a_Constraint_Problem}

\bibitem{terboven2015openmptasking}
C.~Terboven and M.~Klemm.
\newblock Advanced OpenMP Tutorial -- Tasking.
\newblock SC15 Tutorial Slides, OpenMP.org, 2015.
\newblock \url{https://www.openmp.org/wp-content/uploads/sc15-openmp-CT-MK-tasking.pdf}

\bibitem{blumofe1999workstealing}
R.~D. Blumofe and C.~E. Leiserson.
\newblock Scheduling Multithreaded Computations by Work Stealing.
\newblock {\em Journal of the ACM}, 46(5):720--748, 1999.
\newblock \url{https://www.csd.uwo.ca/~mmorenom/CS433-CS9624/Resources/Scheduling_multithreaded_computations_by_work_stealing.pdf}

\bibitem{aili2023mpiSudoku}
H.~Aili.
\newblock Using MPI One-Sided Communication for Parallel Sudoku Solving.
\newblock Bachelor's thesis, Ume{\aa} University, 2023.
\newblock \url{https://www.diva-portal.org/smash/get/diva2:1766597/FULLTEXT01.pdf}

\end{thebibliography}









\end{document}
\endinput
%%
%% End of file `sample-sigconf.tex'.
# Archive

This directory contains **obsolete programs** and experimental implementations
of Curve Number (CN) generation.  
They are preserved for reference but generally provide **lower performance**
than the current C MPI implementation in [`src/`](../src/), which is the
**main program** of this repository.

## Contents

- **arcs/**  
  Analysis of Antecedent Runoff Condition (ARC).

- **python/**  
  Python implementations of CN generation:  
  - Serial version.  
  - Parallel version using Python multiprocessing.

- **hybrid/**  
  Hybrid implementation of CN generation using C with MPI + OpenMP.

---

These programs illustrate earlier approaches and prototypes.  
For production use, please see the **C MPI program in [`src/`](../src/)**.

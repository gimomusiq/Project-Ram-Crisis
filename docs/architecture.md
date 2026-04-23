# RAM Deduplication Engine Architecture

## Overview

This design targets a secure, low-overhead RAM deduplication engine for Windows. The initial implementation is a user-space prototype that validates the core algorithm and hashing strategy. Production deployment requires a Windows kernel-mode driver with proper page-table and memory manager integration.

## Components

### Page Scanner

- Scans memory in 4 KB page units.
- Computes a page signature using a fast 64-bit hash plus a secondary fingerprint.
- Uses a low-priority background worker thread to avoid interfering with foreground tasks.
- Prioritizes user-mode private pages and avoids kernel/system-critical regions.

### Deduplication Manager

- Maintains a hash table of candidate pages keyed by computed signature.
- Resolves collisions through full page comparisons.
- Tracks shared page reference counts and shared mapping state.
- Handles copy-on-write by cloning pages when a write occurs to a shared page.

### Security Layer

- Supports per-process opt-in semantics.
- Restricts deduplication to safe memory regions.
- Avoids deduping pages containing sensitive or sandboxed data unless explicitly authorized.
- Uses constant-time table probing and comparison to minimize timing side-channel risk.

### ML-Assisted Prediction (Future)

- Optional layer for ranking likely duplicate pages.
- Can use process metadata, historical duplicate patterns, and allocation behavior.
- Designed as an advisory component that improves scan efficiency without being required for correctness.
- In the prototype, this is modeled as a predictor that scores processes and memory regions by duplication likelihood.

### Scan Scheduler (Prototype)

- A background scan scheduler manages scan cadence and candidate selection.
- It uses ML predictions and process opt-in state to prioritize work.
- The scheduler is intentionally low-priority and periodic to mimic a safe OS-cooperative scan engine.

### VRAM-Aware Deduplication (Future)

- Envisioned as an extension when GPU/shared memory APIs allow secure cross-domain dedupe.
- This repo focuses on RAM-only dedupe first.

## Prototype goals

- Validate the hashing and duplicate detection algorithm.
- Demonstrate the savings potential with representative workloads.
- Model shared page state and copy-on-write behavior in a safe user-space simulator.
- Provide a safe basis for porting into a Windows kernel driver later.

## Next implementation steps

1. Build the Windows kernel driver skeleton in `driver/`.
2. Implement a kernel-mode page scanner that enumerates committed user pages.
3. Add a dedupe manager that can allocate shared physical pages and update process page tables.
4. Add user-mode opt-in API and control utility.
5. Expand the prototype with a real page-fault/COW handler in kernel mode.

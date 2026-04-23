# Project Vision

The RAM Dedupe Engine project aims to create a safe, high-performance memory deduplication system that reduces RAM pressure by sharing identical pages across processes while preserving correctness and security.

## Vision

We want a memory subsystem that:
- identifies identical committed pages in RAM
- merges duplicates into shared read-only pages
- tracks references and preserves copy-on-write semantics
- keeps per-process opt-in control so only cooperative workloads participate
- exposes diagnostics and safety controls for kernel and user space

## Why it matters

Modern workloads often create repeated in-memory copies of the same data. By deduplicating those pages:
- applications can reclaim memory without changing behavior
- system memory efficiency improves under load
- overall application density rises on Windows systems

## Core pillars

- `Safety`: preserve page isolation with robust copy-on-write and boundary checks
- `Correctness`: compute reliable page signatures and avoid false sharing
- `Visibility`: expose opt-in controls, diagnostics, and policy hooks
- `Performance`: scan efficiently, prioritize likely savings, and minimize overhead
- `Extensibility`: support future storage-tiering, predictive eviction, and hybrid RAM/VRAM dedupe

## Future path

Start with the prototype in `user/` and the driver skeleton in `driver/`, then evolve the solution into a full Windows WDK driver with:
- page enumeration and signature hashing
- duplicate page merging and reference counting
- kernel page fault interception for copy-on-write handling
- process-level opt-in policy and telemetry interfaces

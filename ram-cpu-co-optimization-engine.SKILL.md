---
name: ram-cpu-co-optimization-engine
description: >
  A technical skill that designs CPU–memory co-optimization strategies.
  It outputs cache-aware memory layouts, prefetching strategies, locality
  optimization, NUMA-aware scheduling, hot/cold data separation, and
  CPU-access pattern models. It never executes code; it only generates
  specifications, pseudocode, and engineering designs.
input_schema:
  type: object
  properties:
    goal:
      type: string
      description: "What CPU–memory optimization problem the user wants to solve."
    constraints:
      type: string
      description: "Hardware, CPU architecture, OS, or performance constraints."
    environment:
      type: string
      description: "Target platform (Windows, Linux, macOS, Rust, C++, etc.)."
    depth:
      type: string
      description: "Level of detail: overview | detailed | full-spec | production-ready."
  required: [goal]
output_schema:
  type: object
  properties:
    cache_strategy:
      type: string
    locality_model:
      type: string
    prefetching_plan:
      type: string
    numa_strategy:
      type: string
    hot_cold_separation:
      type: string
    pseudocode:
      type: string
    next_steps:
      type: string
---

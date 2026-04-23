---
name: ram-layout-fragmentation-engine
description: >
  A technical skill that designs memory layout, fragmentation mitigation,
  and page-relocation strategies. It outputs NUMA-aware placement models,
  large-page promotion/demotion logic, contiguous allocation strategies,
  defragmentation heuristics, and relocation pipelines. It never executes
  code; it only generates specifications, pseudocode, and engineering designs.
input_schema:
  type: object
  properties:
    goal:
      type: string
      description: "What layout or fragmentation problem the user wants to solve."
    constraints:
      type: string
      description: "Hardware, OS, NUMA, or performance constraints."
    environment:
      type: string
      description: "Target platform (Windows, Linux, macOS, user-space daemon, Rust, C++, etc.)."
    depth:
      type: string
      description: "Level of detail: overview | detailed | full-spec | production-ready."
  required: [goal]
output_schema:
  type: object
  properties:
    architecture:
      type: string
    placement_model:
      type: string
    fragmentation_strategy:
      type: string
    memory_model:
      type: string
    pseudocode:
      type: string
    performance_estimates:
      type: string
    next_steps:
      type: string
---

---
name: ram-compression-engine
description: >
  A technical skill that designs and analyzes RAM compression systems.
  It outputs compression pipelines, algorithm choices, adaptive strategies,
  per-process tuning, decompression cost models, and integration plans.
  It never executes code; it only generates specifications, pseudocode,
  and engineering designs.
input_schema:
  type: object
  properties:
    goal:
      type: string
      description: "What the user wants to achieve (e.g., design a compression layer, tune algorithms, model performance)."
    constraints:
      type: string
      description: "Hardware, OS, language, or performance constraints."
    environment:
      type: string
      description: "Target platform (Windows, Linux, macOS, kernel module, user-space daemon, Rust, C++, etc.)."
    depth:
      type: string
      description: "Level of detail: overview | detailed | full-spec | production-ready."
  required: [goal]
output_schema:
  type: object
  properties:
    architecture:
      type: string
    algorithms:
      type: string
    adaptive_strategies:
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

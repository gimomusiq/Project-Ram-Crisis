---
name: ram-performance-simulation-engine
description: >
  A technical skill that models and simulates memory system performance.
  It outputs throughput models, latency estimates, page churn simulations,
  dedupe/compression hit-rate predictions, VRAM residency models, and
  architecture comparisons. It never executes code; it only generates
  specifications, pseudocode, and engineering designs.
input_schema:
  type: object
  properties:
    goal:
      type: string
      description: "What performance or simulation question the user wants to explore."
    constraints:
      type: string
      description: "Hardware, OS, or performance constraints."
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
    simulation_model:
      type: string
    metrics:
      type: string
    architecture_comparison:
      type: string
    performance_estimates:
      type: string
    pseudocode:
      type: string
    next_steps:
      type: string
---

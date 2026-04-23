---
name: ram-predictive-eviction-engine
description: >
  A technical skill that designs predictive memory eviction systems.
  It outputs page-scoring models, ML-assisted prediction strategies,
  eviction heuristics, swap optimization plans, and performance models.
  It never executes code; it only generates specifications, pseudocode,
  and engineering designs.
input_schema:
  type: object
  properties:
    goal:
      type: string
      description: "What the user wants to achieve (e.g., design eviction heuristics, build prediction models)."
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
    prediction_model:
      type: string
    eviction_strategies:
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

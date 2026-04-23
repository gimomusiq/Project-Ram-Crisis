---
name: ram-dedupe-engine
description: >
  A technical skill that designs, analyzes, and optimizes RAM deduplication systems.
  It outputs architecture diagrams, algorithms, memory models, hashing strategies,
  scanning pipelines, security layers, and performance estimates. It never executes
  code; it only generates specifications, pseudocode, and implementation plans.
input_schema:
  type: object
  properties:
    goal:
      type: string
      description: "What the user wants to achieve (e.g., design a dedupe engine, optimize scanning, create hashing strategy)."
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
    memory_model:
      type: string
    security_model:
      type: string
    pseudocode:
      type: string
    performance_estimates:
      type: string
    next_steps:
      type: string
---

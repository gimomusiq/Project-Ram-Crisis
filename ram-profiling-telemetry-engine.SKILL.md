---
name: ram-profiling-telemetry-engine
description: >
  A technical skill that designs memory profiling, telemetry, and analysis systems.
  It outputs instrumentation plans, sampling strategies, page-level metrics,
  fragmentation analysis, churn models, and profiling pipelines. It never executes
  code; it only generates specifications, pseudocode, and engineering designs.
input_schema:
  type: object
  properties:
    goal:
      type: string
      description: "What the user wants to measure or analyze (e.g., page churn, dedupe potential, compression ratios)."
    constraints:
      type: string
      description: "Hardware, OS, language, or performance constraints."
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
    instrumentation:
      type: string
    metrics:
      type: string
    sampling_pipeline:
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

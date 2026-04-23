---
name: ram-storage-tiering-engine
description: >
  A technical skill that designs multi-tier memory systems using RAM, SSD,
  and optional VRAM. It outputs hot/cold page classification models, NVMe
  streaming pipelines, read-ahead/write-behind strategies, swap optimization,
  and tiered memory hierarchies. It never executes code; it only generates
  specifications, pseudocode, and engineering designs.
input_schema:
  type: object
  properties:
    goal:
      type: string
      description: "What tiering or memory hierarchy problem the user wants to solve."
    constraints:
      type: string
      description: "Hardware, SSD/NVMe, OS, or performance constraints."
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
    tiering_model:
      type: string
    hot_cold_classifier:
      type: string
    streaming_pipeline:
      type: string
    swap_optimization:
      type: string
    pseudocode:
      type: string
    next_steps:
      type: string
---

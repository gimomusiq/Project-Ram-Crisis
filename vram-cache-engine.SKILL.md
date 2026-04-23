---
name: vram-cache-engine
description: >
  A technical skill that designs hybrid RAM/VRAM caching systems.
  It outputs VRAM allocation strategies, GPU-accelerated hashing pipelines,
  offloading heuristics, memory residency models, and integration plans.
  It never executes code; it only generates specifications, pseudocode,
  and engineering designs.
input_schema:
  type: object
  properties:
    goal:
      type: string
      description: "What the user wants to achieve (e.g., design VRAM cache, GPU hashing, hybrid memory model)."
    constraints:
      type: string
      description: "Hardware, GPU architecture, OS, or performance constraints."
    environment:
      type: string
      description: "Target platform (Windows, Linux, macOS, Vulkan, CUDA, DirectX, Rust, C++, etc.)."
    depth:
      type: string
      description: "Level of detail: overview | detailed | full-spec | production-ready."
  required: [goal]
output_schema:
  type: object
  properties:
    architecture:
      type: string
    gpu_pipeline:
      type: string
    caching_strategies:
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

---
name: ram-cross-process-sharing-engine
description: >
  A technical skill that designs proactive cross-process memory sharing systems.
  It outputs shared-region models, asset-sharing strategies, DLL/so residency
  plans, shared texture/font/JS library caches, and cooperative loading pipelines.
  It never executes code; it only generates specifications, pseudocode, and
  engineering designs.
input_schema:
  type: object
  properties:
    goal:
      type: string
      description: "What cross-process sharing problem the user wants to solve."
    constraints:
      type: string
      description: "OS, security, privilege, or performance constraints."
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
    shared_region_model:
      type: string
    asset_sharing_strategy:
      type: string
    residency_plan:
      type: string
    cooperative_loading_pipeline:
      type: string
    pseudocode:
      type: string
    next_steps:
      type: string
---

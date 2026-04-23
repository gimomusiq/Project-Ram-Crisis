---
name: ram-os-integration-engine
description: >
  A technical skill that designs OS integration strategies for memory systems.
  It outputs user-space daemon models, driver interaction plans, privilege
  boundaries, IPC mechanisms, API surfaces, and safe integration patterns.
  It never executes code; it only generates specifications, pseudocode, and
  engineering designs.
input_schema:
  type: object
  properties:
    goal:
      type: string
      description: "What integration problem the user wants to solve (e.g., user-space daemon, driver hooks, API design)."
    constraints:
      type: string
      description: "OS, security, privilege, or performance constraints."
    environment:
      type: string
      description: "Target platform (Windows, Linux, macOS, kernel cooperation, Rust, C++, etc.)."
    depth:
      type: string
      description: "Level of detail: overview | detailed | full-spec | production-ready."
  required: [goal]
output_schema:
  type: object
  properties:
    architecture:
      type: string
    api_surface:
      type: string
    privilege_model:
      type: string
    ipc_strategy:
      type: string
    integration_plan:
      type: string
    pseudocode:
      type: string
    next_steps:
      type: string
---

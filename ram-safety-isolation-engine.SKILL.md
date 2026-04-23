---
name: ram-safety-isolation-engine
description: >
  A technical skill that designs safety, isolation, and correctness models for
  memory systems. It outputs sandboxing strategies, copy-on-write rules,
  privilege boundaries, timing-attack mitigations, and safe dedupe/compression
  integration patterns. It never executes code; it only generates specifications,
  pseudocode, and engineering designs.
input_schema:
  type: object
  properties:
    goal:
      type: string
      description: "What safety or isolation problem the user wants to solve."
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
    threat_model:
      type: string
    isolation_strategy:
      type: string
    privilege_model:
      type: string
    copy_on_write_rules:
      type: string
    timing_mitigation:
      type: string
    pseudocode:
      type: string
    next_steps:
      type: string
---

# Security and Safety Review

This review focuses on the safe implementation of RAM page deduplication, including page sharing, memory boundary handling, and opt-in semantics.

## Process Opt-In Semantics

- The deduplication engine only operates on processes that are explicitly opted in.
- User-mode control is provided via IOCTLs and a controller utility.
- Opt-in scope should be restricted to cooperative workloads and clearly documented.
- The driver rejects system-critical and blacklisted process images during opt-in registration to reduce the risk of impacting protected OS services.

## Page Sharing and Isolation

- Shared pages must remain read-only in all participating processes.
- Any write access to a shared deduped page must trigger a copy-on-write operation.
- The driver should never merge pages from security boundaries or differently privileged processes without explicit authorization.

## Memory Boundary Handling

- The page scanner should enumerate only committed user-space pages and avoid kernel or reserved memory regions.
- Page signature computation must be deterministic and consistent across identical content.
- Page comparisons should only take place after hash/signature matching to avoid false deduplication.

## Copy-On-Write Handling

- The driver should register a write-fault interception path for deduped pages.
- On a write fault, the engine must allocate a private copy of the page and update the process mapping.
- A runtime COW-protected page registry should track which physical pages are currently shared and require copy-on-write semantics.
- Reference counts must be updated atomically to avoid stale shared mappings.

## Diagnostics and Safety Controls

- The engine should expose diagnostics for opted-in processes, page entry counts, and duplicate page events.
- Safety controls should allow disabling deduplication at runtime and draining shared state without crashing processes.
- The driver should expose validation paths for opt-in registration, health queries, and runtime drain operations.
- The driver should avoid holding long-lived locks while traversing page tables or processing scan candidates.

## Recommended safety tests

- Validate opt-in policy enforcement against system-critical and protected process IDs.
- Confirm that drain operations clear shared state and do not leave stale protected pages in the registry.
- Verify that health diagnostics remain consistent before and after opt-in/out transitions.
- Include threading and concurrency tests for scan scheduling and shared registry access.

## Review Conclusions

The current prototype includes:
- explicit opt-in control via `controller` commands
- diagnostics query support from the driver
- committed user page enumeration and page signature computation using kernel memory manager inspection
- shared-page group promotion with duplicate page tracking and a COW-protected page registry
- runtime opt-in policy enforcement to exclude protected OS processes
- dedicated documentation for release, packaging, and safety review

This review finds the design ready for test-signed validation in a real Windows WDK environment. Future work should still confirm page-fault/COW callback behavior, validate signed driver deployment policies, and confirm stability under representative workloads.

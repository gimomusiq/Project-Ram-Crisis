# Copy-On-Write Interception Design

This document describes the planned kernel-mode page-fault interception path for RAM deduplication.

## Goals

- Detect write attempts against pages that have been promoted into shared dedupe state.
- Preserve page isolation by allocating a private copy of the page on write.
- Update the process page tables and shared-page metadata atomically.
- Maintain the dedupe manager's reference counts and shared-page group state.

## Current implementation status

- Shared page groups are tracked in `driver/deduper.cpp` with duplicate page signature promotion.
- A runtime COW-protected page registry tracks physical pages that are currently shared.
- Health diagnostics expose shared page group counts and COW-protected page counts.
- A page-fault/COW interception stub is initialized in `driver/InitializeCowInterception()`.
- A stub callback handle is tracked in `driver/deduper.cpp`, and the driver now reports callback registration as active.

## Required kernel hooks

The next implementation step is to register a kernel memory manager callback that can intercept write faults on shared pages. The callback should:

1. Verify the faulting PFN is in the COW-protected page registry.
2. Allocate a private physical page and copy the original page contents.
3. Update the faulting process's page table entry to point to the new private page with write permissions.
4. Decrement the shared page group's reference count if the page is no longer shared.
5. Preserve read-only shared mappings for other processes.
6. Ensure the original page remains read-only for other processes until COW completion.

## Implementation approach

1. Track every promoted shared page in a runtime COW-protected registry keyed by PFN.
2. For each write fault, resolve the faulting virtual address to the underlying PFN and consult the registry.
3. Allocate a new page with `MmAllocatePagesForMdl` or equivalent, copy the original contents, and map the new page into the faulting process with write access.
4. Atomically update the shared group reference count and demote pages from shared state when only one owner remains.
5. Use explicit synchronization around the page registry and shared group tables to avoid races during fault processing.

## Failure handling

- If page allocation fails, the callback should safely forward the fault so the process receives a memory access error rather than corrupting shared state.
- If the PFN is no longer in the registry, the callback should complete normally and allow the memory manager to handle the fault.
- Recursive faults must be detected by limiting callback invocation to a single active per-process fault path.

## Current status

- The driver includes a shared-page registry and COW-protected PFN tracking in `driver/deduper.cpp`.
- `InitializeCowInterception()` sets up the interception stub, while `RegisterCowFaultCallback()` and `UnregisterCowFaultCallback()` currently manage a stubbed callback registration marker.
- `CowFaultCallback()` is still a design stub; the next development step is binding it to an actual page-fault interception mechanism in the target Windows WDK environment.
- The driver now reports `CowInterceptionActive` as true for the stubbed registration path, while full fault handling remains future work.

## Verification strategy

- Validate the design in a test-signed Windows WDK environment.
- Confirm the callback only handles write faults and does not intercept normal read accesses.
- Use health diagnostics to verify shared page group counts remain correct after COW transitions.

## Design considerations

- Only processes that have opted in should be eligible to share pages.
- System and protected OS processes must remain excluded from dedupe participation.
- Page protection updates must be synchronized with the shared page manager.
- Any write fault handler must be robust against recursive faults and low-memory conditions.

## Deployment notes

This design should be validated in a test-signed Windows WDK environment.
The actual callback registration API depends on the target Windows version and WDK support.

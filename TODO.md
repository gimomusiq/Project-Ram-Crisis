# RAM Dedupe Publish Readiness TODO

- [x] Add a repository license file (LICENSE) for publish readiness.
- [x] Add CI workflows for Windows build/test of the user prototype and future driver targets.
- [x] Expand the root README.md with polished release-oriented build, usage, and architecture documentation.
- [x] Create a Windows WDK project scaffold in `driver/ram_dedupe.vcxproj`.
- [x] Implement kernel driver `DriverEntry`, control device creation, symbolic link setup, and base IOCTL handling.
- [x] Add a user-mode controller for process opt-in, stats, and process listing.
- [x] Add a minimal driver package manifest in `driver/ram_dedupe.inf`.
- [x] Extend driver control IOCTLs with process opt-out, health status, and runtime drain support.
- [x] Implement page enumeration and page signature computation for real committed user memory.
- [x] Build the shared-page dedupe manager with reference counting, page identity, and merge/unmerge state. (shared-page group manager implemented)
- Prototype status: the repository includes shared page registration and diagnostic support, but safe page-fault interception / copy-on-write handling for deduped shared pages remains future work. (COW-protected page registry implemented)
- [x] Add benchmark validation for memory savings and runtime behavior.
- [x] Perform a security and safety review for memory boundaries, paging semantics, and opt-in attack surface.
- [x] Produce a signed Windows driver release package with documentation and deployment instructions.
- [x] Integrate per-process opt-in policy enforcement and runtime safety checks. (blacklisted system processes rejected)
- [x] Create driver install/uninstall scripts and package validation steps.
- [x] Add unit and integration tests for driver control, page scanning, and dedupe invariants. (added driver control test harness)

## Known prototype limitations

- The current driver implementation tracks shared page groups and COW-protected PFNs, but it does not yet perform full kernel page-fault interception.
- The current `CowFaultCallback()` is a diagnostic stub; actual write-time copy-on-write behavior is outside this prototype release scope.
- Safety and regression testing should cover COW handling, page-table updates, and fault-handler recursion for the next development phase.

# Bug backlog

## Driver implementation bugs
- [x] Fix compiler error: `CompareCowProtectedPages` is used before it is declared in `driver/deduper.cpp`.
- [x] Correct remote process scanning: `ScanProcessPages()` reads another process's virtual memory without attaching to that process.
- [x] Remove invalid remote address checks: `MmIsAddressValid()` and `MmGetPhysicalAddress()` are called on target process addresses from the current context.
- [x] Harden `ZwQueryVirtualMemory` scanning so a single inaccessible region does not abort the entire process scan.
- [x] Add synchronization around `gDedupeContext` generic tables to avoid races between the scanner thread and IOCTL handlers.
- [x] Fix `ListOptedInProcesses` to preserve full 64-bit process IDs on x64 instead of truncating to `ULONG`.
- [x] Fix duplicate promotion: `AddPageEntry()` currently groups pages by signature only, without a full-page comparison to avoid hash collisions.
- [x] Prevent group overflow: `PageEntry` and `SharedPageGroup` currently cap PFN tracking at 8 entries and silently drop extra duplicates.
- [x] Make `CreateControlDevice()` robust against stale symbolic links or existing device names during driver load.
- [x] Reduce privileges when creating the scanner thread; `THREAD_ALL_ACCESS` is excessive for `PsCreateSystemThread()`.
- [x] Reconsider the hard-coded low user address start (`0x10000`) in `ScanProcessPages()` and ensure all valid regions are scanned.
- [x] Clarify `gCowInterceptionActive` state so it matches actual interception registration, not just shared-page promotion state.
- [x] Ensure `DrainDedupeState()` and shared-page cleanup correctly clear the full dedupe state and do not leave stale protected pages.

## Driver design gaps and missing coverage
- [x] Add real benchmark validation for the driver path, not just the user-space simulator.
- [x] Add integration tests that exercise real driver IOCTLs, page scanning, and shared-page state cleanup.
- The kernel-mode copy-on-write design is documented in `docs/cow_interception.md`, but the actual callback registration mechanism is left for a future implementation phase.
- Add safety tests for opt-in policy enforcement, page-table updates, and fault-handler recursion handling. (opt-in enforcement and COW interception reporting tests added)
- [x] Add packaging validation and signing support for the Windows driver release.
- [x] Harden `driver/package_driver.ps1` to validate build outputs, include release metadata, and support signed driver packaging.
- [x] Harden `driver/install_driver.ps1` and `driver/uninstall_driver.ps1` for existing service/device state and failure recovery.
- [x] Extend CI to build and validate the driver with the actual Windows WDK toolset rather than skipping on `msbuild.exe` availability.

## User-prototype and tooling issues
- [x] Consider making `ProcessState::pid` 64-bit for x64 realism in the user prototype.
- [x] Avoid holding the scan scheduler mutex while calling `engine.scanRegion()` to keep scheduling responsive.
- [x] Add a `scoreDetail` member or remove the unused assignment in `user/scan_scheduler.cpp` to match `scan_scheduler.h`.
- [x] Include `<sstream>` in `user/scan_scheduler.cpp` for `std::ostringstream` usage.
- [x] Increase `controller` process-list buffer handling or return required buffer size rather than truncating to 256 entries.
- [x] Validate `bytesReturned` and returned buffer length in `user/driver_test.cpp` for driver IOCTL queries.
- [x] Replace `assert()` test code with a proper unit test framework for the user prototype.
- [x] Add a driver-level controller test that validates the full opt-in, opt-out, stats, list, health, and drain command flow.
- [ ] Fix `driver/sign_driver.ps1` to securely free the plaintext BSTR after converting `SecureString` for signtool.
- [ ] Remove or use the unused `$releaseDocsFile` variable in `driver/package_driver.ps1` and ensure manifest path correctness.
- [ ] Verify secure password handling across `driver/create_signed_package.ps1` and `driver/sign_driver.ps1` and add input validation.
- [ ] Review documentation redundancy in `README.md`, `driver/README.md`, and `docs/cow_interception.md` to avoid repeated prototype caveats.

## Audit follow-up and progress milestones

- [ ] Implement a basic COW write-fault handling prototype in `driver/deduper.cpp` to move beyond the stubbed callback path.
- [ ] Add benchmark results from `user/benchmark.exe` and `user/driver_benchmark.exe` to `README.md` and `docs/release.md`.
- [ ] Capture a simple simulator/controller demo screenshot or short video for the repository README.
- [ ] Commit progress in logical increments: COW work, benchmarks, docs updates, and demo artifacts.

## Future work

- Implement kernel-mode copy-on-write interception and page-fault callback registration in the driver.
- Add actual `CowFaultCallback()` behavior to safely copy shared pages on write and preserve dedupe correctness.
- Expand regression coverage for COW handling, protected page cleanup, and opt-in/out process lifecycle.
- Add a full production driver release build configuration and signed package validation for Windows driver distribution.
- Add regression tests that explicitly validate page-table updates, COW fault recursion safety, and shared page demotion.
- Replace the current stubbed COW callback registration path with a real memory manager fault hook in the Windows WDK driver.

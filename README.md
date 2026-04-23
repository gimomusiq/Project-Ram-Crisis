# RAM Dedupe Engine Prototype

This repository contains the first implementation step for a next-generation RAM deduplication engine.

## What is included

- `user/` — a user-space prototype that simulates page scanning, ML-assisted scan prioritization, duplicate detection, and copy-on-write semantics.
- `driver/` — a kernel-mode driver skeleton and design notes for a Windows WDK implementation.
- `docs/architecture.md` — architecture and design rationale for the deduplication engine.

## Build the user prototype

```powershell
cd user
mkdir build
cd build
cmake ..
cmake --build .
```

The repository now includes release readiness metadata such as `LICENSE`, `.gitignore`, and a kernel driver project scaffold in `driver/ram_dedupe.vcxproj`.

If `cmake` is not installed, you can compile directly with Visual Studio's `cl` or a compatible compiler:

```powershell
cd user
cl /std:c++17 /W4 /EHsc dedupe_simulator.cpp dedupe_engine.cpp ml_predictor.cpp scan_scheduler.cpp
```

Then run:

```powershell
.\dedupe_simulator.exe
```

To run the test harness:

```powershell
.\dedupe_engine_tests.exe
```

To use the controller for process opt-in once the driver is installed:

```powershell
.\dedupe_controller.exe optin <pid>
.\dedupe_controller.exe optout <pid>
.\dedupe_controller.exe stats
.\dedupe_controller.exe health
.\dedupe_controller.exe drain
.\dedupe_controller.exe list
```

To run a driver control integration test after installing the driver:

```powershell
.\dedupe_driver_test.exe
```

Driver install/uninstall scripts are available in `driver/` for test deployment:

```powershell
cd driver
.\install_driver.ps1
.\uninstall_driver.ps1
```

To run the benchmark harness after building:

```powershell
.\dedupe_benchmark.exe
```

## Validation, benchmarks, and packaging

The repository now includes a Windows CI workflow in `.github/workflows/windows-ci.yml` that builds the user prototype, runs tests, and exercises the benchmark harness. Release and driver packaging guidance are available in `docs/release.md`, driver signing support is available in `driver/sign_driver.ps1`, safety review notes are in `docs/safety_review.md`, and COW interception design is documented in `docs/cow_interception.md`.

### Benchmark results

- `user/benchmark.exe` measures user-mode scan performance and dedupe throughput.
- `user/driver_benchmark.cpp` is available for driver-level performance validation.
- Add measured memory savings, scan latency, and driver integration metrics here after running the benchmark harness.

### Demo

- A simple simulator or controller demo screenshot/video is encouraged to show the prototype in action.
- Capture output from `user/dedupe_simulator.exe`, `user/driver_test.exe`, or `user/dedupe_controller.exe` for repository media.

## Notes and known limitations

This repo starts with a safe, portable prototype. The next step is to extend the kernel driver skeleton into a real Windows WDK driver and add a user-mode controller/executable for process opt-in.

Known limitations:

- Copy-on-write interception registration is currently stubbed and treated as active; the driver does not yet handle real kernel page-fault write faults.
- Shared page promotion and COW-protected PFN tracking are present, but actual COW write handling is not implemented.
- The current prototype is suitable for design review and integration testing, but the kernel driver is not yet ready for production release.
- This repository is published as a prototype; the implementation scope is complete for the current design milestone.

## Future work

- Implement real kernel-mode copy-on-write page-fault interception in the Windows driver.
- Add real shared-page protection and COW fault handling prior to production deployment.
- Harden the release packaging and signing flow for signed Windows driver distribution.
- Add regression tests that explicitly validate COW page-fault handling, page identity, and opt-in safety.

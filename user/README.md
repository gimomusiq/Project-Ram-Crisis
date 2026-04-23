# RAM Dedupe User Prototype

This folder contains the user-space simulator and prototype components for the RAM deduplication engine.

## Overview

The user prototype demonstrates:
- page scanning and duplicate detection
- ML-guided scan prioritization
- simulated copy-on-write semantics
- process opt-in control and diagnostics

## Build

From the `user` folder:

```powershell
mkdir build
cd build
cmake ..
cmake --build .
```

Alternatively, compile without CMake using Visual Studio tools:

```powershell
cl /std:c++17 /W4 /EHsc dedupe_simulator.cpp dedupe_engine.cpp ml_predictor.cpp scan_scheduler.cpp controller.cpp
```

## Run

Run the simulator executable:

```powershell
.\dedupe_simulator.exe
```

To execute the test harness:

```powershell
.\dedupe_engine_tests.exe
```

To use the controller interface:

```powershell
.\dedupe_controller.exe optin <pid>
.\dedupe_controller.exe stats
.\dedupe_controller.exe list
```

To run the benchmark harness:

```powershell
.\dedupe_benchmark.exe
```

## Notes

This user-space layer is intended to validate deduplication behavior and scheduling logic before kernel-mode integration. It is not a production driver, but a prototype for the RAM memory system architecture.

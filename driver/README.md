# Windows Kernel Driver Skeleton

This folder contains the initial outline for a Windows kernel-mode deduplication driver.

## Goal

The eventual driver should:

- scan committed user pages in 4 KB chunks
- compute page signatures and detect duplicates
- merge identical pages into shared read-only pages
- handle copy-on-write on writes to shared pages
- support per-process opt-in policy
- expose diagnostics and control through a user-mode controller

## Current status

- A driver skeleton is implemented with `DriverEntry`, control device creation, symbolic link setup, and a page scanner thread stub.
- The driver includes a basic IOCTL for process opt-in, opt-out, runtime drain, health query, and a generic table for tracking opted-in PIDs.
- The driver now includes a first-pass page signature scan workflow that enumerates opted-in processes and computes signatures over committed user pages.
- The driver now tracks duplicate page signatures, stores duplicate PFN candidates, and promotes duplicate pages into shared dedupe groups for future COW handling.
- The driver maintains a runtime COW-protected page registry for shared page candidates and exposes this state via diagnostics.
- The driver health query includes shared page group counts and COW-protected page counts.
- The driver enforces runtime opt-in policy to reject system-critical image names and protected OS processes.
- The driver includes a shared-page group manager, a COW protected page registry, and a callback skeleton; callback registration is currently stubbed and treated as active.
- A new debug IOCTL exposes direct invocation of the COW callback stub for integration validation without requiring actual kernel fault interception.
- Current COW interception state is now reported as active via the stubbed callback registration path, while actual page-fault handling remains future work.
- The current driver skeleton is suitable for review and initial integration, but it is not yet a complete WDK release driver.
- This driver skeleton is published as a prototype implementation with explicit limitations documented.
- Next work: wire the shared-page COW registry into a kernel page-fault callback path so shared pages can be made truly copy-on-write.
- This folder now includes a WDK project scaffold at `driver/ram_dedupe.vcxproj`, a minimal install manifest in `driver/ram_dedupe.inf`, and installation/packaging scripts.

## Driver installation and validation

Use the following scripts from the `driver/` folder:

```powershell
.\install_driver.ps1
.\uninstall_driver.ps1
.\sign_driver.ps1 -CertificatePath <path> -CertificatePassword <password>
.\package_driver.ps1 -OutputFolder <path>   # generates package_manifest.json and release ZIP
.\create_signed_package.ps1 -CertificatePath <path> -CertificatePassword <password>
```

`install_driver.ps1` now installs the driver package and attempts to start the `ram_dedupe` service automatically after installation.

The packaging workflow now validates required build outputs, generates a package manifest, and produces a timestamped release ZIP.

To validate driver control once installed, use the test harness in `user/`:

```powershell
.\dedupe_driver_test.exe
```

This repository also builds and runs the driver integration test harness in CI using `dedupe_driver_test`, installing the test driver before validation and uninstalling it afterward.

The signed driver package should include the signed `ram_dedupe.sys`, `ram_dedupe.inf`, and release documentation.

## Signing requirements

- A valid code signing certificate or test-signing certificate is required for modern Windows driver distribution.
- Use `driver/sign_driver.ps1` with `signtool.exe` available on PATH.
- Use `driver/create_signed_package.ps1` to sign the driver and assemble the release ZIP.

## Next steps

1. Implement page enumeration and real page signature computation.
2. Add the deduplication manager with shared page tracking and reference counting.
3. Add page fault interception / copy-on-write handling using kernel memory manager callbacks.
4. Expand the opt-in control channel with diagnostics, safety checks, and process policies.

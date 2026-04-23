# Release and Packaging Guidance

This document describes how to package the RAM Dedupe Engine prototype and future driver artifacts.

## User Prototype Packaging

For the current user-space prototype, package the following:

- `user/build/Release/dedupe_simulator.exe`
- `user/build/Release/dedupe_engine_tests.exe`
- `user/build/Release/dedupe_controller.exe`
- `user/build/Release/dedupe_benchmark.exe`
- `user/README.md`
- `docs/architecture.md`
- `docs/vision.md`
- `docs/release.md`

A simple release package can be created with a ZIP archive containing the built executables and documentation.

## Driver Packaging

Future driver packaging should include:

- the signed `.sys` driver file
- a driver INF file describing the service installation
- a signed catalog file (`.cat`) if distributing an installable driver package
- a release README with installation and test-signing instructions

### Recommended Packaging Steps

1. Build the driver using the Windows Driver Kit (WDK) and the project file in `driver/ram_dedupe.vcxproj`.
2. Generate a driver package manifest or INF file for the driver service.
3. Use `driver/install_driver.ps1` to install the driver for test deployment.
4. Use `driver/uninstall_driver.ps1` to remove the driver and clean up the service.
5. Sign the driver with a valid code signing certificate using `driver/sign_driver.ps1`, or use test signing in development mode.
6. Use `driver/create_signed_package.ps1` to sign and package the driver binary, INF, and release README into a ZIP archive.

### Environment prerequisites

- A Windows host with the Windows Driver Kit (WDK) and `signtool.exe` available.
- A valid driver signing certificate or a test signing certificate.
- Administrative privileges to install and validate the driver package.

## Distribution Notes

- The user prototype is intended for validation and should be distributed as a portable ZIP bundle.
- The kernel driver requires signed binaries on modern Windows, so the distribution process should account for signing and driver installation policy.
- Include clear release notes describing the prototype status, known limitations, and safety considerations.

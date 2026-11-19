# Rust + Tauri migration (in progress)

Run `npm ci` then `npm run dev`. Rust and Tauri 2 system prerequisites are required.
Run Rust checks with `cargo test --manifest-path src-tauri/Cargo.toml`.

Implemented:
- Seven-stage Russian wizard: welcome, disk, installation mode, Windows size/image index, packages/drivers, computer settings, confirmation.
- Embedded package catalog and selection state across wizard navigation.
- Linux disk enumeration and partition role detection matching the C++ heuristics.
- Backend settings validation and post-install JSON compatible with the Linux launcher's CreatePostInstallConfig.
- Read-only GPT partition plan with AMDZ and adjacent service partition preservation, boundary checks, free partition-number allocation, fixed 120 GiB mode and manual size mode.
- Twelve Rust tests covering configuration, validation, partition roles, partition geometry, process execution and nested Ventoy detection.
- Rust modules for command execution, mount cleanup, live media discovery, WIM application, UEFI/BIOS boot configuration and payload staging (integration in progress).

Not yet ported:
- Installation execution: UEFI/Legacy commands, mounting, applying WIM, BCD, payload staging, progress, cleanup and reboot.
- Integration of live media discovery and deployment into the installation orchestrator.
- Windows target packaging and Linux Live integration verification.

The confirmation page previews configuration and partition geometry only. No destructive installation commands are exposed. Existing C++ and scripts remain the functional reference; this branch is not yet a feature-complete replacement.

Reference: https://v2.tauri.app/start/

## External components (retained unchanged)

`assets/PSetup.exe`, `bcd-sys-2.4-x86_64.AppImage` and `ms-sys` remain prebuilt external dependencies. They are copied or invoked by the Rust installer; rewriting their implementations is outside migration scope. PSetup continues to consume the existing post-install configuration format.

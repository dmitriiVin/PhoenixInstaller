# Rust + Tauri migration (in progress)

Run `npm ci` then `npm run dev`. Rust and Tauri 2 system prerequisites are required.
Run Rust checks with `cargo test --manifest-path src-tauri/Cargo.toml`.

Implemented:
- Seven-stage Russian wizard: welcome, disk, installation mode, Windows size/image index, packages/drivers, computer settings, confirmation.
- Embedded package catalog and selection state across wizard navigation.
- Linux disk enumeration and partition role detection matching the C++ heuristics.
- Backend settings validation and post-install JSON compatible with the Linux launcher's CreatePostInstallConfig.
- Read-only GPT partition plan with AMDZ and adjacent service partition preservation, boundary checks, free partition-number allocation, fixed 120 GiB mode and manual size mode.
- Nine Rust tests covering configuration, validation, partition roles and partition geometry.

Not yet ported:
- Installation execution: UEFI/Legacy commands, mounting, applying WIM, BCD, payload staging, progress, cleanup and reboot.
- Live media discovery and Windows setup runner.
- Windows target packaging and Linux Live integration verification.

The confirmation page previews configuration and partition geometry only. No destructive installation commands are exposed. Existing C++ and scripts remain the functional reference; this branch is not yet a feature-complete replacement.

Reference: https://v2.tauri.app/start/

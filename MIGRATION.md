# Migration status

The Tauri entry point is `src-tauri/src/main.rs`; the Rust installation orchestrator is `install.rs`. The application backend is Rust; the Tauri UI is HTML/CSS/JavaScript. The old C++ application, CMake build, ImGui renderer and C++ library submodules have been removed.

Implemented in Rust: disk enumeration/partition roles, settings and package validation, post-install configuration, GPT/AMDZ partition planning, Legacy/MBR creation, process execution/elevation, Ventoy/Data discovery, mount cleanup, WIM application, UEFI/BIOS boot configuration and verification, payload staging, reboot, async Tauri invocation and progress events. The Russian wizard supports all three installation modes, partition size, image edition, programs, drivers and computer name.

PSetup.exe, bcd-sys and ms-sys remain external binaries, as requested. PSetup's configuration format and staged files follow the previous Linux implementation.

Validation: Rust unit tests and local compilation; Linux CI workflow added. Remaining verification: disposable Linux Live VM runs for UEFI and Legacy, all modes, AMDZ at both disk ends, failures during WIM/copying, boot into the installed Windows and PSetup execution. These runtime scenarios cannot be verified on the macOS host.

Build the application with `npm run build` (see README.md). Historical C++ sources are available in Git history only.

# Windows Installer — Rust + Tauri

Linux Live installer for Windows. The application backend is Rust and the desktop interface uses Tauri 2 with local HTML/CSS/JavaScript.

## Build

Install Rust, Node.js and the Tauri 2 prerequisites for your platform. On Ubuntu 24.04:

```sh
sudo apt install libwebkit2gtk-4.1-dev libappindicator3-dev librsvg2-dev patchelf
npm ci
npm run dev
npm run build -- --no-bundle
```

The Linux runtime requires `lsblk`, `findmnt`, `swapon`, `pkexec` (unless run as root), `mount`, `umount`, `wimlib-imagex`, `mkfs.ntfs`, `mkfs.fat`, `sgdisk`, `sfdisk`, `wipefs`, `partprobe`, `udevadm`, `rsync` and `systemctl`.

Place the Windows ISO at `/Windows10.iso`. Boot Ventoy with its volume mounted under `/run/media/`; a sibling partition labelled Data must contain Drivers and Programs folders. Keep the resource directory produced by Tauri with the executable. PSetup.exe, BCD-SYS and MS-SYS are shipped unchanged as external binaries.

## Installation modes

- Reinstall Windows: reuse EFI and Windows partitions; format Windows and preserve Data.
- Full reinstall: create Windows at 120 GiB and allocate the remaining space to Data.
- Manual layout: select Windows size (minimum 100 GiB), allocating the remainder to Data.

UEFI layouts preserve AMDZ at either end of a GPT disk and an adjacent service partition of 64–192 MiB. Legacy uses MBR and rejects AMDZ for repartitioning. Geometry assumes 512-byte logical sectors; other sector sizes are rejected. Mounted target disks and active swap are rejected. WIM edition and payload resources are checked before disk modification. The installer reboots after successful deployment, boot verification and file staging.

## Validation

```sh
cargo test --locked --manifest-path src-tauri/Cargo.toml
node --check frontend/app.js
```

CI builds on Ubuntu. Local macOS builds can verify compilation and launch the wizard; disk installation requires Linux Live. End-to-end UEFI/Legacy installation must be validated in a disposable VM with representative disks before production use. The application backend is implemented entirely in Rust. The Tauri interface uses HTML/CSS/JavaScript. No C++ application sources, CMake build files or former C++ library submodules are included.

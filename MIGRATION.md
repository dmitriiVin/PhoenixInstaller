# Rust + Tauri migration (in progress)

Run `npm install` then `npm run dev`. Tauri 2 system prerequisites and Rust are required.

Implemented: Tauri application shell, embedded package catalog, read-only Linux disk enumeration.

Not yet ported: partition role detection, installation wizard and validation, UEFI/Legacy installation (all three modes), AMDZ preservation, image discovery/mounting/application, boot configuration, post-install payload/configuration, Windows setup runner, progress and cleanup. Existing C++ implementation and scripts remain the functional reference. This branch is not a feature-complete replacement and cannot perform installations yet.

Reference: https://v2.tauri.app/start/

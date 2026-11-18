# Horizon Prism Launcher fork

This repository is a fork of [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher)
that launches [Project Horizon](https://playhorizon.dev) — Minecraft Java 26.3
rewritten in Rust — instead of the Java client. Horizon itself is not
reimplemented here: Prism builds the command line, prepares the resources and
tracks the process.

## Branches

| Branch | Milestone |
|---|---|
| `horizon/m1` | Horizon instance type skeleton (`InstanceType=Horizon`, `horizon.json`) |
| `horizon/m3` | real `horizon-client` launch, account session on stdin |
| `horizon/m4` | automatic preparation with `horizon-prepare --check`/`--prepare` |
| `horizon/m2` | Horizon settings page (channel, pinned versions, fullscreen) |
| `horizon/m5` | Horizon mods page (`.hmod`/`.hnmod`, `horizon-mods.txt`) |
| `horizon/m6` | fork branding (`HORIZON_FORK=ON`), updater, CI |
| `horizon/m7` | Horizon version management (install, update, remove) |
| `horizon/latest` | M1 + M3 + M4 + M2 + M5 + M6 + M7 in order (default branch) |

## Status

Implemented and tested on macOS 26 (x86_64) with the real Horizon nightly
2026-11-13 binaries:

- resources prepared in 67 s (223 MB) on a cold start;
- `horizon-client` launched with the session on stdin, two frames rendered,
  exit 0;
- settings persisted in `horizon.json`;
- Horizon mods listed, toggled and added from the instance window, with
  `horizon-mods.txt` following the game's own rules (9 unit tests).

Native mods (`.hnmod`) are shown but not built into the game: this fork runs
a prebuilt `horizon-client`, so the page says so instead of pretending they
run.

The Horizon settings page can also open a version manager: it lists the
installed and installable Horizon releases through `horizon-launcher`, installs,
removes and checks for updates, and applies a version to the instance. It never
removes a version Horizon or another instance plays, and it leaves the
launcher's own update to Horizon.

GitHub Actions builds the fork with `HORIZON_FORK=ON` on macOS (universal
bundle), Ubuntu 24.04 and Windows 2022 (MSVC), runs the test suite (25 tests,
including `HorizonMod_test` and `HorizonRelease_test`) and uploads an unsigned
macOS Debug bundle as a workflow artifact. See `.github/workflows/horizon.yml`.

## Building with the fork identity

`-DHORIZON_FORK=ON` (or `fork/build-m6.sh` in the development repository)
builds `horizon-prism.app`, the `horizon-prism` binary, the
`dev.playhorizon.HorizonPrismLauncher` bundle id and a separate data folder,
so the fork and Prism can be installed side by side. The GitHub updater points
at this repository on Windows and Linux; Sparkle stays off on macOS because the
fork has no signing key yet. Artifacts are unsigned.

Not yet signed and not yet released. Windows and Linux are compiled and tested
in CI but have not been run by hand.

## License

Prism Launcher is licensed under GPL-3.0; this fork keeps the same license.
See [LICENSE](LICENSE).

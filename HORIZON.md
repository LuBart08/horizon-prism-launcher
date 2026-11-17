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
| `horizon/latest` | M1 + M3 + M4 + M2 in order (default branch) |

## Status

Implemented and tested on macOS 26 (x86_64) with the real Horizon nightly
2026-11-13 binaries:

- resources prepared in 67 s (223 MB) on a cold start;
- `horizon-client` launched with the session on stdin, two frames rendered,
  exit 0;
- settings persisted in `horizon.json`.

Not yet packaged, not yet upstreamed, and not yet tested on Windows or Linux.

## License

Prism Launcher is licensed under GPL-3.0; this fork keeps the same license.
See [LICENSE](LICENSE).

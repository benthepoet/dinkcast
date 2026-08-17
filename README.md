# Dinkcast

Dink Smallwood on the Sega Dreamcast (KallistiOS).

This repository is the engine and porting plan. It does **not** ship proprietary game media. Point the build at official freeware Dink and/or GNU FreeDink data with `DINK_DATA`.

- Port spec: [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md)
- **Progress + feasibility:** [PROGRESS.md](PROGRESS.md)
- Workflow: [AGENTS.md](AGENTS.md)
- Gotchas: [docs/GOTCHAS.md](docs/GOTCHAS.md)
- Toolchain: [docs/TOOLCHAIN.md](docs/TOOLCHAIN.md)

## License

Dinkcast is **free software** under the **GNU General Public License v3.0 or later** ([LICENSE](LICENSE)). SPDX: `GPL-3.0-or-later`.

That matches **GNU FreeDink** (GPLv3), whose interpreter we graft rather than replacing. Combined binaries that include FreeDink-derived code stay under the GPL.

**Game data is separate.** Keep it **outside** this repo. `dink.dat`, graphics, sounds, and `story/*.c` come from the user’s `DINK_DATA` tree and keep *their* licenses. Do not assume this repo’s GPL covers RTSoft or third-party assets.

GNU freedink-data tarball: use the inner `dink/` folder (the one with `Dink.dat`), not the tarball root. `make data-check` verifies the path (`local.mk` or `DINK_DATA=`).

KallistiOS and `dc-chain` are third-party toolchain pieces with their own licenses.

## Status

**V1 accepted:** official `tiles/Splash.bmp` on Flycast with a real BIOS (`make docker-cdi` + `make emu`). **4.1–4.2** leave-title is in this PR. Next engine bite after merge is **5.1**. Next *picture* gate is **V2 (6.3 tiles)**.

## Dreamcast toolchain

Full steps: [docs/TOOLCHAIN.md](docs/TOOLCHAIN.md).

```bash
make docker-cdi   # ELF + CDI via KOS Docker image (needs dockerd)
make emu
```

Without Docker, `source $KOS_BASE/environ.sh && make dc && make cdi`.
Without `KOS_BASE` or Docker, `make dc` exits 2.

```bash
make title-preview   # needs DINK_DATA; writes build/title_preview.ppm
```

Host still: `make title-preview` → `build/title_preview.ppm`.

## Checks

```bash
make host
```

**Emulator:** Flycast + **`dc_boot.bin`** in `~/.local/share/flycast/`. `make emu` opens `build/dinkcast.cdi`. REIOS often will not boot this CDI. Real hardware + `dcload` is still the ship check.

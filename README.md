# Dinkcast

Dink Smallwood on the Sega Dreamcast (KallistiOS).

This repository is the engine and porting plan. It does **not** ship proprietary game media. Point the build at official freeware Dink and/or GNU FreeDink data with `DINK_DATA`.

- Port spec: [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md)
- **Progress + feasibility:** [PROGRESS.md](PROGRESS.md)
- Releases: [CHANGELOG.md](CHANGELOG.md) (`VERSION` / git tag `v0.2.0`)
- Workflow: [AGENTS.md](AGENTS.md)
- Gotchas: [docs/GOTCHAS.md](docs/GOTCHAS.md)
- Toolchain: [docs/TOOLCHAIN.md](docs/TOOLCHAIN.md)
- Canvases (Cursor): [docs/canvases/](docs/canvases/)

## License

Dinkcast is **free software** under the **GNU General Public License v3.0 or later** ([LICENSE](LICENSE)). SPDX: `GPL-3.0-or-later`.

That matches **GNU FreeDink** (GPLv3), whose interpreter we graft rather than replacing. Combined binaries that include FreeDink-derived code stay under the GPL.

**Game data is separate.** Keep it **outside** this repo. `dink.dat`, graphics, sounds, and `story/*.c` come from the user’s `DINK_DATA` tree and keep *their* licenses. Do not assume this repo’s GPL covers RTSoft or third-party assets.

GNU freedink-data tarball: use the inner `dink/` folder (the one with `Dink.dat`), not the tarball root. `make data-check` verifies the path (`local.mk` or `DINK_DATA=`).

KallistiOS and `dc-chain` are third-party toolchain pieces with their own licenses.

## Status

**v0.2.0** (2026-08-22). Campaign DinkC host grafts (`goto`, `spawn`, `load_screen` / `draw_screen`, `screenlock`, `sp_target` / `sp_follow`, `get_sprite_with_this_brain`) plus village playtest leftovers (HUD paper, wizard idle, AlkNuts). Visual gates **V1–V6** and **8.6 house** stay accepted. Spec: [docs/V0.2.md](docs/V0.2.md). Audio, VMU, and **14.6** are not in this tag. Pictures: [docs/PLAYTEST.md](docs/PLAYTEST.md).

## Dreamcast toolchain

Full steps: [docs/TOOLCHAIN.md](docs/TOOLCHAIN.md).

```bash
make docker-cdi   # ELF + CDI + CHD via KOS Docker image (needs dockerd + chdman)
make emu          # Flycast on the CHD; serial also in build/emu.log
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

**Emulator:** Flycast + **`dc_boot.bin`** in `~/.local/share/flycast/`. `make emu` opens `build/dinkcast.chd`. REIOS often will not boot. Real hardware + `dcload` / burned CDI is still the ship check. Needs **chdman** (`pacman -S mame-tools`).

# Dinkcast

Dink Smallwood on the Sega Dreamcast (KallistiOS).

This repository is the engine and porting plan. It does **not** ship proprietary game media. Point the build at official freeware Dink and/or GNU FreeDink data with `DINK_DATA`.

- Port spec: [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md)
- **Progress:** [PROGRESS.md](PROGRESS.md)
- Contributor / agent workflow: [AGENTS.md](AGENTS.md)

## License

Dinkcast is **free software** under the **GNU General Public License v3.0 or later** ([LICENSE](LICENSE)). SPDX: `GPL-3.0-or-later`.

That matches **GNU FreeDink** (GPLv3), whose interpreter we graft rather than replacing. Combined binaries that include FreeDink-derived code stay under the GPL.

**Game data is separate.** Keep it **outside** this repo. `dink.dat`, graphics, sounds, and `story/*.c` come from the user’s `DINK_DATA` tree and keep *their* licenses. Do not assume this repo’s GPL covers RTSoft or third-party assets.

GNU freedink-data tarball: use the inner `dink/` folder (the one with `Dink.dat`), not the tarball root. `make data-check` verifies the path (`local.mk` or `DINK_DATA=`).

KallistiOS and `dc-chain` are third-party toolchain pieces with their own licenses.

## Status

Bite **0.1** skeleton + Bite **0.2** color-field source (`src/main.c`). ELF needs KallistiOS.

**First screenshot (do not skip):** plan **Bite 3.4** — official title still at 640×480.

## Dreamcast toolchain

1. Install [dc-chain](https://github.com/KallistiOS/KallistiOS) / `sh-elf-gcc` and KallistiOS.
2. `source $KOS_BASE/environ.sh`
3. `make dc` → `build/dinkcast.elf`
4. `make emu` once a `.cdi` exists (or point Flycast at the ELF).

Without `KOS_BASE`, `make dc` exits 2.

```bash
make title-preview   # needs DINK_DATA; writes build/title_preview.ppm
```

That is Bite **3.4** on the host: official `tiles/Splash.bmp` (640×480). DC/Flycast still needs KallistiOS.

## Checks

```bash
make host
```

**Emulator:** [Flycast](https://github.com/flyinghead/flycast) (`make emu` / `make run`). Install a `flycast` binary or Flatpak `org.flycast.Flycast`. Override with `FLYCAST=` or `EMU=`. There is no CDI/ELF until Bite 0.2, so `make emu` exits 2 today. Real hardware + `dcload` is still the ship check.

## Remote

No `origin` is configured:

```bash
git remote add origin git@HOST:USER/dinkcast.git
git push -u origin master
```

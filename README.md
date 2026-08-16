# Dinkcast

Dink Smallwood on the Sega Dreamcast (KallistiOS).

This repository is the engine and porting plan. It does **not** ship proprietary game media. Point the build at official freeware Dink and/or GNU FreeDink data with `DINK_DATA`.

- Port spec: [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md)
- Contributor / agent workflow: [AGENTS.md](AGENTS.md)

## License

Dinkcast is **free software** under the **GNU General Public License v3.0 or later** ([LICENSE](LICENSE)). SPDX: `GPL-3.0-or-later`.

That matches **GNU FreeDink** (GPLv3), whose interpreter we graft rather than replacing. Combined binaries that include FreeDink-derived code stay under the GPL.

**Game data is separate.** `dink.dat`, graphics, sounds, and `story/*.c` come from the user’s `DINK_DATA` tree and keep *their* licenses. Do not assume this repo’s GPL covers RTSoft or third-party assets.

KallistiOS and `dc-chain` are third-party toolchain pieces with their own licenses.

## Status

Spec + workflow + license. No Dreamcast ELF yet.

**First screenshot (do not skip):** plan **Bite 3.4** — official title still at 640×480.

**First implementation PR after kickoff:** **Bite 0.1** (repo/`Makefile` already has `make host`; 0.1 is the remaining skeleton). Then **0.2** color field (`KOS_BASE`). Workflow: [AGENTS.md](AGENTS.md) — feature branch, named orchestrator, reviews on the PR.

## Checks

```bash
make host
```

`make dc` is supposed to fail until Bite 0.2 (`KOS_BASE` + `src/`).

**Emulator:** [Flycast](https://github.com/flyinghead/flycast) (`make emu` / `make run`). Install a `flycast` binary or Flatpak `org.flycast.Flycast`. Override with `FLYCAST=` or `EMU=`. There is no CDI/ELF until Bite 0.2, so `make emu` exits 2 today. Real hardware + `dcload` is still the ship check.

## Remote

No `origin` is configured:

```bash
git remote add origin git@HOST:USER/dinkcast.git
git push -u origin master
```

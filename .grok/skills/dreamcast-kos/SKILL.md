---
name: dreamcast-kos
description: >
  Dreamcast homebrew with KallistiOS: SH-4, PowerVR2, AICA, CDI/ISO9660,
  Docker KOS images, Flycast. Use when building or debugging Dinkcast,
  writing KOS C, packing a .cdi, PVR textures, /cd paths, or the user
  says /dreamcast-kos, "Dreamcast", "KallistiOS", "KOS", "CDI", "Flycast",
  "PVR", or "SH-4".
---

# Dreamcast / KallistiOS

Read [docs/GOTCHAS.md](../../../docs/GOTCHAS.md) before changing FS, CDI packing, PVR upload, Docker/`make dc`, or boot. If you learn a **new class** of failure, add one bullet there (rule + wrong vs right). Do not restage a war story.

Canon for this repo: [DREAMCAST-PORT-PLAN.md](../../../DREAMCAST-PORT-PLAN.md), [docs/TOOLCHAIN.md](../../../docs/TOOLCHAIN.md), [AGENTS.md](../../../AGENTS.md).

## Defaults

- Retail DC: 16 MB RAM, 8 MB VRAM, 2 MB AICA. Stream; do not preload the world.
- Video: 640×480 RGB565. Sprites/tiles = **PowerVR textured quads**, not CPU blit.
- Play input: Maple **controller only**.
- 60 Hz logic; 30 FPS is the floor, not the design.
- Screen swap: cache tilesets. Same-set **0.2–0.6 s** on CD; new set **0.5–2 s**. Every-screen “Loading…” is a pack/evict bug (plan + GOTCHAS). Flycast will look instant.
- Data: `DINK_DATA` is the inner `dink/` tree (has `Dink.dat`). Never commit game blobs.
- SH-4 ELF must not contain host paths. Runtime roots: `/pc/dink`, `/cd/dink`, `/cd/DINK`, `/cd`.
- `printf` is SCIF serial. Flycast's log is **not** that. On-screen status = `bfont` / HUD.

## Build

- Host: `make host`.
- ELF+CDI: `make docker-cdi` (daemon must run; user in `docker` group). Image: `KOS_DOCKER_IMAGE` or default in `tools/docker_kos.sh`.
- Flycast: `make emu`. Prefer **real `dc_boot.bin`** in `~/.local/share/flycast/`. REIOS often never reaches `1ST_READ.BIN`.
- After a red/stripe/boot fail: read the framebuffer HUD first, then GOTCHAS. Spawn the **troubleshooting team** in AGENTS.md (≥2 specialists + debug orchestrator). Do not solo-guess.
- **Visual gates:** do not proceed past 3.4 / 6.3 / 8.4 / 9.3 / 13.2 / 16.x until the human accepts that picture.

## PVR textures

- `pvr_txr_load_ex` **always twiddles**. Poly format must be twiddled RGB565 (no `PVR_TXRFMT_NONTWIDDLED` unless you used a nontwiddle load).
- Texture size power-of-two; pad 640×480 into 1024×512; UVs = `w/tw`, `h/th`.
- 640×480 RGB565 title = 614400 B.

## ISO9660 / CDI

- `mkdcdisc -d DIR` names the on-disc folder **`basename(DIR)`**. Pass the `dink` tree, not `build/iso`.
- Disc names are often `DINK.DAT`. Probe case-insensitively; `stat`+`S_ISDIR` is unreliable — `opendir` / open `dink.dat`.
- Wait for GD-ROM (`cdrom_get_status` PAUSED/STANDBY) before the first `/cd` access.
- A data root is valid only if `dink.dat` actually opens.

## Do not

- `-DDINK_DATA_DEFAULT=\"$HOST_PATH\"` through `kos-cc` (quotes/backslashes).
- Invent DinkC or a “faster” interpreter — graft FreeDink. SH-4 can run it; don’t JIT.
- Skip Bite 3.4 (official still) to start gameplay.
- Claim Flycast boot without saying BIOS vs REIOS.

# Dreamcast / Dinkcast gotchas

Append **one bullet per class of mistake** (rule + wrong vs right). Not a changelog. Agents must read this before touching FS, CDI, PVR, Docker, or boot. The skill [dreamcast-kos](../.grok/skills/dreamcast-kos/SKILL.md) points here.

## Toolchain and Docker

- **Docker installed ≠ daemon usable.** Need `systemctl start docker` and the user in group `docker` (or `sudo docker`). No socket → no pull/build.
- **Do not bake host `DINK_DATA` into the SH-4 compile.** `-DDINK_DATA_DEFAULT=\"/home/...\"` breaks `kos-cc` (stray `\` / unclosed quote) and is meaningless on the DC. Probe `/pc/dink`, `/cd/*`, `getenv` only.
- **`local.mk` wins over container env.** Host path is invisible inside Docker. Use `make -e cdi` and/or fall back to `/dink` (the mount). Prefer `DINK_DATA ?=` in `local.mk`.
- **`make docker-cdi` must live on `master`.** A target only on a docs branch is `No rule to make target`.
- **Fine-grained `github_pat_` can open PRs and push branches but still 403 `mergePullRequest`.** That is a token permission, not a broken PR. Human merges in the GitHub UI. Never “finish” a PR by rewriting `master`.
- **REIOS ≠ a BIOS.** Flycast without `dc_boot.bin` in `~/.local/share/flycast/` often never runs `1ST_READ.BIN`.
- **Flycast `N[RENDERER]` is not KOS `printf`.** SH-4 serial is SCIF. `make emu` passes `-config config:Debug.SerialConsoleEnabled=yes` so `printf` mixes into the same terminal. Settings → Advanced → Serial Console does the same.

## Disc and `/cd`

- **`mkdcdisc -d DIR` uses `basename(DIR)` as the folder on the disc.** `-d build/iso` → `/cd/iso/...`. Pass the data tree named `dink` → `/cd/dink`.
- **ISO9660 is 8.3 / often `DINK.DAT`.** `stat("/cd/dink")` can fail; `S_ISDIR` is untrustworthy. `opendir` and open `dink.dat` (case-insensitive). Also try `/cd`, `/cd/DINK`, leftover `/cd/iso`.
- **Empty directory is not a data root.** Only accept a root if `dink.dat` opens.
- **GD-ROM is not ready on the first line of `main`.** Wait for `cdrom_get_status` PAUSED/STANDBY/PLAYING before probing `/cd`.
- **On-screen HUD beats serial.** Use `bfont_draw_str` on `vram_s` so a red screen states `NO DATA ROOT` and lists `/cd`.

## PVR / video

- **`pvr_txr_load_ex` always twiddles.** Drawing with `PVR_TXRFMT_NONTWIDDLED` yields horizontal stripes of the right colors. Poly = twiddled `PVR_TXRFMT_RGB565`.
- **Brown then stripes = decode OK, upload/format wrong.** Red HUD = never found data (or ELF never ran).
- **640×480 is not a texture size.** Pad to 1024×512; sample with UVs `640/1024`, `480/512`.

## Screen changes

- **Flycast load time is not hardware.** Host disk hides seeks. Judge `swap_ms` on a CDI/ODE/burn, not only `make emu`.
- **A loading screen on every edge is a bug.** Keep the current tileset in VRAM. Same-tileset walk: **0.2–0.6 s** on CD. New tileset: **0.5–2 s**. **> 3 s** on a normal neighbor = over-evict or disc order. Title → first map may show “Loading…”.
- **Do not reopen music** unless the screen’s MIDI id changed; that adds another seek.

## DinkC

- **SH-4 is fast enough.** Do not invent a VM or JIT “because 200 MHz.” Graft FreeDink; parse once; table-dispatch; cap ops per frame. Busy loops are a script bug (same as PC).
- **A hitch is not proof scripts are hot.** Profile GD-ROM and texture upload first. Reliability is `wait` / `say_stop` fibers and command coverage, not clock rate.

## Data

- **GNU `freedink-data` tarball root is not `DINK_DATA`.** Use the inner `dink/` (`Dink.dat`, `Tiles/`, `Story/`).
- **Keep that tree outside the git repo.**
- **Original `Ts01.bmp` is 400×400 (8×8 cells).** FreeDink still uses a 12-wide cell index; slot 30 is empty. Start-screen tile 30 is black; house floor is sprites.
- **Dink idle frames are `graphics/Dink/idle/dir.ff`.** White is sprite colorkey. Pack ARGB1555 with A=0 on white. Draw on the **punch-through** list (`PVR_LIST_PT_POLY`) with PT binsize 16. The TR list still writes A=0 as **black** (the idle slab). `pvr_init_defaults` often leaves PT bins at 0.
- **`pvr_mem_malloc` only after `pvr_init`.** Title `pvr_shutdown` on leave. First play upload is `tiles_upload_pvr` (`pvr_init` + PT bins). Decode BMP/`.ff` to RAM first; do not upload editor sprites before that. Assert `pvr_mem_base != NULL` is this, not a VRAM leak.

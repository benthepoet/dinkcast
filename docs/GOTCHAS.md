# Dreamcast / Dinkcast gotchas

Append **one bullet per class of mistake** (rule + wrong vs right). Not a changelog. Agents must read this before touching FS, CDI, PVR, Docker, or boot. The skill [dreamcast-kos](../.grok/skills/dreamcast-kos/SKILL.md) points here.

## Toolchain and Docker

- **Docker installed ≠ daemon usable.** Need `systemctl start docker` and the user in group `docker` (or `sudo docker`). No socket → no pull/build.
- **Do not bake host `DINK_DATA` into the SH-4 compile.** `-DDINK_DATA_DEFAULT=\"/home/...\"` breaks `kos-cc` (stray `\` / unclosed quote) and is meaningless on the DC. Probe `/pc/dink`, `/cd/*`, `getenv` only.
- **`local.mk` wins over container env.** Host path is invisible inside Docker. Use `make -e cdi` and/or fall back to `/dink` (the mount). Prefer `DINK_DATA ?=` in `local.mk`.
- **`make docker-cdi` must live on `master`.** A target only on a docs branch is `No rule to make target`.
- **Makefile.dc must list header deps.** `kos-cc` incremental builds only rebuild changed `.c`. After `EditorSprite` grew warp fields, `script.o`/`talk.o` stayed on the 60-byte layout. House drew (new `main.o`) but `dinkc preload unique=0` and A found no scripts. `$(OBJS): src/mapscr.h`.
- **Fine-grained `github_pat_` can open PRs and push branches but still 403 `mergePullRequest`.** That is a token permission, not a broken PR. Human merges in the GitHub UI. Never “finish” a PR by rewriting `master`.
- **REIOS ≠ a BIOS.** Flycast without `dc_boot.bin` in `~/.local/share/flycast/` often never runs `1ST_READ.BIN`.
- **Flycast `N[RENDERER]` is not KOS `printf`.** SH-4 serial is SCIF. `make emu` enables SerialConsole and tees combined stdout to the terminal **and** `build/emu.log` (override `EMU_LOG=`). Each run overwrites the file. Settings → Advanced → Serial Console does the same.

## Disc and `/cd`

- **`mkdcdisc -d DIR` uses `basename(DIR)` as the folder on the disc.** `-d build/iso` → `/cd/iso/...`. Pass the data tree named `dink` → `/cd/dink`.
- **ISO9660 is 8.3 / often `DINK.DAT`.** `stat("/cd/dink")` can fail; `S_ISDIR` is untrustworthy. `opendir` and open `dink.dat` (case-insensitive). Also try `/cd`, `/cd/DINK`, leftover `/cd/iso`.
- **Empty directory is not a data root.** Only accept a root if `dink.dat` opens.
- **GD-ROM is not ready on the first line of `main`.** Wait for `cdrom_get_status` PAUSED/STANDBY/PLAYING before probing `/cd`.
- **On-screen HUD beats serial.** Use `bfont_draw_str` on `vram_s` so a red screen states `NO DATA ROOT` and lists `/cd`.

## SH-4 stack

- **KOS thread stack is 32–64 KB.** `World` (9 KB) + `MapScreen` (7 KB) + `HardMap.btile_default` (10 KB) + `EdGfx[96]` (6 KB) as **locals** smash the stack. Serial then looks like a parse bug (`edraw skip seq=388` — that 388 is a **Y**). Put those in BSS (`static`) or heap. Host `make host` will not catch this.
- **SH-4 int32 must be 4-aligned.** `uint8_t active` + `char script[14]` can make `sizeof(EditorSprite) % 4 != 0` so `sprite[i].seq` is a misaligned load (garbage seq=y). Use int32 `active` and a 16-byte script tail. Snapshot sprites **immediately after `map_load_record`** (BSS copy).
- **`edraw_load_screen` must heap-copy sprites before `memset(g_edg)`.** ELF packs `g_edg` → `g_hard` → `g_atlas` → `g_scr`. Free+memset first, then copy, reads `act=88` (Y). A second BSS `static` table can sit next to `g_edg` and die the same way. `malloc`, copy, then memset; write the snap back at the end.
- **Restore `g_scr.sprite` after edraw before `script_on_main` / talk.** Draw already memcpy’d the snap; attach and `talk_probe` still walked the smashed table (`preload unique=0`, no mom `main`, A does nothing). Keep the snap on the **heap**, not BSS next to `g_edg`.
- **Do not BSS `EdGfx[96]` next to `g_scr`.** `edraw.c` and `main.c` can disagree on `sizeof(EdGfx)` if only `main` includes `kos.h`. `memset(g)` then walks into the map table. Serial: `restore … script=s1-h1-m` then `preload unique=0`. Allocate gfx with `edraw_gfx_alloc()`; restore again immediately before attach.

## PVR / video

- **`pvr_txr_load_ex` always twiddles.** Drawing with `PVR_TXRFMT_NONTWIDDLED` yields horizontal stripes of the right colors. Poly = twiddled `PVR_TXRFMT_RGB565`.
- **Brown then stripes = decode OK, upload/format wrong.** Red HUD = never found data (or ELF never ran).
- **640×480 is not a texture size.** Pad to 1024×512; sample with UVs `640/1024`, `480/512`.
- **`pvr_wait_ready` before `pvr_mem_free` of a texture still in the last scene.** Walk/idle frames share a similar `cx`; punch (`ds-h*`, cx ~58) then idle (`ds-i*`, cx ~36) does not. Evict after `scene_finish` but before the next wait and the GPU still samples the old quad — idle pixels appear left of Dink. Wait, then swap.
- **`pvr_shutdown` on leave-title leaves the splash in the last PVR frame.** Flycast keeps showing it while we load `map.dat` / editor BMPs (serial already says `leave_title`). Re-`pvr_init` and draw a brown clear immediately, then load. Do not wait until `tiles_upload_pvr`.

## Screen changes

- **Flycast load time is not hardware.** Host disk hides seeks. Judge `swap_ms` on a CDI/ODE/burn, not only `make emu`.
- **A loading screen on every edge is a bug.** Keep the current tileset in VRAM. Same-tileset walk: **0.2–0.6 s** on CD. New tileset: **0.5–2 s**. **> 3 s** on a normal neighbor = over-evict or disc order. Title → first map may show “Loading…”.
- **Do not reopen music** unless the screen’s MIDI id changed; that adds another seek.

## DinkC

- **SH-4 is fast enough.** Do not invent a VM or JIT “because 200 MHz.” Graft FreeDink; parse once; table-dispatch; cap ops per frame. Busy loops are a script bug (same as PC).
- **A hitch is not proof scripts are hot.** Profile GD-ROM and texture upload first. Reliability is `wait` / `say_stop` fibers and command coverage, not clock rate.
- **DinkC files are `story/NAME.c`.** `map.dat` has no extension. Over **32 KB** fails that script only (log `dinkc too big`). Never skip a missing file silently (`dinkc miss`). `dink_fopen` already covers `STORY/NAME.C` / 8.3.
- **Start house screen script is empty.** Official loc 1 `script=` is blank; `s1-h1-m` / `s1-h1-1` live on **sprites**. `main script_id=0 script=` after leave-title is not a parse miss. Preload unique on-vision sprite files.
- **DinkC has no `/* */`.** FreeDink is line + `get_word`. Stock `story/` uses `//` only. Lexer skips `//` to EOL. `/*` is `SLASH` then `STAR`, not a block comment. `&s2-map` is one `VAR` (hyphen is ident).
- **Stock DinkC is messy.** Missing `{` after `void name()`, extra `}`, `+=` and `&x / 5`, `choice_start`/`title_start` prose, multiline `"…"`, typos (`wait(200:`, `say_stop('…"`). Parser is permissive; it does not execute.
- **`wait(ms)` is a fiber yield**, not a busy loop. 60 Hz tick is 16 ms. `say_stop` yields until A. Do not attach/run house `main()` until 11.5/11.6 — it `freeze`s and `say_stop`s.
- **DinkC 1.08 looks up locals before globals.** Unknown `&name` is 0. Engine names `&current_sprite` / `&current_script` / `&return` / `&arg1`–`9` are not `play.var` slots. Only register `MAIN.c` `make_global_int` names — do not invent extras.
- **A while `choice_start` picks the first visible line** until Bite 13 draws the menu. `&result` is FreeDink `line_return` (hidden lines still increment). 0 visible lines do not yield. Serial `say` is the 11.5 box. Do not `freeze++` in the engine on talk — `freeze(1)` in the script is the nest.
- **`playmidi` / `draw_status` are implemented no-ops** until 12 / 16. Do not leave them as `unimplemented` or mom `main` looks broken. `fade_*` / `load_screen` do not yield (no fade clock, no 14 warp).
- **`DINKC_DUMP_FNS=1`** prints `dinkc dump implemented=` at bind/atexit and after screen attach. Missing names are unique first-seen calls not in `k_fn[]`. Add the name to the table (even as a stub) instead of a second if-chain.
- **Official freeware data has no dialog font BMP.** Original Dink used GDI (Arial-like); FreeDink uses LiberationSans TTF. Do not ship TTF on the DC. 13.1 is an embedded public-domain IBM VGA 8×8 atlas (16 KB ARGB1555), not LiberationSans. Say box is 13.2.
- **`say` is sprite-anchored, not a HUD bar.** FreeDink `say_text`: `x = owner.x-75`, `y = owner.y-100` (default defense), wrap 150, `` `X `` color prefix. A **or** B advances `say_stop`. Do not punch on B while a line is up.
- **`freeze` is not a nest.** FreeDink sets `spr[1].freeze` to the script id (any non-zero). `unfreeze` clears it. If the fiber dies before `unfreeze`, Dink stays locked — thaw when `dinkc_vm_live()==0`.
- **Choice menu is D-pad + A.** Visible lines only; `&result` is the official number (hidden lines still count). A picks the highlighted row, not always 1.
- **Do not `start_proc(talk)` on a new fiber each A.** FreeDink `locate`s `talk()` on the sprite’s existing script. A new instance leaks slots and can leave a stale `WAIT_CHOICE`/`wait`. Kill that sprite’s fibers first. `wait` uses the engine clock (`g_now_ms`), not the fiber’s last `wait_until`.
- **House exit is a warp, not an edge.** Hardness `100+editor` + `is_warp` (`special_block`). Edge is `did_player_cross_screen` (playl=20, x=619, y=0/399, map ±1 / ±32). Clamp if `loc[neighbor]==0`. Instant swap (no fade).
- **Do not `tiles_evict` before the new atlas is uploaded.** Swap that frees tile PVR first leaves the last house frame (Dink in the doorway) on screen. If stamp/atlas/upload then stalls, it never draws map 439. Free CPU atlas only; replace PVR in `tiles_upload_pvr`. Serial: `warp … enter map 439` then `edraw unique 16` and silence.
- **Do not brown-clear every screen swap.** Plan: a loading screen on every edge is a bug. `tiles_draw_clear_pvr` after the house door left a stuck brown field while `hard.dat` (~2 MiB) reloaded. Keep the previous tiles on screen until the new atlas uploads.
- **Do not reopen `hard.dat` (~2 MiB) on every screen swap.** `/cd` fread of that file a second time hangs (`swap hard load` then silence; house tiles stay on screen). Load once, keep `raw` for restamp. Unique 6 with garbled `edraw slot` was smash/stale `.o`, not only the 2 MiB.
- **Attach is not “every script field.”** FreeDink `draw_screen_game` runs the screen `script` `MAIN` first (`strlen>1`). `game_place_sprites` only `load_script`s **type 1** on-vision sprites with `strlen(script)>1`. `game_screen_init_scripts` then `locate("main")`. Type 0/2 names stay unused. Mom `main` `freeze`+`say` when `&story==0` is intended.

## Data

- **GNU `freedink-data` tarball root is not `DINK_DATA`.** Use the inner `dink/` (`Dink.dat`, `Tiles/`, `Story/`).
- **Keep that tree outside the git repo.**
- **Original `Ts01.bmp` is 400×400 (8×8 cells).** FreeDink still uses a 12-wide cell index; slot 30 is empty. Start-screen tile 30 is black; house floor is sprites.
- **Dink idle frames are `graphics/Dink/idle/dir.ff`.** White is sprite colorkey. Pack ARGB1555 with A=0 on white. Draw on the **punch-through** list (`PVR_LIST_PT_POLY`) with PT binsize 16. The TR list still writes A=0 as **black** (the idle slab). `pvr_init_defaults` often leaves PT bins at 0.
- **`pvr_mem_malloc` only after `pvr_init`.** Title `pvr_shutdown` on leave. First play upload is `tiles_upload_pvr` (`pvr_init` + PT bins). Decode BMP/`.ff` to RAM first; do not upload editor sprites before that. Assert `pvr_mem_base != NULL` is this, not a VRAM leak.
- **One `map.dat` screen holds several visions.** Start house: vision **0** intact, **1** fire/`explo-`, **2** holes. `&vision` starts at 0. Draw `vision==0 || vision==current`; type **2** is hardness-only (`game_place_sprites`). Drawing every `active` stacks the burned house on the intact one.
- **Walls use `SET_SPRITE_INFO`, not the `load_sequence` line.** Seq 31 is `NOTANIM` (cx/cy/hard 0). FreeDink `program_idata` applies per-frame xoffset/yoffset/hardbox (last line wins). Guessing a center or skipping `hr<=hl` at seq level makes a broken house and walk-through walls. `hard==0` is solid.
- **No idle seq on diagonals.** `human_brain` snaps 1/3→2 and 7/9→8 before `base_idle+dir`. Seq 11/13/17/19 are empty; load fail after `sprite_frame_free` makes Dink vanish. Snap first; only evict the old frame after the new one loads.
- **Player move is a point, not a box.** `check_if_move_is_legal` / `get_hard(x-20, y)`. Seq hardboxes are stamped with `add_hardness` exclusive ranges. Testing the player’s walk hardbox as a rectangle wedges Dink in the table and walls. 3 px jumps without 1 px tests also embed you.
- **Depth is `que` if nonzero, else `y`.** `screen_rank_*`. **Do not draw at `que`** — that is a sort key (plate `que=248`, `y=207`). Drawing at `que` throws sprites off the playfield.
- **Talk is `run_through_tag_list_talk`, not a ray.** First slot 1–99 whose hardbox+10, stretched 50/35 in facing dir, contains Dink. Skip brain 8. A `freeze++` with no DinkC `unfreeze` leaves Dink stuck until 11.3 — that is the nest, not a walk bug.
- **Hit is not talk-range.** `run_through_tag_list` inflates hardbox 5/5/5/10 then 28/36 in facing (range 0). `SET_FRAME_SPECIAL` marks the punch frame (fists 102/4/6/8 frame 3). B starts `base_attack+dir` (100) and `nocontrol` until the seq ends — do not treat B as an instant ray.

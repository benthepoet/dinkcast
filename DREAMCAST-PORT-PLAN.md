# Dink Smallwood → Sega Dreamcast Port Plan (granular)

**Target:** Sega Dreamcast (retail, 16 MB main RAM)  
**Source game:** Dink Smallwood (Robinson Technologies, v1.07 / v1.08 behavior via GNU FreeDink)  
**Homebrew stack:** KallistiOS via **`make docker-cdi`** (default) or native `dc-chain` + `source $KOS_BASE/environ.sh && make dc`. Pack with `mkdcdisc` (CDI for hardware). Iterate in **Flycast** (`make emu`) on the **MIL-CD CHD** (`build/dinkcast.chd`, from a CUE — not a GDI) with a **real `dc_boot.bin`**. DiscJuggler CDI is flaky in Flycast; a GDI CHD is rejected (needs 3 GD-ROM tracks). `dcload` / burned CDI remains the ship check.

**Emulator (binding):** **Flycast** + real BIOS, image = **CHD**. REIOS often never runs `1ST_READ.BIN`. Flycast’s log is not KOS `printf`.

**Where we are:** Tagged **v0.3.0** (2026-08-23). START menu + VMU **17**. **V6** + **8.6 house** accepted. Audio **12** and **14.6** wait for requester go. Next engine bite only when the requester says.

**Companions (do not fork facts):** landed work + **feasibility %** → [PROGRESS.md](PROGRESS.md); CDI/PVR/Docker mistakes → [docs/GOTCHAS.md](docs/GOTCHAS.md); **FreeDink field-by-field** → [docs/FREEDINK-ALIGN.md](docs/FREEDINK-ALIGN.md); agent rules → [.grok/skills/dreamcast-kos/SKILL.md](.grok/skills/dreamcast-kos/SKILL.md).

**How to use this file:** remaining bites in **implementation order** (ids stay stable): **10 → 11 → 13 → 14.1–14.2 → 15.1 → 11.10 → 15.2 → 14.4a → 14.4b → 14.5 (if `needed`) → 14.4c → 14.3 → 15.3–15.4 → 16 → 12 → 17 → 18 → 14.6**. Distill does **not** replace the eviction policy. **14.4c** is `cpu_pixels` / `EdGfx` Always/Screen/Sticky **class** eviction (not recency, not seq-id ranges). 14.4b packs are Always/Screen/Prev; Prev is packs only, not pixels. **14.3** after **14.4c** (14.4b green or 14.5 disc is not enough while play-path still guesses victims). **15.3** after **14.4c** (14.5 disc is not enough while seq-id pixel victims are live; do not add new seq-id ranges for sword/bow). **14.6** (per-frame `dir.ff` reads) is **after** the rest of the base system (through **16**) and only when the requester says we are ready to test the **full campaign**. **11.10** is the leftover wave-1 sprite commands; it needs live `BrainSpr` from **15.1** and comes **before** damage (**15.2**). **Audio (12) is after inventory and combat (16),** not after DinkC. A bite is done when **Done when** is true on Flycast or hardware *and* any **Host check** passes. Update PROGRESS in the same PR. `playsound` is a **silent stub** until 12.

**FreeDink is the implementation, not a hint.** This is a **graft**. When a behavior already exists in GNU FreeDink (`live_screen.cpp`, `gfx_sprites.cpp`, `dinkini.cpp`, `brain.cpp` `move` / `check_if_move_is_legal`, `game_place_sprites`), **copy that rule**. Do not invent a “simpler” hardbox, center, vision filter, or move test. If the port and FreeDink disagree, the port is wrong unless this plan names a Dreamcast-only exception (PVR lists, ISO 8.3, Maple). Patch this plan in the same PR if we must diverge.

**Canon for layouts:** GNU FreeDink headers (`dinkvar.h`, `screen.h`, `hardness.h`, `dinkini.c`) plus *The Ultimate Dink File Format FAQ* (Dink Network). When this plan and a checked-out FreeDink tag disagree on a field width, **FreeDink wins** — patch this plan, do not invent a third layout.

**Input (binding decision):** **Play the game with the Dreamcast controller only** (Maple pad). D-pad walk (8-way, same as Dink’s keypad dirs), A talk/confirm, B hit/cancel, X magic, Y inventory, Start title/pause. Choice menus and inventory are pad-driven, not number keys. Saves are VMU slots — no name typing. Do **not** require a PC keyboard in Flycast or a Dreamcast keyboard to finish the campaign. Maple keyboard / emulator keys are optional later for debug and are **deferred**. Original Dink’s extra PC keys (quick-save, editor, cheats) stay unmapped unless a later bite names them.

**Frame rate (binding decision):** **60 FPS / 60 Hz logic** is the target (VGA and 480p-class). DinkC `wait`, walk speed, and `dink.ini` delays are converted against that tick so they match FreeDink. **30 FPS is the floor**, not the design: if a busy indoor or 480i cannot hold 60, drop *presentation* to 30 but keep **one 60 Hz simulation step** (or two ticks per displayed frame) — do not retune the whole game to 30 or walk/talk will desync. 96 tiles + a few dozen quads is not a 30 Hz problem on PowerVR2; if you miss 60, profile upload/disc/CPU blit, not “the DC cannot do 60.”

**Screen-to-screen delay (binding):** Flycast will look instant (host disk). **Real CDI/CD-ROM** is seek-bound, not SH-4. Keep the **current tileset** (and neighbor screens if they fit §1.2) resident so most edge walks are parse + a few sprites, **not** a loading screen.

| Case | Hardware target | Treat as bug if |
|---|---|---|
| Same tileset, few new seqs | **0.2–0.6 s** | Every hedge shows “Loading…” |
| New tileset / biome | **0.5–2 s** | |
| Cold first visit, several BMPs + `.ff` | **≤ 4 s** | **> 3 s** on a normal neighbor walk |
| Track change (one ADPCM stream) | **+0.3–1 s** | Music seek during every screen |

A full-screen load is for **title → first map** and a **tileset miss** only. Worse than that is pack order or over-evict (GOTCHAS).

**DinkC performance (binding decision):** FreeDink’s interpreter is **good enough on SH-4**. The SH-4 is fast enough to run stock DinkC **reliably on throughput**. Do **not** write a custom “tuned” DinkC VM, JIT, or new language for speed. Scripts are tiny and mostly asleep on `wait` / `say_stop`. Frame time will be BMP decode, PVR upload, hardness, and GD-ROM — not dispatch.

**Reliability ≠ MHz.** Wrong `freeze` nesting, `say_stop` vs screen `main`, or missing commands look like a “slow CPU.” They are not. Hosting rules: graft FreeDink; parse each `story/*.c` **once**; table-dispatch; cap ~2 ms / a few thousand ops per frame so a busy loop cannot lock the machine. A hitch blamed on DinkC is **disc or upload** until a profile says otherwise. Revisit a new VM only with a measurement that names dispatch as the spike (not expected on freeware Dink).

---

## 0. Legal and data source

**Assumed legal source**

- Engine reference: **GNU FreeDink** (GPL). Behavior bible, not an SDL binary on SH-4.
- Game data: **official freeware Dink** and/or **FreeDink data**. The original game data is **required**. Required files at `DINK_DATA`:

  | File / dir | Role |
  |---|---|
  | `dink.dat` | 768-cell world table (which `map.dat` slot, MIDI id, indoor) |
  | `map.dat` | Fixed-size screen records (tiles + editor sprites + screen script) |
  | `hard.dat` | Tile hardness bitmaps |
  | `dink.ini` | Sequence load table (paths, delays, hardboxes, centers) |
  | `tiles/ts*.bmp` | Tileset sheets (50×50 cells) |
  | `graphics/**` | Sprite frames (BMP and/or `.ff` packs) |
  | `story/*.c` | Unmodified DinkC |
  | `sound/` | WAV SFX + MIDI (MIDI is **not** played raw on AICA) |

**Rules**

- Do not commit RTSoft/GNU blobs unless that exact file’s license allows it. `DINK_DATA` is the **inner `dink/`** of GNU `freedink-data` (has `Dink.dat`), **outside** this repo. Mixed-case names (`Dink.dat`, `Tiles/`) are normal.
- Never compile a host `DINK_DATA` path into the SH-4 binary. Disc layout: folder **`dink`** at ISO root → `/cd/dink` (see GOTCHAS).
- MIDI → ADPCM is an **offline pack** step. Do not claim converted tracks live in-repo.
- Still-proprietary FreeDink replacement sounds stay out of the tree.

---

## 1. Dreamcast hard constraints

Not “optimize later.”

| Resource | Hardware | Port budget |
|---|---|---|
| Main RAM | 16 MB SDRAM, SH-4 ~200 MHz | After KOS/libc/stack: **~12–13 MB**. Engine + caches **≤ 10 MB**. |
| VRAM | 8 MB PowerVR2 | 640×480 RGB565 ×2 ≈ **1 228 800 B**. Textures **≤ 6.5 MB**. |
| Sound RAM | **2 MB AICA** | Resident SFX **≤ 512 KB**. One music ring **256–512 KB**. Voices **≤ 16**. |
| CPU | SH-4 | No x86. Sprite math: int or SH-4 FPU. No thread-per-sprite. |
| GPU | PowerVR2 **textured quads** | No SDL blit path as renderer. |
| Disc | GD-ROM / CDI / `dcload` hostfs | Stream per screen. Never preload the whole tree. |
| Input | Maple controller | See §1.3. |
| Video | **640×480** RGB (VGA or 480i) | Optional **320×240** only as a named fallback (Bite 18.3). |

### 1.1 Coordinate system (do not invent another)

| Quantity | Value | Notes |
|---|---|---|
| Tile size | **50×50** px | Original. Never 32/64 as the *logical* size. |
| Screen tiles | **12 × 8 = 96** | Indices in this plan are **1-based** in file structs if FreeDink is; keep a comment at the parser. |
| Playfield | **600×400** | 12×50 by 8×50. |
| Status bar | **640×80** under the playfield | Original 640×480 = 400 play + 80 HUD. Playfield is **centered horizontally** with 20 px left/right gutter (`offset_x = 20`, `offset_y = 0`) unless FreeDink’s current tag uses a different origin — match FreeDink. |
| World | **32 × 24 = 768** screens | Screen number `n` is 1..768. Neighbor: `n-1` west, `n+1` east, `n-32` north, `n+32` south (confirm against FreeDink `update_play_input` / map walk). |
| Sprite 1 | Always **Dink** | Editor sprites in `map.dat` occupy slots 2..100 typically (`MAX_SPRITES_EDITOR` 100, slot 0 unused). Runtime sprites go up to FreeDink `MAX_SPRITES_AT_ONCE` (300) — **do not allocate 300 fat textures**; allocate 300 *metadata* slots, textures only for live seqs. |

### 1.2 Memory map (engine)

Keep a `mem_log()` that prints these counters every screen load. **14.4** owns the main-RAM file/pixel pools that this table originally omitted (`file_blob`, `cpu_pixels`, `ts_rgb`). Those three plus engine BSS/scripts must stay inside the **≤ 10 MB** engine cap **after** `bmp_decode` is freed. During unpack, `mem peak` includes `file_blob` + `cpu_pixels` + `ts_rgb` + `bmp_decode`; that sum plus CPU atlas BSS (512 KB) + seq/hard (~0.5 MB) can exceed 10 MB if every pool binds at once (~10.4). Village realistic peak is still ~9. Cap-check is post-decode.

| Pool | Cap | Lives in | Contents |
|---|---|---|---|
| `bmp_decode` | 1.5 MB | Main | Transient BMP unpack; **free before next big load**. Never two slurps at once. Peak during one decode is this pool **plus** current `file_blob` + `cpu_pixels`; cap check is **after** the unpack is freed. Name the peak in `mem_log` (`mem peak`). |
| `file_blob` | **≤ 4.5 MB peak** | Main | Session `dink_blob_get` (dir.ff, tilesheet BMP, story, ini). Peak = Always + **this** Screen’s packs + **Prev** packs. Steady ≈ Always + Prev (Screen packs stay until two screens old). **Not** “every pack ≥80 KB forever.” `hard.dat` is an open `FILE*`, **not** this pool. |
| `cpu_pixels` | **≤ 2.0 MB** | Main | Decoded sprite ARGB1555 in `EdGfx` + sticky kill/Dink frames. Cap is **bytes**, not only 96 slots. **14.4c** owns play-path eviction of those pixels (Always/Screen/Sticky class victims, not recency; Prev is packs only). Choice overlay (seq 30) and inventory overlay (seq 423) are **VRAM after upload** — free those CPU pixels; do not count 640 KB / ~1 MiB twice. |
| `ts_rgb` | **≤ 1.25 MB** | Main | Decoded tilesheet RGB565 LRU. Byte cap, not “8 sheets.” Duck vis 2 uses Ts01+Ts02+Ts03 (~1.17 MB if all three stay decoded). Keep at most what fits; extra sheets stay as BMP in `file_blob` until 14.5. |
| CPU atlas | 512 KB | BSS | Tile stamp buffer (`g_atlas_pix`). Do not calloc on swap. Count in `mem peak`. |
| `dink_dat` | ≤ 64 KB | Main | Parsed world table (768 × a few ints) |
| `cur_screen` | ≤ 64 KB | Main | One `editor_screen` + derived tile ids |
| `hard_cache` | ≤ 256 KB | Main | Current screen 600×400 hardness **or** 96 tile-hard refs + sprite boxes |
| `seq_meta` | ≤ 256 KB | Main | `dink.ini` parsed table (paths, delays, boxes) — **not** pixels. **14.4 catalog** may add a few KB of pack/frame sizes (no pixels). |
| `tile_tex` | ≤ 512 KB | VRAM | Current tileset atlas |
| `sprite_tex` | ≤ 4 MB | VRAM | Current screen + Dink seqs |
| `title_tex` | ≤ **1 MiB** | VRAM | 1024×512 RGB565 pad of 640×480 still; **freed** on leave-title (4.2) |
| `sfx_bank` | ≤ 512 KB | AICA | Boot SFX |
| `bgm_ring` | 256–512 KB | AICA | One stream |
| HUD atlas | ≤ 128 KB | VRAM | Digits, health chunks, magic gauge (256×256 ARGB1555) |

**Binding (residency, 14.4):** a `dir.ff` is a **decode source**, not a session cache. Needed frames go into `cpu_pixels`; the pack **stays** in `file_blob` while that screen is Screen **or Prev** (until two screens old). Then it may leave. Play-path still must not `fopen` (GOTCHAS); a miss skips the frame or waits for 14.5 distill. Do **not** add per-seq specials (`magic/dir.ff` drop, mom-hp pin, `EdGfx` victims `110..129` / `seq>=200`) — one policy, one catalog. **14.4b** is packs. **14.4c** is pixels. Distill and 14.6 do not skip 14.4c.

**Binding (do not pin the ≥80 KB class):** GNU freeware `dink/` has **142** `dir.ff` (~33 MB). **89** are ≥80 KB (~**31 MB**). That class cannot live in 16 MB SDRAM. The 80 KB pin was a `/cd` hang workaround, not a budget. Always-resident packs are a **named set** (player + UI), not a size threshold.

### 1.2.1 Working set (what we expect)

Four classes. `mem_log` prints bytes per class every swap. **Official freeware sizes** (GNU inner `dink/`, packs not 1555):

| Set | Pack bytes (approx) | Notes |
|---|---|---|
| Always | **~2.03 MB** | idle+walk+push+hit+textbox **+ menu** (`graphics/inter/menu/` ~825 KB). Walk seqs **71–74 and 76–79**, not 75 (`botl-b`). Arrows 456/457 are **loose BMPs** (~38 KB), not a `dir.ff`. `dink.ini` ~45 KB and preloaded `story/` count here. Inventory seq 423 frame 1 is **VRAM after upload** (~1 MiB 1024×512 ARGB1555, same pad class as title_tex); then free CPU. |
| House vis 0 | **1.18 MB** packs; **~0.96 MB** used-frame POT1555 | mom+innwalls+details+food/paper/shiny. Choice overlay **640 KB** is VRAM, not this row. |
| Outdoor 439 | **1.51 MB** | **home 676 KB + trees 423 KB** live here, not in the start house. |
| Duck 441 vis 2 | **1.34 MB** packs; Ts01–03 RGB565 **~1.17 MB** if all three decoded | |
| 408 savebot/girl | **1.54 MB** | `s1-gate` `preload_seq`/`create_sprite` → `people/girl/dir.ff` **188 KB**. Map.dat-only catalog misses this. |
| Castle | **1.58 MB** `struct/Castle/dir.ff` | Later; print in the catalog so 14.5 is not declared unused from the village. |

| Class | What | Evict? |
|---|---|---|
| **Always** | Named list above. | No |
| **Screen** | Packs and tilesheets needed to **decode** this vision’s editor sprites, **screen-script** `preload_seq`/`create_sprite` seqs, live `BrainSpr`, `parm_seq`, atlas. | Keep packs until this screen is **two screens old** (so Prev still has a decode source). Drop unused **pixels** on swap; do not drop the pack the moment frame 1 lands. Play-path (14.4c) may drop a Screen **pixel** whose pack is still cached so `ensure_frame` can decode again. Victim = class, not seq id. |
| **Prev** | **One** previous screen’s Screen packs (not pixels), for one backtrack without a GD-ROM seek. On the duck, Prev is **439**, not the house. | On the swap after next (two screens old). House re-entry after pig is **not** covered — catalog lists which hops would `fopen` again (seek + blob, not a hang class). |
| **Sticky** | Current Dink facing; seq **164** explode once loaded. **Not** the choice CPU copy after PVR upload. | When a replacement facing/seq is resident. Play-path must not evict these to make room. |

**Reopen vs hang:** The GOTCHAS “third `trees/dir.ff` hang” was **never confirmed** as a class separate from KOS #1492. Sector-pad retired first-open of non-2048 files **in Flycast**; hardware/ODE still pending. Later `ff load` with no `ok` is treated as the same stream-abort class (unpadded era), not as “reopen is unsafe.” **14.4b** Screen/Prev may `fopen` a pack after it is two screens old — that is GD-ROM seek + `file_blob` cost, not a hang veto. Catalog still **counts** would-reopen hops on house → 439 → 441 vis 2 → 408 → 407 → 439 → house so 14.5 sees that cost. If a hop needs a pack Prev already dropped, record `14.5: needed` only when the working set is over cap. Play-path still must not `fopen` every frame (decode-source / skip, 60 Hz).

If `file_blob` peak or `cpu_pixels`/`ts_rgb` would exceed caps, **refuse the new alloc** (keep previous atlas/pixels, log `mem refuse pool=… need=…`). Player-visible: refuse `cpu_pixels`/`file_blob` → **missing sprites** (log skip, do not silent-black Dink); refuse atlas → **previous floor** (wrong tiles, same as today’s `swap atlas fail keep`); never silent-black ground from a 524288 sbrk.

**Catalog (host, 14.4a):** from `DINK_DATA` + `dink.ini` + `map.dat` **and** that screen’s `story/*.c` (`preload_seq`, `create_sprite`, `sp_base_walk`). Emit per pack: path, bytes, seq ids, estimated POT ARGB1555 for frames this screen uses. `hard.dat` line: `FILE* not blob`. No game pixels in git. `make host` **prints** Always+Screen+Prev for house vis 0, 439, 441 vis 2, 407, 408 (girl), castle pack size, and a **campaign** Always+Screen plus Always+Screen+Prev peak (every nonempty map; Prev = neighbor or warp). It **fails** only if the catalog tool itself is broken — over-cap working sets print `14.5: needed` with numbers, they do not fake a green 14.4b.

**Distill (14.5, gated):** only if that catalog shows a **legal** Always+Screen+Prev **peak** still over cap after dropping unused pack bytes. CDI-only subset `dir.ff` of used 8-bit BMPs (like MIDI→ADPCM). Host tests keep **unmodified** `DINK_DATA` `dir.ff` as required original data. Generated `build/distill/` is a DC pack step, not a second art pipeline. Per-frame TOC/offset **load** (do not slurp unused BMPs into `file_blob`) is **14.6**, after 16 and a full-campaign go. Do not zlib the official tree in git.

### 1.3 Controller map

| Maple | Dink |
|---|---|
| D-pad | Walk (brains / player) |
| A | Talk / confirm / advance `say` |
| B | Hit / cancel |
| X | Magic (once Bite 15.x exists; ignore until then) |
| Y | Inventory (once Bite 16.x exists) |
| Start | Pause / leave title |
| L/R | Unused in v1 (do not bind map-cheat) |

Logic tick: **60 Hz** on VGA. Tie animation delays from `dink.ini` to that tick (FreeDink frame delay is ms-ish — convert: `frames = max(1, delay_ms * 60 / 1000)` and then **diff against FreeDink** on one walk cycle).

### 1.4 Texture rules

- Upload **RGB565** or **VQ**. No RGBA8888 resident textures.
- **`pvr_txr_load_ex` always twiddles.** Draw twiddled `PVR_TXRFMT_RGB565`. Do not set `PVR_TXRFMT_NONTWIDDLED` unless the load was linear (it was not).
- Pad non-POT images (640×480 → 1024×512). UVs = `w/tw`, `h/th`.
- 50 is not a power of two: **atlas** into 512×512 (50×50 cells + 1 px gutter) **or** pad each tile to 64×64. Pick one in Bite 6.1 and do not mix. Same twiddle rule as the title.
- Evict on screen change: previous `tile_tex` + seqs not referenced by the new screen or by Dink.

---

## 2. Work bites

### Phase A — Boot to official title still — **DONE** (Flycast, real BIOS)

#### Bite 0.1 — Repo skeleton (no KOS calls required to exist yet)

- Create layout in §3. `README.md` documents `KOS_BASE`, `dc-chain`, `DINK_DATA`.
- `Makefile` has two targets from day one: `host` (gcc, tools + unit tests) and `dc` (`kos-cc` → `dinkcast.elf`).
- **Bootstrap already in-tree:** `make host` runs `tools/check_port_plan.py` and `tools/check_agents.py`; `make dc` exits 2 until 0.2. Remaining 0.1 work is empty `src/` stubs listed in §3 as you need them — do not skip to tiles.

**Done when:** `make host` is green on the PR; `make dc` exists and fails clearly without KOS.  
**Status: done** (`make host`, `Makefile.dc`, Docker image).

#### Bite 0.2 — Color field

- `vid_set_mode(DM_640x480, PM_RGB565)`.
- Clear to `#5A3A1A`. Infinite `thd_sleep` / pvr wait.
- Serial: `dinkcast boot ok`.

**Done when:** 640×480 solid field on DC or emulator.  
**Status: done** (brown boot / HUD; title replaces it on success).

#### Bite 1.1 — Path resolver **(done)**

- `dink_fs_init()`: try `/pc/dink` (`dcload`), then `/cd/dink`, then compile-time `DINK_DATA`.
- `dink_fopen(rel)` joins root + relative, ISO9660-safe (`8.3` fallback: also try uppercased names).

**Host check:** `tools/test_fs_join` on Linux.

**Done when:** Serial prints resolved root.

#### Bite 1.2 — Existence probe **(done)**

- Open `dink.dat`, `fseek` end, print size.
- Missing file → **red screen** + `missing dink.dat` on serial. No hang.

**Done when:** `found dink.dat N bytes` with N matching the host `stat`.

#### Bite 2.1 — BMP header only **(done)**

- Support **BITMAPINFOHEADER**, uncompressed **8-bit paletted** and **24-bit**. Reject RLE/32-bit/OS2-v1 if not needed (log and fail).
- Fill `struct Bitmap { int w,h,stride; int bpp; uint8_t *pixels; uint8_t pal[256*3]; }`.
- Hard cap: `w*h*2 > 1 500 000` → error (RGB565-sized bound).

**Host check:** `tools/bmp_info path` prints `w h bpp first_pixel`.

**Done when:** Host tool matches ImageMagick/`file` on one 8-bit and one 24-bit Dink BMP.

#### Bite 2.2 — BMP on DC **(done)**

- Load one small known BMP from `DINK_DATA` (e.g. a 50×50 tile) into main RAM, print `w h`, **free**.

**Done when:** Serial numbers match host.

#### Bite 3.1 — Identify the official title file **(done)**

- Do **not** invent a logo.
- **Locked path:** `tiles/Splash.bmp` (also `Tiles/Splash.bmp`) — GNU freedink-data **640×480 8-bit** load still (“Loading…” seascape). That is the first showable official full-screen graphic.
- The start-menu **Dink Smallwood wordmark** is sequence **196** (`graphics/startme/options/dinkL-`) inside a **`.ff` pack**, not a lone BMP. That is **Bite 8.5 + a later title-menu pass**, not a redo of 3.4.
- Path constant: `src/title_path.h` → `tiles/Splash.bmp`.

#### Bite 3.2 — CPU RGB565 convert **(done)**

- 8-bit: palette index → RGB565 (`(r>>3)<<11 | (g>>2)<<5 | (b>>3)`).
- 24-bit: BGR BMP order → RGB565.
- Output buffer `w*h*2`, then **free** the paletted source.

**Host check:** hash first 64 px of a fixture BMP.

#### Bite 3.3 — PVR upload **(done)**

- Pad 640×480 → **1024×512** RGB565 (~1 MiB). UVs `640/1024`, `480/512`.
- `pvr_txr_load_ex(..., PVR_TXRLOAD_16BPP)` then `PVR_TXRFMT_RGB565` **twiddled**. Comment: source still is 614400 B; padded tex is 1 MiB; framebuffers ~1.2 MiB.

**Done when:** splash is not striped; texture handle non-null.

#### Bite 3.4 — **First visual milestone: title quad** **(done)**

- One textured quad of `tiles/Splash.bmp` at 640×480.
- `pvr_wait_ready` / scene / list / finish / swap every frame.
- Boot: wait GD-ROM; on failure, **bfont HUD** + `/cd` listing (not Flycast’s log).

**Done when:** Official splash is stable on **640×480 RGB** in Flycast (real BIOS) or hardware. **Verified 2026-08-16.** Do not start map tiles until 4.x is in and 6.x is the next visual.

#### Bite 4.1 — Maple poll **(source)**

- Read controller port 0. Ignore missing controller (stay on title).
- Do not tear down the title PVR loop until 4.2. Poll inside the present loop.
- Controllers: see §1.3. No keyboard.

#### Bite 4.2 — Leave title **(source)**

- Start or A → `GAME_STATE_LOADING` (solid color or “loading…” via bfont).
- **`pvr_mem_free` the title texture** so VRAM is back (title_tex ≤ 1 MiB).
- Keep `dink_fs` root; next loads use the same `/cd/dink`.

**Done when:** Title → placeholder; HUD or serial `leave_title`; VRAM up after free. Then Bite 5 (map parse), not a new title graphic.

---

### Phase B — Official map data, one screen of tiles

#### Bite 5.1 — `dink.dat` parser (host-first)

Parse original layout (verify vs FreeDink / FAQ; typical shape):

```
offset 0:     char ident[24];        /* often "Smallwood" + pad */
then:         int32 loc[769];        /* 0 = empty screen; else map.dat slot */
              int32 music[769];      /* MIDI id */
              int32 indoor[769];     /* indoor flag */
```

Use **explicit little-endian** readers (`read_i32le`). Never `fread` a packed struct on SH-4 without a static assert on size vs file.

- Store `struct World { int32_t loc[769], music[769], indoor[769]; }`.
- RAM: 769×12 ≈ 9 KB.

**Host check:** `tools/dump_world` prints count of non-zero `loc` and `loc[1]`, `music[1]`.

**Done when:** Numbers match a hex dump of the same `dink.dat`.

#### Bite 5.2 — `map.dat` record size lock

- Compute `record_size` from FreeDink `sizeof(struct editor_screen)` **as serialized**, not as the compiler’s in-memory size.
- `tools/map_recsize` prints `file_size / record_size` and asserts remainder 0 (or documents trailer).

**Done when:** Record count is consistent with used `loc[]` max.

#### Bite 5.3 — One screen tile plane

- Load screen index `loc[S]` (start with the **stock start screen** — FreeDink new-game `&player_map`, usually a Stonebrook-area id; record the number in `src/start_map.h` after reading data).
- Parse **96 tiles**: tileset id + local tile index as FreeDink stores (often `tile.num` encodes sheet*128 + cell — **copy FreeDink’s decode**, do not guess).
- Print 8 lines of 12 integers.

**Host check:** `tools/dump_screen S` matches WinDinkEdit / FreeDink editor for that screen.

**Done when:** Same dump on DC serial.

#### Bite 5.4 — Editor sprites (data only)

- Parse up to 100 editor sprites: `active, x, y, seq, frame, type, size, brain, hard, vision, script[13 or 21]`.
- **Vision:** sprite is live when `vision == 0` or `vision == &vision` (`game_place_sprites`). Start `&vision` is **0**.
- **Type:** FreeDink `game_place_sprites`: **0** background draw + hardness, **1** live sprite, **2** hardness only (not drawn).
- **`hard == 0` means solid** (inverted). Stamp `k[frame].hardbox` from `SET_SPRITE_INFO` / `load_sprites` defaults (`gfx_sprites.cpp`).
- **SET_SPRITE_INFO** last-wins per (seq,frame) for xoffset/yoffset/hardbox. Do not invent a second center.
- Print actives: `sprite i seq= frame= type= vis= xy= script=`.

**Done when:** Count of actives matches the PC editor for that screen.

#### Bite 6.1 — Tileset atlas policy (pick and freeze)

- Policy A: one 512×512 RGB565 atlas, 50×50 cells, 1 px gutter.  
  or Policy B: 64×64 padded tiles, N textures.  
- **Choose A** unless a tileset BMP will not fit; write the choice in `src/tiles.h`.
- Budget: **≤ 512 KB** VRAM for the current sheet(s) this screen needs. A screen can reference more than one `ts*.bmp` — load **only those**, evict the rest.

#### Bite 6.2 — Decode `tiles/tsNN.bmp`

- 8-bit sheets. Index 0 (or magenta — match FreeDink) is **transparent** for *sprites*, not for ground tiles.

**Host check:** cell (0,0) RGB of `ts01.bmp` matches a known crop.

#### Bite 6.3 — Upload atlas + 96 quads

- Draw playfield at (§1.1) origin. Each tile = one quad, UV into atlas.
- Empty tile index 0 = skip or black (match FreeDink).

**Done when:** **Second visual milestone** — stock start screen tiles, 600×400 in the 640×480 frame, original 50×50 cells. No Dink yet.

#### Bite 6.4 — Evict

- API `tiles_evict()`. Next load must not leak (`pvr_mem_available` ± 4 KB).

**Done when:** Load screen A, evict, load A again; VRAM delta 0.

---

### Phase C — Hardness, Dink sprite, walk

#### Bite 7.1 — `hard.dat` parser

- FreeDink: hardness tiles are a stream of 50×50 (or 51×51 — **verify**) uint8 grids, indexed by tile hardness id.
- `hard_lookup(tileset, cell) → grid`.

**Host check:** dump hardness id for start screen tile (0,0).

#### Bite 7.2 — Screen hardness stamp

- Build a 600×400 (or 12×8 coarse) walk mask by stamping each tile’s hard grid.
- Sprite hardness later (7.3).

#### Bite 7.3 — Sprite hardboxes (static)

- From `dink.ini` hardbox per frame (Bite 8.1). Stamp into the mask at editor sprite `x,y` minus center.

#### Bite 7.4 — Debug overlay

- `DINK_DEBUG_HARD`: magenta 50% on blocked pixels (or 2×2 blocks). Off by default.

**Done when:** Overlay matches PC editor hardness on the start screen.

#### Bite 8.1 — `dink.ini` parse

Commands (names as in file):

| Directive | Meaning |
|---|---|
| `load_sequence` / `load_sequence_now` | `path`, `seq_id`, delay, center x/y, hardbox l/r/t/b |
| `SET_SPRITE_INFO` | per-frame center / box |
| `SET_FRAME_FRAME` | frame alias |
| `SET_FRAME_DELAY` | per-frame delay |

- Fill `struct Seq { char path_prefix[128]; int delay; int cx,cy; struct Frame frames[MAX_FRAMES]; } seqs[MAX_SEQUENCES];`
- `MAX_SEQUENCES` = FreeDink’s (1000-ish). **Metadata only.** Paths stay relative.

**Host check:** `tools/dump_ini 1` prints seq 1 prefix + frame count.

#### Bite 8.2 — Frame path resolve

- `prefix + frameindex + ".bmp"` and/or `.ff` pack. Implement **BMP first**; `.ff` is Bite 8.5 if start-screen Dink frames are BMP.

#### Bite 8.3 — Load Dink idle/walk only

- Sequences for Dink walk/idle (ids from `dink.ini` / FreeDink `base_walk` defaults — typically walk seqs in the 1–12 band; **copy the ids FreeDink assigns to player**).
- VRAM **≤ 256 KB** for the current facing.

#### Bite 8.4 — Draw sprite 1

- Quad at `x - cx + offset_x`, `y - cy + offset_y`.
- New-game position: FreeDink start `x,y` on `&player_map` (record in `start_map.h`).

**Done when:** Official idle frame on the start screen at the official spawn.

#### Bite 8.5 — `.ff` reader (only if needed)

- If any required Dink frame is `.ff`, implement the FreeDink FF container (palette + frames). Host dump first.

#### Bite 8.6 — Draw editor sprites (houses / props)

- **5.4 is data only.** This bite draws every **active** editor sprite on the current `map.dat` screen (slots **1..100**, slot 0 unused; Dink remains sprite **1** at runtime and is **not** taken from the editor slot).
- Resolve `seq` + `frame` through `dink.ini` + existing BMP / `.ff` loaders. Skip missing seq/prefix (log once); do not invent frames. Draw only `editor_sprite_draw` (vision + not type 2).
- Quad placement same as 8.4: `x - cx + playfield_ox`, `y - cy + playfield_oy`.
- **Z:** match FreeDink y-sort (typically `y` as depth so southern props sit in front). Dink vs props must not always paint last.
- Cache unique `(seq, frame)` textures for the screen. **Evict all** on screen change / leave play. Count against `sprite_tex` (≤ 4 MB with Dink seqs).
- No DinkC, no talk, no brains beyond static draw. Required so V4 walk is readable (Stonebrook houses).

**Host check:** start-screen active count matches `tools/dump_screen`; each loaded seq/frame listed.

**Done when:** Official start-screen houses/props visible with tiles + Dink idle (or walk if 9.3 is already on the disc).

#### Bite 9.1 — Input → facing

- D-pad sets `dir` 2/4/6/8 (Dink dirs: 1 unused, 2=down? — **use FreeDink’s 1–9 keypad dirs**: 2 down, 4 left, 6 right, 8 up).

#### Bite 9.2 — Move + hardness

- Speed: FreeDink `spr[1].speed` (default 3). **`move()` is 1 px per step**; `check_if_move_is_legal` samples **`get_hard(x - playl, y)`** (sprite origin), not the seq hardbox. Hardboxes are for **stamping** (`add_hardness`, exclusive `[left,right)×[top,bottom)`). Slide: try X then Y. Keep walk seq while the pad is held (blocked still animates).

#### Bite 9.3 — Walk animation

- Advance frame using seq delay. Switch seq on dir change. Idle seq when no pad.

**Done when:** Walk the start screen; walls match PC. No talk/hit yet.

- **Leftover (graft with 10.1 / walk polish, not a house PR):** `human_brain` idle snap 1/3→2, 7/9→8; full `get_box` clip (we only skip fully off-playfield quads).

---

### Official campaign systems (stock 1.08 only — no D-Mods)

These **are** the FreeDink systems needed to finish the retail freeware campaign. They are **not** extra. Do not invent a parallel engine. **Out of scope:** D-Mod loader, `dmod.diz`, second data tree, DinkEdit, D-Mod-only DinkC.

| System | FreeDink | Bite | Notes |
|---|---|---|---|
| Disc / title / tiles / house / walk | paths, `load_screen_to`, `get_hard`, `game_place_sprites` | 0–9, 8.6 | **done** |
| Talk / hit probes | `run_through_tag_list_talk`, hit list | **10** | |
| DinkC graft | `dinkc.cpp`, `dinkc_bindings.cpp` | **11** | long pole |
| Attach on enter | `game_place_sprites`, `game_screen_init_scripts`, `locate("MAIN")` | **11.6** | screen `script` + every sprite `script` |
| Engine + `MAIN.c` globals | `attach()`, `make_global_int` | **11.4** | copy lists; include `&vision`, `&story`, `&life`, … |
| `&vision` / `force_vision` | `draw_screen_game`, DinkC | **11.5 / 11.8** | burned house, many quests |
| `freeze` nest, yields | `spr[].freeze`, `wait` / `say_stop` / `move_stop` / `choice` | **11.3** | |
| Unimplemented command | log + no-op | **11.9** | never skip the `.c` file |
| Live sprite DinkC | `move` / `create_sprite` / `sp_kill` / NPC `sp_x` | **11.10** | after 15.1; before 15.2. Essential, not village-only |
| SFX + one music stream | `sfx`, `bgm`; MIDI id from `dink.dat` | **12 (after 16)** | silent `playsound` until then |
| Font / say / choice | brain 8, `game_choice` | **13** | V5 |
| Edge walk + swap | `did_player_cross_screen` | **14.1–14.2** | |
| Warp | `special_block`, `process_warp`, `is_warp` / `warp_*` | **14.2** | parse those map fields |
| `screenlock` | `get_hard` clamp + DinkC | **14.2** | |
| Indoor flag | `dink.dat` `indoor[]` | **14.2** | only if stock scripts/engine use it |
| `play.spmap` editor_type | `fix_dead_sprites`, `update_play_changes` | **14.2 + 17.1** | dead monsters stay dead 1–5 min |
| Brains 0–17 | `brain_*.cpp` / `update_frame` | **15.1** | all stock ids this bite; then 11.10; damage 15.2 |
| Combat / weapons / magic | `hurt`, `arm_weapon`, missiles | **15** | |
| Push | `human_brain` / `dink_base_push` | **15** / walk polish | |
| Death / game over | `die` script, life 0 | **15.2** | |
| Touch / pickup | `run_through_touch_damage_list` | **16.1** | |
| Inventory + HUD | `status`, items | **16** | V6; `draw_status` |
| Map graphic | `process_show_bmp`, seq 165 | **16.3** | Y / map button |
| VMU save | `savegame` → Maple VMU | **17** | include `play.spmap` + globals |
| 60 Hz vs `dink.ini` delays | `ThinkSprite`, `game_compute_speed` | keep 60 Hz tick; convert `wait`/seq delay | not a 30 Hz retune |

**Do not skip** a row because the start house does not use it. Graft when that bite is open.

---

### Phase D — Talk / hit hooks, then DinkC in waves

DinkC is the long pole **for completeness**, not for CPU. **Use FreeDink’s interpreter** (graft + bind). Do not design a faster dialect. Hosting rules: parse once, function table, per-frame op cap. See the binding decision at the top of this document.

Ship commands in **waves**. Unimplemented command = `DINKC unimplemented: name` + **no-op**. Never skip the file.

#### Bite 10.1 — Talk probe (engine)

- A **just-pressed**: `run_through_tag_list_talk` — first live editor sprite (slot 1–99) whose hardbox is inflated by 10 and extended 50 (x, dirs 4/6) or 35 (y, dirs 2/8) in Dink's facing, and that contains Dink's `(x,y)`. Skip `brain==8` and empty `script`. Not a ray-step along `dir`.
- Freeze player (`spr[1].freeze++` nest). Unfreeze is 11.3 / DinkC.

#### Bite 10.2 — Hit probe (engine)

- B just-pressed: `item-fst` — snap diag 1/3→2, 7/9→8; `seq = base_attack+dir` (fists **100** until Bite 15); `nocontrol` until the seq ends.
- On `SET_FRAME_SPECIAL` frames, `run_through_tag_list` (hardbox +5/−5/−5/+10, dir range 28/36) — first overlapping live sprite queues `script_on_hit`.

#### Bite 10.3 — Hook table (stub)

```
void script_on_main(int script_id);
void script_on_talk(int sprite);
void script_on_hit(int sprite);
```

Stubs log `talk sprite=N script=foo`.

**Done when:** On a stock NPC screen, A logs talk; B plays hit anim and logs hit. Scripts need not run yet.

---

#### Bite 11.0 — DinkC files on disc

- `story/NAME.c` from sprite/screen script field (no extension in `map.dat`; add `.c`).
- ISO9660: also try `STORY/NAME.C`.
- Load whole file into a **32 KB** cap buffer (stock scripts are small; larger → log and fail that script only).

#### Bite 11.1 — Lexer

- Tokens: ident, number, `"string"`, `&name`, operators `+ - * / = == != < > <= >= && ||`, `( ) { } , ;`.
- Comments: `//` to EOL. Ignore `/* */` if FreeDink does not; if it does, match.
- Host: lex `story/start.c` (or whatever the start screen uses), print token count.

#### Bite 11.2 — Parser → bytecode or AST

- Procedures: `void name(void)` { … }.
- Stmts: expr; `if` / `else`; `while` if present in stock (FreeDink has limited control flow — **match FreeDink**, not ANSI C).
- **No** general C. No structs, no pointers.

**Host check:** parse all `story/*.c` in `DINK_DATA`; report fail list. Target: **0 parse errors** on stock freeware scripts (unknown *commands* are still valid calls).

#### Bite 11.3 — VM with yield

DinkC is **concurrent**. Each attached script is a fiber:

| Yielding call | Behavior |
|---|---|
| `wait(ms)` | Sleep, resume later |
| `say_stop` / `say_stop_npc` | Wait for A |
| `move_stop` | Wait until at dest |
| `choice` | Wait for menu |

- `struct Script { ip; locals[256]; wait_until; sprite; state; }`.
- **Max 20 live scripts** (start conservative; FreeDink allows more — raise when a stock screen needs it).
- 60 Hz: tick all runnable scripts, budget **2 ms** SH-4; overflow → log.

**Host check:** script `void main(void) { wait(1); }` completes in a fake 60 Hz loop.

#### Bite 11.4 — Variables

- `&name` globals persist. **Copy** FreeDink `attach()` engine vars **and** official `story/MAIN.c` `make_global_int` list (`&vision`, `&story`, `&life`, `&exp`, `&player_map`, …). Do not invent names.
- Locals per script.
- `int &x;` in 1.08: match FreeDink 1.08 mode.

#### Bite 11.5 — Wave 1 commands (opening village)

Implement **exactly** these first (signatures as DinkC Reference / FreeDink):

`say`, `say_stop`, `say_stop_npc`, `wait`, `freeze`, `unfreeze`, `sp_active`, `sp_x`, `sp_y`, `sp_dir`, `sp_seq`, `sp_frame`, `sp_brain`, `sp_script`, `sp_base_walk`, `sp_base_idle`, `sp_base_attack`, `sp_speed`, `sp_timing`, `sp_pseq`, `sp_pframe`, `move`, `move_stop`, `create_sprite`, `sp_kill`, `playsound` (stub until 12), `debug`, `kill_this_task`, `script_attach`, `external`, `set_callback_random` (may no-op if unused on start screens), `force_vision` (and `&vision` writes).

Wire `say*` to a **serial + later Bite 13 box**.

**Done when:** unmodified start-screen `main()` runs; a stock `talk()` that only `say_stop`s works with A advancing (box can be ugly).

**Leftover (not this bite):** several names above stayed **silent `return 1`** so they would not log `unimplemented`. NPC `sp_x` / `sp_y` / `sp_dir` / `sp_seq` / `sp_frame` / `sp_base_attack` are **sprite 1 only**. Those are **11.10** (needs 15.1 live sprites). `playsound` stays stub until **12.3**. `set_callback_random` may still no-op here if unused on start screens; make it real in **11.10**.

#### Bite 11.6 — Attach on screen enter

- Screen script from `map.dat` → `locate`/`MAIN` (`game_screen_init_scripts`).
- Each editor sprite with `script` → instance attached to that sprite → `main()`.
- Match FreeDink `game_place_sprites` attach order (rank, then scripts).

#### Bite 11.7 — Wave 2 (choices + items prelude)

`choice_start`, `choice_end`, numbered choice lines, `stop`, `wait_for_button`, `sp_touch_damage`, `sp_hitpoints`, `sp_defense`, `hurt`, `add_item`, `add_magic`, `add_exp`, `playsound` (**still stub** until 12), `initfont`/`get_next_sprite` as needed.

Depends on Bite 13 for the menu.

#### Bite 11.8 — Wave 3 (combat / magic / map)

`arm_weapon`, `arm_magic`, `kill_shadow`, `sp_attack_wait`, `sp_range`, `sp_target`, `compare_weapon`, `compare_magic`, `preload_seq`, `sp_custom`, `sp_editor_num`, `draw_status`, `update_status`, `stopcd`/`playmidi` (midi → our stream id table), `fade_up`/`fade_down`, `fill_screen`, `load_screen` / screen change helpers FreeDink uses internally vs DinkC.

#### Bite 11.9 — Command coverage log

- Every dispatch through one table. Startup can `DINKC_DUMP_FNS=1` to print implemented vs called-but-missing after a play session.
- Opening-hours gate: walk Stonebrook, talk to 3 stock NPCs, no `unimplemented` that aborts a quest.

#### Bite 11.10 — Wave 1 live sprite commands (after 15.1)

**Do not start until 15.1 is merged** and the requester says go. **Before 15.2.** Stock scripts need these; they are not optional village polish. Graft FreeDink `dinkc_bindings` + `spr[]` (`change_sprite`, `move` / `check_if_move_is_legal`, `add_sprite` / `create_sprite`, `sp_kill`). Write **live `BrainSpr`**, not the editor snapshot.

Make **real** (no silent `return 1`):

- `sp_x`, `sp_y`, `sp_dir`, `sp_seq`, `sp_frame`, `sp_base_attack`, `sp_base_idle`, `sp_pseq`, `sp_pframe` — slots **1–99**, read (`-1`) and write
- `sp_active`, `sp_kill` — hide/remove as FreeDink
- `move`, `move_stop` — set velocity; `move_stop` **yields until dest** (not resume next tick)
- `create_sprite` — allocate a live slot + seq/brain as FreeDink
- `sp_script` — attach `main()` to that sprite
- `script_attach`, `external`, `set_callback_random`

Leave **`playsound`** stub until **12.3**. Combat XP/death (`sp_exp`, `sp_base_death` / `sp_base_die`, `hurt` applying) stays **15.2**. `sp_sound` is sprite SFX — not this bite (no AICA until 12).

**Host check:** NPC `sp_x` write moves `BrainSpr`; `create_sprite` returns a live slot; `move_stop` does not complete in one 16 ms tick unless already at dest.

**Done when:** a stock script that `create_sprite`s or `move_stop`s an NPC does that on Flycast/hardware (not only “no unimplemented log”).

---

### Phase E — Text and transitions

#### Bite 13.1 — Font

- Original Dink font graphic (FreeDink `LiberationSans` is **not** the 1998 look; prefer the stock bitmap font if present in data). Atlas ≤ 64 KB.

#### Bite 13.2 — Say box

- Bottom or sprite-anchored box (match FreeDink placement). Glyphs, word wrap at playfield width − pad.
- A or B advances `say_stop`.

#### Bite 13.3 — Choice menu

- D-pad + A. Return index to VM as FreeDink does (`&result` / choice return — **match FreeDink**).

**Done when:** A real `talk()` with choices is playable from unmodified `.c`.

#### Bite 14.1 — Edge walk

- x < 0 → west, etc. Only if `loc[neighbor] != 0`; else clamp.

#### Bite 14.2 — Swap screen

- Evict **unused** tilesets/seqs only. Keep the tileset if the neighbor still uses it. Parse new `map.dat` record. Keep sprite 1, `&player_map`, wrap x/y (left exit → x = 619 / playl — **match `did_player_cross_screen`**).
- Do not stream a new music file on every edge unless `dink.dat` MIDI id changed. **Until 12.4 the id is stored only** (no AICA).
- **Warp:** parse `is_warp`, `warp_map`, `warp_x`, `warp_y`, `parm_seq`; `special_block` / `process_warp` + fade.
- **`screenlock`:** `get_hard` edge clamp when set.
- **`play.spmap` / `update_play_changes` / `fix_dead_sprites`:** editor_type 1–8 (killed / changed sprites persist 1–5 min).
- **Indoor:** honor `indoor[]` if stock engine/scripts use it.

#### Bite 14.3 — Leak check

After **14.4c**. Not a visual gate. **14.6** is not this gate. Pin-forever ≥80 KB packs grew until OOM; a 20-crossing delta is only meaningful with pack **and** pixel class residency.

- `mem_log` on every successful swap (`vram_free`, `swap_n`). After **20** swaps, print `leak 20` deltas vs swap **4** (warm). Main + VRAM deltas **≤ 4 KB** *for pools that should be stable* (`cpu_pixels` may change with the screen; `file_blob` must not climb unbounded). Measure a **two-screen ping-pong** (maps **439** / **441**), not house vs the 20th unique map.
- Log `swap_ms` on every successful swap (timer starts after the previous scene wait). Same-tileset neighbor must stay in the § screen-delay table **on hardware**. Flycast / host disk may be ~0.

**Host check:** `tests/test_leak` ping-pongs 439 and 441 twenty times. After swap 4, `|file_blob|`, `|always|`, and `|ts_rgb|` deltas to swap 20 are ≤ 4 KB; `ff_disc_loads` does not climb. `cpu_pixels` is logged, not failed.

**14.1–14.2 done when:** Walk start screen into a real neighbor; tiles + Dink. **14.3 done when:** 14.4 **pack and pixel** policy is live (14.4c); host 20-crossing counters stable; `swap_ms` in the Flycast log (may be ~0). Hardware delay is an ODE/burn check, not a host fail.

**Won’t in 14.3:** 14.6 TOC/offset; 15.3 weapons; a new `EdGfx` victim policy for Screen-vs-Screen slot thrash (Ethel oldman walk vs duck death). That is occupancy, not a leak.

#### Bite 14.4 — Residency (catalog first, then one policy)

**Why:** Session `dink_blob_get` plus size-pin (≥80 KB, 32 ff slots) keeps **decode sources** for the whole walk. Official data has **89** `dir.ff` ≥80 KB (~**31 MB**). Village walk after 15.2 pinned idle/walk/push/mom/walls/home/trees/magic/knight (~6–7 MB of packs) then `Out of memory` 200704 / seq 63 425984. Per-seq drops (`magic/dir.ff` after 164) are correct for that pack and **wrong as a pattern**.

Three PRs, same bite id: **14.4a** catalog, **14.4b** packs (Always/Screen/Prev), **14.4c** pixels (Always/Screen/Sticky). **Do not start 15.3** until **14.4c** (14.5 disc is not enough while seq-id pixel victims are live; do not add new seq-id ranges for sword/bow). **14.6** is not this gate. **14.4c** is not optional and is not 14.6. That does not replace the human gate after merge.

**Not a visual gate.** Host first. Keep the 164 magic drop until 14.4b.

##### 14.4a — Catalog + `mem_log` (no pin change)

1. Host tool (`tools/pack_catalog` or `tests/test_residency`): §1.2.1 screens including **408 girl via screen script**, Prev=**439** when cataloging duck, `hard.dat` as `FILE* not blob`. Print keep-whole-pack vs drop-after-decode vs peak-during-load. Over-cap lines print `14.5: needed pool=… bytes=…`.
2. `mem.c` / `mem_log` on every swap: pool bytes+counts, Always/Screen/Prev/Sticky, `mem peak` during decode. Still **size-pin** at runtime so Flycast does not change mid-catalog.

**Host check:** `make host` runs the catalog. Tool crash = fail. Over-cap working set = **print**, not fail (the point of 14.4a is to stop lying).

**Done when:** Catalog numbers for house, 439, 441 vis 2, 407, 408, castle pack; `mem_log` on Flycast swap; 164 special still present. Human reads the table.

##### 14.4b — One eviction policy

Only after 14.4a numbers exist.

1. Replace size-pin and `ff_cache_drop_unpinned` no-op with §1.2.1 classes. Always = **name list** in one header.
2. Retire ad-hoc `seqs[164]` pack drop into “Screen pack, frames complete → drop.” Sticky 164 **pixels** stay.
3. Play-path still must not `fopen`. Miss → skip (log once) unless 14.5 distilled that frame.
4. `mem refuse` as named above.

**Done when:** 14.4a catalog plus policy: `file_blob` **peak** on the opening-village walk is under 4.5 MB **or** 14.4a already recorded `14.5: needed` and this bite does not claim the walk is green. Do **not** ship `14.5: not needed` from a house+duck file_blob-only spreadsheet. Human confirms `mem_log` / no `Out of memory` if 14.5 is not needed.

**Won’t in 14.4a/b:** zlib official `dir.ff` in git; a second graphics format; custom DinkC; 14.3’s 20 crossings; weapons (15.3); removing the 164 drop in 14.4a; **pixel class eviction** (that is **14.4c** — Always/Screen/Sticky victims, not recency).

##### 14.4c — Pixel working set (`cpu_pixels` / `EdGfx`)

**Why:** 14.4b is **pack** classes (`file_blob`). Ethel’s house (#88) showed decoded **pixels** still fill a 96-slot table; play-path then evicts with seq-id ranges (`110..129` duck death, `seq>=200` “NPC walks”). That is a per-seq special, the same class 14.4 forbade for packs. Combat-first enter-path (#88) is the Screen fill order, not the eviction policy. Distill (14.5) and per-frame reads (14.6) do **not** replace this. Cap is **bytes** (§1.2), not slot count.

**Do not start** until the requester says go. Not 14.6. Not a visual gate. After 14.5 disc; **before 14.3**.

1. Play-path `ensure_frame` victim = §1.2.1: never Always, never Sticky (Dink facing, seq 164). Prefer a Screen pixel whose pack is still `ff_cached` so the frame can decode again. **No** seq-id ranges (`110..129`, `>= 200`, “innwalls vs oldman”).
2. Enter-path fill order stays combat death (164, duck 110/120, pig/pill/dragon walk) before people walks (#88). `create_sprite` / created people use the same class order — do not dump every walk frame into the table first.
3. If a decode would exceed **2.0 MB** `cpu_pixels`, `mem refuse pool=cpu_pixels` (keep previous pixels, log skip). 96 is an array bound: grow it only with a byte check, or evict first. Slot-full is not a different policy from byte-full.
4. Log `edraw evict` once per need, not a per-tick `edraw full skip`. Failed decode after evict must not leave `*n` stale.

**Host check:** Ethel map 2 vis 1 still has seq 117 and 123. A filled table + cached pack decodes a needed frame **without** `110..129` / `>= 200` in `edraw.c`. Name Always/Screen/Sticky in the test or log, not “seq 200.”

**Done when:** Those host checks; Flycast punch duck in Ethel’s house (headless + flying head, no skip storm); mom/oldman turn from a cached pack. `gh` search of `edraw.c` has no `s >= 110 && s <= 129` and no `s >= 200` victim pick.

**Won’t in 14.4c:** 14.6 TOC/offset reads; play-path `fopen`; rewrite `DINK_DATA`; 14.3’s 20 crossings.

#### Bite 14.5 — Distill (gated)

**Start only if** 14.4a printed `14.5: needed` with a pool and byte count. Typical: one 0.7–1.6 MB `dir.ff` from which we need a handful of frames, or Ts01–03 RGB565 over `ts_rgb`. Not a visual gate.

- Offline pack step (CDI only, like MIDI→ADPCM): subset `dir.ff` of used 8-bit BMPs. Host tests keep unmodified `DINK_DATA` `dir.ff` (**original data required**). Disc may add `build/distill/` at `make docker-cdi`.
- Screens: **every nonempty `map.dat` screen** in the stock campaign (all sprite visions on that screen). Follow `sp_script` / `script("…")` / `add_item("…")` / `add_magic("…")` callees. The 14.4a village walk is a host report, not the distill set. Keep the **used-frame set**. Do not maintain a village map list.
- `file_blob` Always+Screen+Prev can still exceed 4.5 MB on heavy screens after this union. **`mem refuse` is not distill.** Per-frame TOC/offset reads are **14.6**, not this bite. **Pixel class eviction is 14.4c**, not this bite.
- Do **not** zlib-compress the official tree and commit it.

**Done when:** Distill used-frame union covers every nonempty stock-campaign screen (disc completeness, #84–#86). Whole-pack `file_blob` over cap on heavy screens is expected until **14.6**. GOTCHAS: original `DINK_DATA` not rewritten; staged subset `dir.ff` is the decode source.

#### Bite 14.6 — Per-frame `dir.ff` reads (deferred)

**Do not start** until the rest of the base system is working (**16** / V6 inventory+HUD in that order) **and** the requester says we are ready to test the **full campaign**. Not next after 14.5. Not a visual gate. Not a village-only pack list.

Enter-path: `fopen` the pack once, read the existing `dir.ff` TOC (count + name/offset), `SEEK_SET` only the BMP payloads this Screen needs, decode into `cpu_pixels`. Do **not** slurp unused frames into `file_blob`. Play-path still must not `fopen` every tick (GOTCHAS). Same class as `hard.dat` rec reads. Official `DINK_DATA` stays unmodified. **Does not replace 14.4c** (pixel class eviction).

**Done when:** catalog Always+Screen+Prev `file_blob` peak on heavy campaign screens (including map 586/587 class) is under 4.5 MB without dropping rooms, or the requester accepts a documented cap miss. Host check: a pack whose used frames are a subset does not charge `file_blob` the whole `dir.ff`.

---

### Phase F — Combat and inventory

#### Bite 15.1 — Brains (engine, not DinkC)

Implement as FreeDink `update_frame` switch (`brain_*.cpp`). Graft **all** ids 0–17 this bite (stock campaign), not “add when the start house uses it.” **Log `brain unimplemented: N`** only for leftover/title ids that the pad-only port cannot run (mouse 13) or that need DinkC/hardbox not yet wired (button 14). Damage, `DIE`, missiles hitting, and `hurt()` stay **15.2**.

| brain | FreeDink |
|---|---|
| 0 | none |
| 1 | player (`human_brain`) — walk exists; push / talk / hit later |
| 2 | bounce |
| 3 | duck |
| 4 | pig |
| 5 | one-time anim |
| 6 | **repeat** (fireplace / fire) |
| 7 | one-time then stay |
| 8 | text (`say`) |
| 9 | pillbug |
| 10 | dragon |
| 11 | missile |
| 12 | scale |
| 13 | mouse (title leftover) |
| 14 | button |
| 15 | shadow |
| 16 | people / NPC walk |
| 17 | missile expire |

Do **not** reuse the old wrong map (9=bounce, 12=text).

**Next:** **11.10** (live `move` / `create_sprite` / `sp_kill` / NPC `sp_*`), then **15.2**.

#### Bite 15.2 — Damage

- `hurt()` and hit probe write `&life` / enemy hp. Death → `die()` script + corpse seq. Life 0 → game-over / restart as FreeDink.
- Push: `dink_base_push` when walking into hardness (`human_brain`).
- Depends on **11.10** for `sp_kill` / `sp_active` (corpses, remove). `sp_exp` / `sp_base_death` (alias `sp_base_die`) land here with DIE — they are **not** in 11.10.

#### Bite 15.3 — Weapons

- After **14.4c**. 14.5 disc is **not** enough while seq-id pixel victims are live; do not add new seq-id ranges for sword/bow. **14.6** is not this gate. Sword/bow walk packs are ~0.5–0.6 MB each plus more decoded frames; `>= 200` as “NPC walks” will eat weapon seqs or keep the wrong Screen pixels. Player-worn prefixes `graphics/dink/sword/` and `graphics/dink/bow/` are **Always** (same class as fists walk/idle), not seq-id victims. Does not replace the human gate after merge.
- `add_item` / inventory slot / `arm_weapon` load the item script on sprite **1000** and `locate` ARM (FreeDink `dc_arm_weapon`). Stock `item-sw1` keeps `base_attack` 100 and rewrites seq 71–79 / 12–18 / 102–108 via `init`/`load_sequence_now`. Leave-title grafts START-1.c fists (`item-fst`, `&cur_weapon=1`, `arm_weapon`). B with `weapon_script` and `base_hit>0` runs USE, not a C punch.
- Bow: `item-b1` USE `create_sprite` brain 11. `activate_bow` hold-to-charge is later; this bite sets last bow power to 100 so the missile can spawn. Needs **11.10** `create_sprite`.

#### Bite 15.4 — Magic

- X: if `&magic_level` ≥ `&magic_cost` and a spell armed → `locate` USE on the magic keep fiber. Else the six miss lines (`magic_script==0`). Mana from original rules (`item-fb` ARM sets `&magic_cost=100`). **This bite is just-pressed X**; meter fill while holding MAGIC is **16** (`update_status`). `preload_seq` packs are Always-until-DISARM (not named prefixes, not seq-id victims).

**Done when:** Stock early enemy can be killed with fists; arm sword via script or inventory; cast **one** stock spell from original magic script.

#### Bite 16.1 — Touch / pickup

- Brain/touch: overlap Dink → `add_item` / kill sprite. Graft FreeDink `run_through_touch_damage_list` (player brain 1, each tick). `touch_damage == -1` locates `TOUCH` (stock `s1-sack` → `add_item("item-pig")`). `>0` is `hurt_thing` + `notouch` 400 ms. `editor_type(n, 1)` is `update_play_changes` type 1 (sprite stays gone on re-enter). Types 6/7/8 timers stay **17**. Y grid is **16.2**.

#### Bite 16.2 — Inventory UI

- Y toggles. Grid of **same item ids** as PC. A arms weapon/magic. Do not invent items. Graft FreeDink `process_item` / `draw_item` (`inventory.cpp`). Seq 423 frame 1 at (20,0); weapons origin 260,83 (4×4); magic origin 45,83 (2×4); step 83×75. A sets `&cur_weapon`/`&cur_magic` then `arm_weapon`/`arm_magic`. Maple Y = `ACTION_INVENTORY`. HUD is **16.3**.

#### Bite 16.3 — HUD

- Life, mana, gold, exp from original status BMPs. Digit atlas ≤ 128 KB (`graphics/inter/numbers/`, health, magic gauge). Seq 180 chrome is overlay VRAM (640×80 wrapped in 256×256 + 64×512 sides), upload then drop CPU — same class as seq 423, not `EdGfx`. `draw_status` / `update_status` become real. Hold MAGIC (X) fills `&magic_level` every 100 ms (`update_status_all`).
- **Map:** `process_show_bmp` + seq 165 marker (stock world map). Maple **L** = FreeDink `ACTION_MAP` / `button6.c`. Y stays inventory. Pad-only.

**Done when:** Pick up a stock item, open inventory, arm it, HUD and attack seq change.

### Phase F′ — Audio (after 16; ids stay 12.x)

Silent game through V6 is OK. `playsound` stays a no-op until 12.3.

#### Bite 12.1 — Host WAV → AICA

- `tools/wav_to_adpcm` (or 16-bit PCM if sample < 8 KB).
- Document command line. Output not committed.

#### Bite 12.2 — SFX bank

- Map FreeDink sound numbers used by hit/talk/combat (from `sound/` + `playsound` ids).
- Load ≤ 512 KB at boot. Log AICA free.

#### Bite 12.3 — `playsound` bind

- Wave 1 stub becomes real. Voice steal oldest if > 16.

#### Bite 12.4 — One streamed loop

- Offline convert **one** title or town track (MIDI rendered on host → ADPCM).
- 32–64 KB disc chunks. One ring 256–512 KB.
- Title **may** start this; Bite 3.4 must still work with audio compiled out.

**Done when:** Hit plays an SFX; title or town loops one track; AICA total ≤ 2 MB.

### Phase F″ — VMU save

#### Bite 17.1 — Save blob

- Pack: version u32, `&` engine globals, `MAIN.c` globals, inventory, `&player_map`, x, y, weapon/magic, **`play.spmap` editor_type** (dead/changed sprites). **< 8 KB**.

#### Bite 17.2 — VMU

- Maple VMU port 1 / first VMU. Icon (32×32, simple). Fail softly if no VMU.

#### Bite 17.3 — Load

- Validate version. Restore. `load_screen`.

**Done when:** Save, reset ELF, load, same screen + items. No HD instant-save.

---

### Phase G — Disc and frame budget

#### Bite 18.1 — 60 / 30 FPS log

- `frame_ms` on start screen and one indoor. Target 60; **floor 30** on real hardware.

#### Bite 18.2 — CDI layout

- `mkdcdisc -d` the **`dink` tree** (basename `dink` → `/cd/dink`). Place `dink.dat`, `map.dat`, `hard.dat`, `dink.ini`, `tiles/` seek-near each other; `graphics/` after. 2048-byte sectors. A seek storm between every screen is a layout bug.

#### Bite 18.3 — 320×240 fallback

- Same logic, half-res PVR. Compile or runtime flag `DINK_VID_240`. Documented, not default.

**Done when:** Start + busy indoor ≥ 30 FPS; all §1.2 counters under cap.

---

## 3. On-disk layout (planned; not the port)

```
dinkcast/
  DREAMCAST-PORT-PLAN.md
  README.md
  Makefile                 # host + dc
  src/
    main.c                 # state machine: title / load / play / inv / save
    mem.c                  # mem_log, pool caps
    fs.c
    bmp.c
    pvr_blit.c             # quad helper
    title.c                # 3.4 present + 4.x maple
    le.c                   # read_i32le
    dinkdat.c
    mapdat.c
    harddat.c
    tiles.c
    dinkini.c
    ff.c                   # 8.5; also start-menu seq 196
    sprite.c
    edraw.c                # 8.6 editor sprite cache
    input.c
    player.c
    brains.c
    dinkc_lex.c
    dinkc_parse.c
    dinkc_vm.c
    dinkc_fns.c            # command table
    text.c
    audio.c
    inventory.c
    combat.c
    hud.c
    save_vmu.c
    title_path.h           # tiles/Splash.bmp
    start_map.h
  tools/
    docker_kos.sh
    make_cdi.sh
    check_port_plan.py
    bmp_info.c
    dump_world.c
    dump_screen.c
    dump_ini.c
    map_recsize.c
    wav_to_adpcm.c
    bmp_to_rgb565.c
  tests/
    test_le.c
    test_bmp.c
    test_dinkc_wait.c      # host VM
  data/                    # .gitkeep + README — no game blobs
```

---

## 4. Deferred (named)

| Item | Status |
|---|---|
| Dink HD extras, huge D-Mods, DFArc, WinDinkEdit | Deferred |
| Community D-Mod loader | Deferred |
| Pixel-perfect 1.08 bugs | Deferred; FreeDink 1.08 *mode* is the target |
| MIDI→ADPCM of **every** track | Deferred; 12.4 is one loop + SFX bank |
| VGA vs TV overscan polish | Deferred |
| Online multiplayer | Deferred |
| New language instead of DinkC | **Not done** |
| Custom high-perf DinkC VM / JIT “because SH-4” | **Not done** — FreeDink’s interpreter is enough; see binding decision |
| Start-menu wordmark (seq 196 `.ff`) as the 3.4 still | **Not done** — Splash.bmp is 3.4; wordmark after 8.5 |
| Native dc-chain required to develop | **Not done** — Docker KOS is enough |

---

## 5. Dependency graph

```
0.1–0.2 → 1.1–1.2 → 2.1–2.2 → 3.1–3.4 TITLE → 4.1–4.2
5.1–5.4 → 6.1–6.4 tiles → 7.1–7.4 hard
                ↘ 8.1–8.5 → 8.6 editor sprites → 9.1–9.3 walk
                         → 10.1–10.3 hooks → 11.0–11.6 DinkC wave 1
                         → 13.1–13.3 text → 11.7 wave 2
                         → 14.1–14.2 transitions
                         → 15.1 brains → 11.10 live sprite cmds → 15.2
                         → 14.4a catalog + mem_log → 14.4b pack policy → 14.5 if catalog `needed`
                         → 14.4c cpu_pixels class eviction (human go; not 14.6)
                         → 14.3 leak check
                         → 15.3–15.4
                         → 16.1–16.3 inventory / HUD (V6)
                         → 17.1–17.3 VMU (v0.3.0; requester before audio)
                         → 12.1–12.4 audio (after 16; skipped this tag)
                         → 18.1–18.3 harden
                         → 14.6 per-frame dir.ff reads (full campaign; human go)
```

**Visual gates (human must accept in Flycast/hardware before the next bite):** V1 3.4 splash **accepted**; V2 6.3 tiles; V3 8.4 Dink idle; V4 9.3 walk; V5 13.2 say box; V6 16.2/16.3 inventory+HUD. See [AGENTS.md](AGENTS.md).  
**Playable slice:** 14–16 on the opening map.

---

## 6. Why the Dreamcast can handle this

Dink is a 1990s 2D tile+sprite game. 96 tiles + a few dozen sprites is far below PowerVR2 quad throughput. Binding limits are **RAM and GD-ROM seeks**, not GPU. Stream per screen, atlas tiles, ADPCM audio, run **unmodified DinkC**. The campaign is data plus scripts; the port is an engine, not a remake.

---

## 7. Implementation notes (Dink-specific)

CDI / Docker / PVR / `/cd` classes of failure live in [docs/GOTCHAS.md](docs/GOTCHAS.md). Do not copy them here.

1. **Endian / alignment:** SH-4 can trap on misaligned 32-bit. Parse with `src/le.c`.
2. **1-based arrays:** Original C used `sprite[1..100]`. Off-by-one will desync editors.
3. **`wait` + talk:** Nested `say_stop` inside `talk` while screen `main` is waiting — fibers, not a single stack.
4. **`freeze` nesting:** Unbalanced freeze is a classic DinkC bug; match FreeDink’s counter.
5. **Transparent index:** Confirm per BMP (0 vs magenta) against FreeDink blit.
6. **Do not** port SDL_Surface. CPU blit into a framebuffer will miss 60 FPS and waste the PVR.
7. **Do not rewrite DinkC for speed.** Graft FreeDink. If a frame is slow, profile textures/disc first.
8. **Next visual after 3.4** is 6.3 (tiles), after 4.x + 5.x. Do not skip 4.1.

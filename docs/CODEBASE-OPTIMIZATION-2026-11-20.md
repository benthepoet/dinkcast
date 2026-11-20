# Dinkcast CPU / RAM / VRAM optimization findings

Date: 2026-11-20  
Code reviewed: `master` at `ac2c4b4` (through PR #163, fast twiddled texture upload)  
Scope: analysis only; no implementation, emulator run, or hardware measurement

## Purpose and constraints

Find additional ways to reduce SH-4 work and reclaim main RAM / VRAM without changing Dink Smallwood gameplay or violating the FreeDink graft and residency rules.

Lookup tables, inline assembly, and build-time conversions are available options, but they should be justified by operation counts or measurements. The existing fast-twiddle path already addresses the clearest assembly-sensitive texture-upload loop; no new assembly is justified by this static review alone.

Relevant binding constraints:

- 60 Hz logic; 30 FPS is the floor, not the design.
- Main-RAM engine target: at most 10 MB after transient BMP decode.
- `file_blob` <= 4.5 MB, `cpu_pixels` <= 2 MB, `ts_rgb` <= 1.25 MB.
- Packs follow Always / Screen / Prev; pixels follow Always / Screen / Sticky.
- Play-path sprite cache misses must not start reopening packs every frame.
- No sequence-id pins, preload-the-world policies, custom DinkC dialect, or JIT.
- PowerVR textured quads remain the renderer.
- Recent landed work already covers resident Dink frames, PAL8 sprites, PAL8 tile atlases, animation lookahead, VRAM eviction, overlay-on-demand residency, and fast twiddling. Do not duplicate those changes.

All timing improvements below are **estimates or measurement targets**, not measured Dreamcast results. Byte counts derived from sizes and array bounds are more certain, but still require instrumentation on the final implementation.

## Ranked findings

### 1. Right-size live DinkC token allocations

**Locations:** `src/dinkc_vm.c:16`, `src/dinkc_vm.c:18-39`, `src/dinkc_vm.c:489-513`, `src/dinkc_vm.c:1127-1185`; related source copy in `src/dinkc_file.c:69-86`.

Every live fiber currently allocates `4096 * sizeof(struct DinkcTok)`. On SH-4, the token structure is expected to be 16 bytes, so this is 64 KiB per fiber regardless of script size. At the existing 20-fiber cap, token arrays can reserve 1.25 MiB.

A 200-token script needs about 3.2 KiB rather than 64 KiB. Twenty such scripts would reclaim approximately 1.2 MiB.

Recommended approach:

- Use `dinkc_lex_count()` to determine the allocation size, then run the existing lexer into the exact-sized array.
- Preserve acceptance at exactly 4096 tokens, rejection at 4097, existing lexer errors, and malformed-stock-script behavior.
- Handle empty input without depending on `malloc(0)` semantics.
- Add a live token-byte counter so the saving is visible rather than implied.
- Test nested fibers, callbacks, keep fibers, choices, waits, kill/reset, and allocation failures.

Trade-off: exact sizing adds a cold-load lexing pass. An alternative is a one-pass geometrically growing array, but that introduces realloc/copy peak considerations. Do not change fiber limits or introduce a compiled-script cache in the same change.

Expected result: large, low-risk main-RAM reduction; no steady-state interpreter speed claim.

Follow-up, separately: immutable per-script token/procedure caching could remove repeated source copies and parsing in `src/script.c:165-205`, but it requires a deliberate lifetime and invalidation policy.

### 2. Bound and account for the `hard.dat` record cache

**Locations:** `src/hard.c:11-15`, `src/hard.c:85-121`; budget in `DREAMCAST-PORT-PLAN.md:107`.

Records are allocated in `g_rec[]` and never released. Stock content references approximately 672 records of 2608 bytes: about 1,752,576 bytes plus allocator overhead. This storage is outside the current `mem_log` pools and substantially exceeds the plan's 256 KiB `hard_cache` row.

Potential saving: approximately 1.4 MiB over a full campaign if the record pool is bounded near 256 KiB.

Recommended policy to evaluate:

- Account for record bytes and count in memory diagnostics.
- Keep records needed by the current screen pinned while stamping.
- Use one previous screen's record set as the preferred victim class, analogous to pack residency, rather than sequence/map-specific exceptions.
- Memoize failed record lookups per screen so a missing record does not cause a read failure on every sample.
- Compare warm neighbor ping-pong reads and `swap_ms`; naive eviction can convert a RAM saving into GD-ROM seeks.

Risk: **medium**. Do not free every record immediately after stamping without measuring neighbor-walk cost. The 600x400 current mask is another 240,000 bytes; clarify the plan's accounting for mask plus record storage before claiming compliance with the 256 KiB row.

This is potentially the largest additional campaign RAM win after token right-sizing, but it is not as safe as the direct stamping fast path below.

### 3. Stamp tile hardness from preloaded records, not through `hard_sample()` per pixel

**Locations:** `src/hard.c:186-207`, `src/hard.c:219-251`.

The first pass in `hard_stamp_tiles()` ensures every required record is loaded. The second pass nevertheless makes up to `96 * 50 * 50 = 240,000` calls to `hard_sample()`. Each call repeats bounds checks and `hard_ensure_rec()` cache logic. Empty tiles also iterate over all 2500 pixels.

Recommended approach:

- Keep the existing preload/failure pass.
- Resolve `g_rec[hid]` once per tile in the write pass.
- Skip absent or empty records.
- Read the documented on-disk transposed `[x][y]` layout directly.
- Leave `hard_sample()` unchanged for movement and other callers.
- Optionally classify all-empty/all-solid records when loaded and use row `memset` for solid tiles.

Expected result: up to 240,000 nested helper calls removed per full stamp; likely several milliseconds saved on affected swaps or scripted `draw_hard_map`, but this must be measured.

Risk: **low**, provided the disk layout and nonzero-to-boolean conversion remain identical.

Validation: build a reference mask through the public `hard_sample()` API and compare it byte-for-byte with the optimized mask on empty, solid, and mixed official screens. Retain no-reopen checks and run player, brains, hurt, and playtest tests.

### 4. Remove avoidable tile-atlas copies and use row operations

**Locations:** `src/tiles.c:152-258`, `src/tiles.c:304-342`, `src/tiles.c:344-367`, `src/tiles.c:369-417`.

Two independent costs exist:

1. Main-palette PAL8 sheet loading allocates `p8`, copies `bm.pixels`, and frees the original buffer.
2. `blit_cell()` runs general per-pixel index, clipping, and format logic for up to 240,000 pixels per atlas.

Recommended approach:

- When `bm.stride == bm.w`, transfer ownership of `bm.pixels` directly to the PAL8 sheet cache and null the Bitmap pointer before `bitmap_free()`. Retain a copy fallback for other layouts.
- Compute clipped row extents once per cell.
- Use row `memcpy` for PAL8-to-PAL8 and RGB565-to-RGB565 cells.
- Use a 256-entry `uint16_t` palette LUT for PAL8-to-RGB565 conversion instead of repeating color shifts per pixel.
- Preserve zero-filled off-sheet regions and the existing mid-build PAL8-to-RGB565 atlas conversion.

Expected result:

- One full-sheet allocation/copy avoided per newly decoded official PAL8 sheet, commonly about 160 KiB.
- At most 4800 row copies for same-format atlas stamping rather than 240,000 general pixel iterations.
- Several milliseconds may be saved on cold atlas builds; measure before claiming.

Risk: **low**. Watch allocation ownership, clipped cells, forced mixed-format tests, and palette conversion equivalence.

Validation: extend `tests/test_tiles_pal8.c` to compare every stamped cell against a reference in PAL8, RGB565, mid-build conversion, and clipped/off-sheet cases. Keep cache bytes and LRU policy unchanged.

### 5. Index `EdGfx` lookups instead of repeatedly scanning 128 entries

**Locations:** `src/edraw.c:43-55`, `src/edraw.c:288-301`; hot callers include `src/main.c:378-413`, `src/main.c:1493-1515`, and `src/main.c:1705`.

Several passes per tick search the `EdGfx` table linearly. Dense screens can produce tens of thousands of integer comparisons per tick.

Recommended approach:

- Maintain per-sequence chains or a small open-addressed `(seq, frame) -> slot` index.
- Update the index on insertion and swap-with-last removal.
- A per-sequence chain needs roughly 1 KB of heads plus one link per `EdGfx` entry.
- Do not change live flags, victim selection, CPU/VRAM accounting, or texture lifetimes.

Estimated target: 0.1-0.8 ms per dense tick, currently unmeasured.

Risk: **low-medium**. Stale indices after eviction are the main hazard.

Validation: `test_edraw`, `test_edraw_vram`, `test_io_once`, and `test_leak`; verify identical disc/frame counts and no new refusals. Use the same busy house/bar route for before/after tick counters when runtime testing is permitted.

### 6. Cache compiled PVR polygon headers for immutable textures

**Locations:** `src/sprite.c:498-524`, `src/sprite.c:595-609`, `src/saybox.c:335-357`, `src/saybox.c:399-407`.

`pvr_poly_compile()` is repeated for sprite submissions and every glyph. A say character can emit five glyph quads. Texture pointer, dimensions, format, list, and alpha mode are stable while a texture is resident.

Recommended approach:

- Cache a compiled PT header in `SpriteFrame`, invalidated whenever upload replaces `f->tex`.
- Keep a separate cached header for the font atlas.
- Initially target font/HUD atlases if a broad `SpriteFrame` metadata increase is undesirable.
- Never share a header across different texture pointers, palette banks, formats, or PVR lists.

Estimated target: 0.1-0.5 ms on ordinary frames, potentially more in long dialogue. Caching across all edraw/pspr frames may add roughly 20-30 KB of metadata; font/HUD-only caching costs less.

Risk: **medium-low**, dominated by invalidation correctness.

Validation: sprite, saybox, and status tests plus visual comparisons when permitted. Check PAL8 and ARGB1555 sprites, dialogue shadows, HUD digits, and overlays.

### 7. Make music refill decoding table-driven and use bulk ring copies

**Locations:** `src/audio.c:322-420`, `src/audio.c:424-460`, `src/audio.c:865-889`.

A 16 KiB ADPCM read produces 32,768 decoder calls and 64 KiB of PCM. Samples are individually split into a byte ring and later reassembled with modulo addressing. Refill runs on the gameplay thread and can contribute to tick spikes.

Recommended approach:

- Represent decoder state using the finite Yamaha step index.
- Generate small exact diff/next-index lookup tables from the current recurrence.
- Store PCM as `int16_t` and use at most two `memcpy` segments for ring wraparound.
- Keep tables small, approximately 2 KB or less.
- Prove bit-exact output for every reachable step/nibble combination and at track/ring wrap.

Estimated target: 30-60% reduction in music poll CPU time, currently unmeasured.

Risk: **low only if bit-exact**. Listen for clicks at loop boundaries when runtime testing is permitted.

Do not replace ADPCM with streamed PCM; that would quadruple music read bytes. Do not load whole tracks into AICA without revisiting the audio budget.

### 8. Index `dir.ff` TOC names

**Locations:** `src/ff.c:290-317`, `src/sprite.c:180-194`, `src/ff.c` TOC parsing.

TOC lookup is a linear case-insensitive scan. Frame-count discovery repeatedly calls it, producing quadratic cold-sequence probing.

Recommended approach:

- Build a small hash/index during `ff_parse_toc()`.
- Preserve first-entry-wins behavior for duplicate names and case-insensitive matching.
- Alternatively, discover sequence length in one TOC pass while still indexing ordinary lookups.
- Include index storage in TOC/file memory accounting.

Estimated target: 0.1-2 ms per cold sequence depending on pack size, plus lower play-path decode latency.

Risk: **low**.

Validation: `test_ff`, case/duplicate-name fixtures, PAL8/sprite tests, and cold frame-load timing.

## Conditional larger wins

### Measure before shrinking the PVR vertex buffer

**Location:** `src/tiles.c:434`.

The PVR initialization requests a 512 KiB vertex buffer, double-buffered internally. If measured peaks permit 256 KiB, this could free approximately 512 KiB VRAM; 192 KiB could free approximately 640 KiB.

Do not shrink blindly. Capture peaks across the bar fight, longest choices, inventory/map, and overlays. Validate VRAM low-water marks and zero new texture refusals. This requires runtime evidence and should be a separate change.

### Build-time per-map-record PAL8 atlases

**Locations:** `tools/distill_frames.py`, `tools/stage_dink.sh`, `src/tiles.c`.

The staged-data pipeline could emit indexed, optionally pre-twiddled 512x512 atlases. Runtime would prefer a staged atlas and fall back to current decoding.

Potential benefits on a hit:

- Remove sheet decode and atlas stamping from the swap.
- Reduce or eliminate the up-to-1.25 MiB `ts_rgb` decoded-sheet working set.
- Replace several reads with one sequential 256 KiB read.

Costs and risks:

- Worst-case 644 screens are approximately 161 MiB on disc before deduplication.
- Requires generated-vs-runtime atlas hash tests.
- Requires a plan amendment because this exceeds the current subset-original-BMP distill definition.
- Hardware `swap_ms` matters more than host or emulator disk timing.

## Ideas not recommended without a separate policy decision

- Global recency-only VRAM eviction, sequence-id pins, permanent pack pinning, or preloading complete animations/campaign data.
- Reopening packs on play-path cache misses.
- Dropping `story/` from Always solely to reclaim about 0.5 MB; the plan currently names it Always.
- A custom DinkC JIT or replacement interpreter.
- Baking type-0 sprites into a render-to-texture background as an early optimization; a 640x480 target adds about 1 MiB and substantial hardware risk.
- New inline assembly without a profile showing that the relevant loop remains hot after the lower-risk changes.

## Recommended sequence

1. **Token storage PR:** exact-sized arrays, byte counter, boundary/lifetime tests. This is the clearest large RAM saving.
2. **Stamping PR:** direct hardness record access, PAL8 ownership transfer, row copies, palette LUT, byte-for-byte equivalence tests. Keep it independent from token storage.
3. **Measure again:** use host operation/timing benchmarks and, when permitted, existing Dreamcast `ioplay` counters to choose among `EdGfx` indexing, PVR header caching, audio LUTs, and TOC indexing.
4. **Separate policy/measurement work:** bounded hardness records, PVR vertex-buffer sizing, and build-time atlases.

Keep each as a focused PR. Do not combine residency policy changes with mechanical fast paths.

## Validation requirements without emulator execution

For each mechanical optimization:

- Add regression/equivalence coverage that fails on the old fixed-allocation or general-loop behavior where practical.
- Run focused tests and then:

```sh
make host DINK_DATA=/home/benh/Source/freedink-data-1.08.20190120/dink
make docker-cdi DINK_DATA=/home/benh/Source/freedink-data-1.08.20190120/dink
git diff --check master...HEAD
```

- Use temporary host benchmarks for cold script loading, hardness stamping, and atlas construction. Report host numbers as host numbers, not Dreamcast frame-time claims.
- Inspect SH-4 structure sizes in the actual cross-build rather than using host `sizeof` results.
- Check allocation failures, zero-length inputs, exact capacity boundaries, clipping, SH-4 alignment, stack usage, and all cache invalidation paths.
- Leave visual, audible, hardware seek, and 60 Hz claims unverified until separately tested. No emulator was run for this review.

## Bottom line

The safest immediate savings are exact-sized DinkC token arrays and mechanical hardness/atlas fast paths. The largest additional campaign RAM opportunity is the unbounded `hard.dat` record cache, but its eviction policy needs measurement to avoid trading RAM for disc seeks. The remaining CPU ideas are plausible and worth profiling, not reasons to rewrite the VM or add speculative assembly.

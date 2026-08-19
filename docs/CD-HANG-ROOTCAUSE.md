# First-open `/cd` hang — root cause (KOS fs_iso9660 stream abort)

**Status: retired in Flycast** — sector padding (#61) plus three
consecutive clean cold boots (leave-title, house door, village traverse),
2026-08-19. Hardware/ODE confirmation still pending.

Follow-up to [FIRST-OPEN-CD-HANG.md](FIRST-OPEN-CD-HANG.md). **Analysis only.**
Not a plan bite; remedies below are options, not an approved sequence.

## Root cause

The hang matches a known KallistiOS bug: **KOS issue
[#1492](https://github.com/KallistiOS/KallistiOS/issues/1492)** —
"fs_iso9660: reading a file whose size is not a multiple of 2048 always
aborts a live GD-ROM DMA stream with data still queued" (open). The
reporter reproduced **intermittent hard hangs on real hardware** (MODE
ODE) with exactly our pattern: open → read whole file → close, repeated
for many files. Same heisenbug shape: timing-sensitive, logging masks it,
different file each boot.

This supersedes FIRST-OPEN-CD-HANG option D ("unfixable Flycast first-read
quirk"). It is a driver-level bug with a confirmed mitigation.

## Mechanism

Since KOS v2.2.0 (2025), `iso_read` on `/cd` uses the GD-ROM **DMA stream**
API for DMA-eligible reads (32-byte-aligned destination, sector-aligned
file position). Our Docker KOS image (`15.2.1-dev-08feb26`) includes it.

1. `iso_read` calls `cdrom_stream_start(sector, req_size/2048, dma=true)`
   with `req_size` = whole remaining file **rounded up** to 2048.
2. `cdrom_stream_request` pulls only what the `fread` asked for, clamped
   to EOF.
3. Any file with `size % 2048 != 0` — nearly everything on the disc
   (fence `dir.ff` 46491, grass 13527, barrels 40313 bytes, …) — can never
   drain the final partial sector's padding. `remain_size` never hits 0,
   the stream never self-closes.
4. The next `iso_close` / `iso_open` / directory read calls
   `iso_abort_stream` → `cdrom_stream_stop` → `syscall_gdrom_abort_command`
   **while sector data is still queued in the G1 DMA pipeline**. Outcome
   on hardware per the reporter: usually fine, sometimes ~4 s
   abort-timeout → `gdrom_reset` stall, intermittently a hard wedge.
   Flycast papers over part of this (drops leftover `dma_buff` on the next
   read command) but not reliably.

## Why intermittent

- The stream path only engages when the destination buffer is **32-byte
  aligned**. `dink_fread_all` reads 8 KiB chunks into a plain
  `malloc`/`realloc` buffer (newlib `fread` bypasses the stdio buffer for
  reads ≥ 1024, so the buffer address reaches `iso_read` directly). KOS
  malloc is 8-byte aligned; landing on 32 is a heap-layout lottery that
  shifts with game state (which edraw/sprite allocations ran first).
- Unaligned misses still pay one `CD_CMD_DMAREAD` command **per 2048-byte
  sector** (≈338 for `struct/home/dir.ff`), each a blocking timeout-less
  `sem_wait(&dma_done)` — hundreds of chances for a stuck completion in
  the emulator.
- Cached repeats never touch the drive (open-once policy) → run 1's full
  village loop held. Only first-opens are exposed, and which file dies
  depends on alignment + IRQ timing → different victim per boot.

## Aggravants in our tree

- `dink_fopen` (`src/fs.c`) resolves each path component with
  `stat` + `opendir`/`readdir`. Every directory read calls
  `iso_abort_stream` — so **every file transition aborts a live,
  residue-carrying stream**. We hit #1492's worst case on nearly every
  open, not just on close.
- `build/dinkcast.elf` is 3,231,864 bytes (2048×1578 **+120**). #1492
  notes an unpadded `1ST_READ.BIN` leaves the same multi-read residue at
  the BIOS→KOS handoff and caused a random wedge at KOS's first GD-ROM
  command.

## Remedy options (ranked)

**1 and 2 are implemented** (fix branch: `tools/stage_dink.sh` +
`tools/pad2048.sh` in `make_cdi.sh`; the staged tree and the boot binary
are padded, the source tree is never touched).

1. **Sector-pad data files at image build.** The reporter's confirmed
   fix: pad files to 2048-byte multiples → stream drains fully,
   self-closes, no abort. ISO9660 allocates whole sectors anyway; zero
   disc cost. Pad staged binaries (`dir.ff` packs, tile BMPs, `dink.dat`,
   `map.dat`, `hard.dat`) before mkisofs/mkdcdisc. Header-driven formats
   tolerate trailing zeros; check lexer/INI stop cleanly on NULs before
   padding text (`story/*.c`, `dink.ini`) — or route small text files
   through a deliberately unaligned buffer (clean per-sector path).
2. **Pad the ELF/`1ST_READ.BIN` to 2048** before scrambling. One build
   line.
3. **Rebuild the KOS Docker image on current master.** We predate
   `cdrom: Don't poll in cdrom_get_status()` (2026-02-21 — old code polled
   the drive from the vblank IRQ via `iso_vblank`) and the `thd_poll`
   treewide (2026-02-24). Track #1492 for the upstream stream fix; a local
   patch (floor stream size to full sectors, serve the tail via `bdread`)
   is ~10 lines in `iso_read`.
4. **Structural (aligns with plan 18.2):** whole-file reads into
   `aligned_alloc(32, …)` (one stream command per file, the pattern KOS
   examples and shipped homebrew use); skip the per-component
   `stat`/`opendir` storm by fopening the canonical uppercase-8.3 path
   directly (scan only on failure) — every avoided `readdir` is one fewer
   forced stream abort.
5. **Keep the dcload A/B** (FIRST-OPEN-CD-HANG option C) as the
   confirmation gate; `/pc` bypasses GD syscalls entirely, so it can only
   prove "the bug is in the `/cd` path", not fix hardware.

## What not to do

- **No retry loops around `cdrom_abort_cmd`.** Aborting a live DMA stream
  is the wedging operation; a watchdog that aborts-and-retries feeds the
  bug.
- No filename warm lists; no "preload the whole tree".
- `mkdcdisc` has no per-file alignment option (`-N` is whole-track
  padding) — pad in the staging step (`truncate -s` up to 2048 multiple).

## Verification (when a remedy PR lands)

- Flycast: repeat leave-title / house-door / village traverse across ≥10
  cold boots; no `ff load` without `ff ok`.
- Hardware or ODE is the verdict platform (Flycast masks part of this
  class). Judge `swap_ms` there.
- Host: blob cache behavior unchanged (`disc_opens` +1 on first, 0 on
  second).

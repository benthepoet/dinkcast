# First-open `/cd` hang (house door / mom) — review only

**Do not implement this on approval.** Critique first. Not a plan bite; not 14.3.

Root-cause analysis: [CD-HANG-ROOTCAUSE.md](CD-HANG-ROOTCAUSE.md) (KOS fs_iso9660 DMA stream abort, KOS #1492).

## What happened

**Separate boots of the same ELF/CHD** (not one session that went good → door → mom):

| Run | Result |
|---|---|
| 1 | Village held: 6 outdoor screens and in/out of all houses, no hang |
| 2 | Choked **leaving the house** (log overwritten; no serial) |
| 3 | Choked **loading mom** on leave-title (`ff load graphics/people/mom/dir.ff`, no `ok`) |
| 4 | Choked **leaving the house** again — log below |

So the same build is **intermittent**: run 1 proves the village loop can complete; later boots die on different first-opens (mom vs home pack).

`hard.dat` rec reads + no mid-file `thd_pass` + `/cd` mutex **did** get past the old 2.1 MiB slurp (runs 3–4 reached house play). They did **not** make first large `fopen`/`fread` reliable.

### Log fail — run 4 leave-house (`build/emu.log`)

This boot **did** load mom (`edraw unique 17`, pig-feed `say`, then warp). Died on the first outdoor house-sprite pack:

```
warp ed=25 map=439 xy=365,307
enter map 439 loc 337
snap swap sprite1 seq=93 y=98 act=1
swap hard keep
…
edraw load seq=93 fr=4
ff load graphics/lands/fence/dir.ff
ff ok graphics/lands/fence/dir.ff 46491
…
ff load graphics/lands/grass/dir.ff
ff ok graphics/lands/grass/dir.ff 13527
…
ff load graphics/bonuses/barrels/dir.ff
ff ok graphics/bonuses/barrels/dir.ff 40313
edraw load seq=63 fr=1
ff load graphics/struct/home/dir.ff
```

No `ff ok`. Seq 63 is `graphics/struct/home/dir.ff` **~692 KB**. Smaller fence/grass/barrels first-opens on the same swap **completed**. Silence is inside `dink_blob_get` of that pack, not warp/script/hardness.

## Diagnosis (for the other model to attack)

Same class as before, not a mom-specific or door-specific logic bug. **Four runs of the same ELF/CHD:** one full village success, later boots that died on different first-opens.

- Flycast + KOS ISO9660: a **first** sequential read of a large file can never complete (`ff load` / `tiles slurp` with no `ok`). Which file dies is not stable.
- Repeat of a cached path is fine (open-once). Run 1’s village loop shows that once packs are in, walking them again holds.
- Run 4 leave-house: fence/grass/barrels first-opens **ok**; hang is `ff load graphics/struct/home/dir.ff` (~692 KB) with no `ok`. Atlas/ts02 not reached yet.
- Mutex serializes threads; hang is **inside one READ**. Yield-removal does not help if the command never completes.

## What not to do

- Filename warm lists (`mom`, `ts02`, `walk-after-mom`, bow).
- Bite 14.3, DinkC rewrite, switching off Flycast/CHD.
- Treating “preload the whole tree” as the design (16 MB, plan §1.2).

## Options (not sequenced as “do these”)

**A. Splash-phase first visits (phase, not names)**  
After title is on screen, before Start: from **start-house map + `dink.ini`**, blob_get each unique `dir.ff` and tilesheet that screen will need (walls, details, mom, idle, walk, ts01). One file, print `ok`, optional short settle, next file. Leave-title then hits cache. Does **not** cover leave-house trees/home/ts02 unless we also walk neighbor loc ids the same way (yard map, not a string list).

**B. Same idea on every swap, bounded**  
Before edraw/atlas, from the **new** `MapScreen` + ini, blob_get unseen packs/sheets for **this screen only**. Still first-opens on the door; just names them in serial. Does not remove the Flycast first-read risk; it makes the next hang obvious and avoids a storm.

**C. A/B off GD-ROM**  
Same ELF, data from `dcload` `/pc/dink`. If house door and mom never hang, the remaining failures are Flycast `/cd`, not DinkC/edraw. Hardware/ODE still the ship check.

**D. Stop calling it a game bug we can patch per file**  
Document: CHD + open-once makes **repeats** stable; **first** large ISO read may still die in emu. Playtest protocol: if last line is `ff load`/`tiles slurp` with no `ok`, reboot; do not add another named warm.

**E. Disc layout later (plan 18.2)**  
Pack order / one container so first-reads are smaller or sequential on the track. Out of this PR unless the other model insists.

## Evidence to capture next time

Run 4’s door hang is in `build/emu.log` until the next `make emu` overwrites it. Copy hangs to `artifacts/` (or `EMU_LOG=artifacts/emu-leave-house.log make emu`).

## Verify (only if someone later implements A/B)

Host: unique start-house packs still `disc_opens` +1 on first, 0 on second. Flycast: leave-title log shows `ff ok` mom **and** `edraw unique`; door log shows `swap atlas ok` not silence after `ff load`/`tiles slurp`.

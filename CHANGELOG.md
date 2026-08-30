# Changelog

Product versions are git tags of the form `vMAJOR.MINOR.PATCH`. They are **not** plan bite ids: bite **0.1** is the repo skeleton.

The engine still needs original Dink data (`DINK_DATA`). That tree is not in git.

## [Unreleased]

## [0.4.0] — 2026-08-30

AICA SFX + streamed MIDI (**12.1–12.4**). Spec: [docs/V0.4.md](docs/V0.4.md). Requester stamped this tag.

### In this tag

- WAV→Yamaha ADPCM overlay (`make sfx-bank` / `music-bank`); never rewrite `DINK_DATA`
- START.c `load_sound` bank; `playsound` returns channel+1
- Editor loop FIRE + warp OPEN; halt hearth while START is up
- `snd_stream` 256 KiB PCM ring; callback must not `fread`; KOS `get_data` is **bytes**
- START menu `1003.mid` (not splash/load); mom `S1-H1-M.c` `dance.mid` on new game
- Pump AICA from the ring during `dink_fread_all` / `load_one` so the 16 KiB SPU buffer does not loop on edges
- `sp_speed`/`sp_dir` graft `changedir` (Chealse `hit()` zip)

### Not in this tag

- Per-frame `dir.ff` reads (plan **14.6**)
- Full-campaign test; real hardware / ODE still unproven

## [0.3.0] — 2026-08-23

START menu + VMU (**17**) plus playtest leftovers through #117. Spec: [docs/V0.3.md](docs/V0.3.md). Requester stamped this tag.

### In this tag

- Compact VMU blob, `save_game` / `load_game` / START Load, Start-pause Continue/Title — #111
- Official START wordmark (seq 196) + Start/Continue hover; empty-slot `Say_xy` — #111
- Burning-house exit + 439 `force_vision` crowd — #116
- 376 pathway after Load (Prev `dir.ff` drop on tilesheet slurp) — #114 / #115
- Playtest #117: type 0/1 hardness needs pixels; `draw_hard_map` live-only; `fade_down`/`fade_up`; `sp_disabled`; say after HUD chrome; Start-pause seq 30 overlay; choice Down does not walk

See [PROGRESS.md](PROGRESS.md) and [docs/PLAYTEST.md](docs/PLAYTEST.md).

### Not in this tag

- Audio (plan **12**), per-frame `dir.ff` reads (plan **14.6**)
- Full-campaign test; real hardware / ODE still unproven

## [0.2.0] — 2026-08-22

Campaign DinkC host slice after the village snapshot. Spec: [docs/V0.2.md](docs/V0.2.md). Canvas: [docs/canvases/v0-2-plan.canvas.tsx](docs/canvases/v0-2-plan.canvas.tsx). Requester stamped this tag; the four Flycast **Done when** pictures in that spec are still PLAYTEST Open.

### In this tag

- VM `goto` / labels (`locate_goto`) — #102
- `spawn` (`dc_spawn`) — #104
- `load_screen` + `draw_screen` (`game_load_screen` / `draw_screen_game`) — #105
- `screenlock` + `get_hard` clamp — #106
- `sp_target` / `sp_follow` on `BrainSpr` (`process_target` / `process_follow`) — #107
- `get_sprite_with_this_brain` (+ rand / next) — #108
- Playtest: HUD LEFTALIGN paper, wizard Screen live, AlkNuts `free_items` — #109

See [PROGRESS.md](PROGRESS.md) for the bite log and [docs/PLAYTEST.md](docs/PLAYTEST.md) for Flycast pictures.

### Not in this tag

- Audio (plan **12**), VMU saves (plan **17**), per-frame `dir.ff` reads (plan **14.6**)
- Flycast **Done when**: holes/letter swap, gossip/shop `goto`, Quackers `sp_follow`, screenlock arena
- Burning start-house exit (`s1-h1-s` freeze/warp) and seq **161** log spam
- Full-campaign test; real hardware / ODE still unproven

## [0.1.0] — 2026-08-22

First tagged snapshot. Village playable in Flycast with official `DINK_DATA`. Visual gates **V1–V6** and **8.6 house** are accepted.

### In this tag

- Title, start-house tiles/sprites, walk and hardness, say box, inventory, HUD
- DinkC interpreter, talk/hit, pickup, weapons and magic (bow charge is still instant-100)
- Village playtest: pigs, grain, barrels, guard, Ethel dialogue, smash leftover hardness
- Pack residency (**14.4b** / **14.4c**), campaign distill on disc (**14.5**)

See [PROGRESS.md](PROGRESS.md) for the bite log and [docs/PLAYTEST.md](docs/PLAYTEST.md) for Flycast pictures.

### Not in this tag

- Audio (plan **12**), VMU saves (plan **17**), per-frame `dir.ff` reads (plan **14.6**)
- Full-campaign test; real hardware / ODE still unproven
- Open pictures: Ethel outdoor house on first visit (map 409), Dink over smashed barrels, pig-pen south fence joint

[0.4.0]: https://github.com/benthepoet/dinkcast/releases/tag/v0.4.0
[0.3.0]: https://github.com/benthepoet/dinkcast/releases/tag/v0.3.0
[0.2.0]: https://github.com/benthepoet/dinkcast/releases/tag/v0.2.0
[0.1.0]: https://github.com/benthepoet/dinkcast/releases/tag/v0.1.0

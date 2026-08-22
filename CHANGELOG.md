# Changelog

Product versions are git tags of the form `vMAJOR.MINOR.PATCH`. They are **not** plan bite ids: bite **0.1** is the repo skeleton.

The engine still needs original Dink data (`DINK_DATA`). That tree is not in git.

## [Unreleased] — v0.3.0 (planned)

Title menu + VMU save/load. Spec: [docs/V0.3.md](docs/V0.3.md). Canvas: [docs/canvases/v0-3-plan.canvas.tsx](docs/canvases/v0-3-plan.canvas.tsx). Opens **17** before audio **12**. **14.6** stays gated.

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

[0.2.0]: https://github.com/benthepoet/dinkcast/releases/tag/v0.2.0
[0.1.0]: https://github.com/benthepoet/dinkcast/releases/tag/v0.1.0

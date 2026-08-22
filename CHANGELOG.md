# Changelog

Product versions are git tags of the form `vMAJOR.MINOR.PATCH`. They are **not** plan bite ids: bite **0.1** is the repo skeleton.

The engine still needs original Dink data (`DINK_DATA`). That tree is not in git.

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

[0.1.0]: https://github.com/benthepoet/dinkcast/releases/tag/v0.1.0

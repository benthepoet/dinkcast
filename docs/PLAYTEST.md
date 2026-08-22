# Playtest pictures

Human-confirmed Flycast/hardware pictures. Host tests in `tests/test_playtest.c` lock the engine path so these cannot silently regress. A green `make host` is **not** a visual accept for anything still in **Open**.

Add a **Confirmed** row only when the requester has seen the picture and said it works. Add a host gate in the same PR.

## Confirmed

| Picture | When | Host lock |
|---|---|---|
| Blood spray (seq 187–189) and hit-point numbers when punching pigs | 2026-08-21 | `test_playtest` pig punch |
| Grain USE no longer spams / hitch | 2026-08-21 | `test_distill` s1-sack → 430/431 |
| Grain sprite + spreading (seq 430/431) on the first outdoor screen | 2026-08-21 | `test_distill` + PVR upload after `preload_seq` |
| Feeding the pigs (map 407 box) and Milder dialogue | 2026-08-21 | (Flycast; no host lock yet) |
| Pigs visible on the **first** visit north from the first outdoor screen | 2026-08-21 | `test_edraw` house → 439 → 407 seq 41 |
| Dink left/right walk with grain equipped | 2026-08-21 | `test_ini` item-pig seq 74 center |
| Barrel smash (including first outdoor screen) | 2026-08-21 | `test_edraw` seq 173 smash |
| Pigs stay gone after kill (no snap respawn) | 2026-08-21 | `test_playtest` pig 7 after seq 164 |
| Milder feed no longer spams / hitch | 2026-08-21 | `preload_seq` frame 1 (not full walk) |
| Village-entrance guard (map 408 vis 1, seq 293) with Chealse | 2026-08-21 | `test_edraw` 408 vis 1 seq 293 |
| Ethel “I’ll find Quackers” unfreezes | 2026-08-21 | `test_dinkc_vm` extra `}` |
| Old man sprite in his house | 2026-08-21 | (Flycast; no host lock yet) |
| Start-house textures after walking back | 2026-08-21 | (Flycast; no host lock yet) |
| Village-entrance guard punch (`s1-gg` `hit()`) | 2026-08-21 | `test_script` / `test_dinkc_vm` locate `hit` |
| `say()` lines expire (Ethel “Why hello, Dink.”, X “I'm no wizard!”) | 2026-08-21 | `test_saybox` TEXT_MIN |

## Open

| Picture | Notes | Host lock |
|---|---|---|
| Ethel outdoor house (map 409 seq 63) on first visit from 408 | Prev drop before slurp malloc; not a seq-63 pin | `test_edraw` 408→409 seq 63; `test_mem` make_room |

Do not mark new pictures confirmed until the requester says so. Name them here when they report them.

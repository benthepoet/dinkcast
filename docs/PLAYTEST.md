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

## Open

- Pigs **respawn immediately** after being killed (map 407). This PR: brain 7 must `hidden` like `lsm_remove_sprite`, or the map snap draws them again.
- Old man sprite in his house.
- Start-house textures after walking back.

Do not mark these confirmed until the requester says so. Name new pictures here when they report them.

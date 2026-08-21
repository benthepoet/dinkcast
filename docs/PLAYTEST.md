# Playtest pictures

Human-confirmed Flycast/hardware pictures. Host tests in `tests/test_playtest.c` lock the engine path so these cannot silently regress. A green `make host` is **not** a visual accept for anything still in **Open**.

Add a **Confirmed** row only when the requester has seen the picture and said it works. Add a host gate in the same PR.

## Confirmed

| Picture | When | Host lock |
|---|---|---|
| Blood spray (seq 187–189) and hit-point numbers when punching pigs | 2026-08-21 | `test_playtest` pig punch |

## Open

Requester: remaining issues exist. Last reports that were **not** accepted after #97:

- Grain USE: thrown feed (seq 430/431) missing; log spam / slowdown (this PR).
- Barrel smash on the **first** outdoor screen (second screen had worked).
- Old man sprite in his house.
- Start-house textures after walking back.

Do not mark these confirmed until the requester says so. Name new pictures here when they report them.

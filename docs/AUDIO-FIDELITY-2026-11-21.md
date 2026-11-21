# Audio fidelity — 2026-11-21

Analysis only. Compared Dinkcast at `origin/master` (`e26f732`) with GNU FreeDink `sfx.cpp`, `bgm.cpp`, `dinkc_bindings.cpp`, `game_engine.cpp`, `update_frame.cpp`, `brain_keyboard.cpp`, `inventory.cpp`, `status.cpp`, `freedink.cpp`, and the official 1.08 `Story/` scripts. No engine change and no new emulator or hardware run. The only runtime evidence quoted is the existing `build/emu.log`; the AICA totals below were recalculated from the staged WAV chunks.

## Findings, in priority order

### 1. Numeric CD fallback can end in silence instead of the fallback track

FreeDink `dc_playmidi` handles a numeric name greater than 1000 by trying `N-1000.mid` and then the original name. `PlayMidi` checks that the requested file exists **before** `Mix_HaltMusic` (`bgm.cpp:104-152`). Thus, after `playmidi("1009")`, `9.mid` exists and starts, the extensionless `1009` lookup fails, and `9.mid` keeps playing.

Dinkcast performs the same two calls (`src/dinkc_cmd.c:1552-1576`), but `audio_music_play` calls `music_stop_dc()` before `open_wav()` (`src/audio.c:719-753`). `midi_to_wav_name` does not add `.wav` to an extensionless name (`src/audio.c:289-326`). The second call therefore stops the successful fallback, fails to open `Sound/1009`, clears `g_midi`, and leaves no music.

This affects stock scripts, not just D-Mods:

- `KING.c:7`: `playmidi("1011")`
- `S5-FGUY.c:91`: `playmidi("1009")`
- `S7-HELL.c:7,9`: `playmidi("1006")` / `playmidi("1018")`
- `S7-MIL.c:103,105`: `playmidi("1004")` / `playmidi("1015")`

The general rule should be “a missing new track does not destroy the currently playing track,” not a special case for these IDs. A fix needs a host/DC test that starts `9.mid`, requests extensionless `1009`, and verifies that `9.mid` is still current and playing. Host `audio_music_play` currently performs no file lookup, so the existing host test cannot catch this.

### 2. Unowned repeating SFX survive screen changes, loads, and restarts

FreeDink `draw_screen_game` calls `kill_repeat_sounds`, which stops repeating channels with `owner == 0` unless `survive` is set (`game_engine.cpp:702-708`, `sfx.cpp:481-504`). `load_game` and `game_restart` use `kill_repeat_sounds_all` (`savegame.cpp:332`, `game_engine.cpp:115-120`). Owner-driven loops are handled separately by `update_sound`.

Dinkcast `brains_reset` only halts voices whose owner is a live slot (`src/brains.c:1568-1578`). `play_draw_screen` does not halt unowned repeats (`src/main.c:620-727`), and `audio_halt_loops` is called on the title path, not every draw (`src/main.c:1291-1299`). Consequently an unowned `playsound(..., repeat=1)` can outlive its script and map.

This is observable in stock `S6-WARP.c:15-50` and `S7-WARP.c:15-50`: each starts seven unowned looping instances of sound 34, then calls `draw_screen()`. FreeDink clears those loops at that draw; Dinkcast carries them into the destination screen. The same ownership policy must cover screen draw, load, restart, and `kill_all_sounds`, including FreeDink's `survive` flag. Merely stopping every voice at every draw would be a different behavior and could cut valid owner or surviving loops.

### 3. Every music track loops; FreeDink defaults to play-once

FreeDink initializes `loop_midi = 0` and calls `Mix_PlayMusic(..., loop_midi == 1 ? -1 : 1)` (`bgm.cpp:44-48,162-165`). `loopmidi(1)` opts into repetition (`bgm.cpp:226-233`, `dinkc_bindings.cpp:1790-1793`). No active `loopmidi` call was found in the official `Story/` tree.

Dinkcast has no `loopmidi` binding or loop-mode state. At end of data, `music_fill_chunk` seeks back to `g_moff` unconditionally (`src/audio.c:392-425`). Thus every track, including START `1003.mid`, repeats forever. This changes normal campaign behavior rather than only fixing an AICA buffer underrun. Continuous music should be an explicit plan exception if it is intentional; otherwise loop mode needs to follow `loopmidi` and end-of-track state must stop cleanly.

Related missing music controls are `turn_midi_off` / `turn_midi_on`: FreeDink `check_midi` returns while `midi_active` is false (`game_engine.cpp:324-346`), while Dinkcast has no equivalent gate in `audio_music_map` (`src/audio.c:842-854`). No active stock use was found, so this is an API compatibility gap rather than a current campaign blocker.

### 4. The official SFX bank exceeds its cap; stereo accounting and truncation are wrong

Plan §1.2 budgets resident SFX at 512 KiB. `audio_init` loads every available START slot and only prints `audio warn sfx over cap` after loading (`src/audio.c:497-535`); there is no admission policy.

Using the current converted official files, after applying the loader's truncation, the unique START bank contains **578,740 bytes** of ADPCM/PCM payload, **54,452 bytes over cap**. The existing serial log corroborates the runtime warning: `audio sfx bytes=534098 slots=48 cap=524288`, followed by `audio warn sfx over cap`. This is not evidence of a 2 MiB AICA overflow—the stream currently uses another 128 KiB—but it does disprove that the stated 512 KiB policy is enforced.

The logged counter is itself **44,642 bytes too low** for this bank. `wav_nframes` returns frames per channel, but `load_dc_fp` sets `out_bytes = (nfr + 1) / 2` for every 4-bit file without multiplying by channel count (`src/audio.c:171-176,217-220`). Untruncated stereo `ESCAPE`, `NONO`, `SEL2`, `SEL3`, and `SECRET` payloads are therefore counted at half size. Even the under-reported total is 9,810 bytes over cap.

Long stereo ADPCM is also truncated to the wrong duration. For `HIGH2.WAV`, `wav_nframes` computes 75,702 frames per channel. The loader keeps `(65534 + 1) / 2 = 32,767` **total bytes** (`src/audio.c:198-215`), yielding 32,767 stereo frames per channel, about 1.49 s at 22050 Hz. Preserving 65,534 frames per channel requires 65,534 bytes across two ADPCM channels, about 2.97 s. Mono long files use the intended 32,767-byte truncation; the channel multiplier is the missing piece.

A future resource policy should first make byte accounting exact, then choose between an explicit larger AICA budget and a documented conversion/working-set policy. Arbitrarily dropping whichever slot crosses 512 KiB would silence campaign sounds and repeat the rejected one-off residency pattern. Host `test_audio` only checks a loose `< 2 MiB` raw-file counter (`tests/test_audio.c:55`), so it cannot enforce the DC budget.

### 5. START, Continue, and inventory UI sounds are missing

The hand-written UI paths do not call the SFX backend:

- `START-1.c:9-30` and `START-2.c:8-26`: focus on/off plays sounds 20 and 21. `startmenu_hover_on/off` and the presentation loop animate without them (`src/startmenu.c:66-88,393-418`).
- `START-2.c:28-43`: opening Continue plays sound 18. `startmenu_present_slots_pvr` opens the choice without it, and its slot navigation/pick also lacks the usual choice sounds 11 and 17 (`src/startmenu.c:468-542`).
- FreeDink `brain_keyboard` plays sound 18 when inventory opens (`brain_keyboard.cpp:441-444`). `process_item` plays 11 for movement, 18 at 42050 Hz for arming, and 17 on close (`inventory.cpp:343-475`). Dinkcast handles all these actions in `inv_tick` with no audio calls (`src/inv.c:263-330`).

These are visible, stock interactions and should have host call-site tests, not just checks that slots 11/17/18/20/21 loaded. In-play choice navigation and confirmation already use sounds 11 and 17 correctly (`src/main.c:1444-1458`).

### 6. Several DinkC sound commands are absent

The command table has `playsound`, `playmidi`, `stopmidi`, and `stopcd`, but no bindings for:

| Command | FreeDink behavior | Stock impact found |
|---|---|---|
| `load_sound` | Replace/load a WAV into a sound index (`dc_load_sound`) | Only START's 49 boot loads; the hard-coded table matches them |
| `sound_set_survive` | Let an unowned repeat survive normal screen cleanup | No active stock use found; needed for the complete loop policy |
| `sound_set_vol` | Adjust a playing soundbank | No active stock use found |
| `sound_set_kill` | Stop a playing soundbank | No active stock use found |
| `kill_all_sounds` | Stop all unowned repeats, including survivors | No active stock use found; same lifecycle class as finding 2 |
| `turn_midi_off` / `turn_midi_on` | Disable/enable map-triggered MIDI | No active stock use found |

`loopmidi` is also absent, but is a normal-behavior discrepancy covered separately above. Unknown commands are logged by the VM, so these are explicit compatibility gaps, not silently skipped script files. Dynamic `load_sound` needs an ownership/eviction rule if supported later; it must not bypass the AICA budget.

### 7. `sp_sound(..., 0)` does not stop the existing loop

FreeDink `update_sound` stops a repeating owner channel when either `spr[owner].sound == 0` or the owner is inactive (`sfx.cpp:534-583`). Dinkcast checks only `!g_b[i].live` in `brains_tick` (`src/brains.c:1839-1854`). `sp_sound` starts a new loop for a positive value but does not halt the old voice for zero (`src/dinkc_cmd.c:1782-1790`). The old loop therefore continues until sprite death, a global loop halt, or shutdown.

Stock `FIRE.c` and `SAVEBOT.c` only set positive sounds, so this is primarily a DinkC/D-Mod correctness gap. A robust fix should reproduce `update_sound`'s zero check and stop the owner voice when the id becomes zero; it should not invent a different positive-to-positive replacement policy.

### 8. Music-bank failures can still produce a successful, incomplete disc build

`tools/midi_bank.sh` counts conversion failures but exits 0 unconditionally (`tools/midi_bank.sh:31-32,81-105`). `make docker-cdi` consequently continues, and `stage_dink.sh` overlays whatever `build/music/*.wav` files happen to exist without checking the expected stock manifest (`tools/stage_dink.sh:83-102`). A FluidSynth, FFmpeg, or ADPCM failure can therefore yield a successful CDI with missing music.

The current build directory contains 33 converted tracks, so this is a failure-path/build-integrity finding, not a claim that today's bank is incomplete. The build should fail on any conversion error and validate the required stock names before staging. Related freshness behavior is already deliberate: `AUDIO_FORCE=1` rebuilds outputs skipped as fresh.

### 9. Smaller lifecycle and format gaps

- `audio_music_play` accepts stereo ADPCM and passes `stereo=1` to `snd_stream_start`, but `music_fill_chunk` decodes every byte through one mono YMZ history (`src/audio.c:433-445,801-825`). The converter writes left and right ADPCM streams in separate channel blocks (`tools/wav_to_adpcm.c:283-304`), so stereo music would be decoded incorrectly. Official music is converted to mono (`tools/midi_bank.sh:86-89`), making this a robustness/D-Mod issue. Rejecting unsupported channel counts is safer than pretending to support them.
- `audio_shutdown` clears pending playback only inside the Dreamcast branch and never clears `g_midi` (`src/audio.c:539-575`). On host, `audio_music_playing()` can remain true after shutdown (`src/audio.c:948-957`). Normal process teardown is unaffected, but reinit/tests can inherit stale state.

## Confirmed aligned or intentionally deferred

- START's 49 `load_sound` entries match `k_start`; the duplicate `CAVEENT.WAV` slots 25/32 share one loaded handle.
- `playsound` uses the supplied base frequency and returns a nonzero KOS channel bank. Voice count 16 and oldest-voice stealing follow plan §12.3; FreeDink instead has 20 SDL channels. Random `plus` pitch and `sound3d` pan/attenuation are not implemented (`src/audio.c:618-655`), but plan §12.3 explicitly allows those to wait. Owner metadata is still needed for loop lifetime.
- Melee/missile impact sounds, default/custom `attack_hit_sound`, player grunts, duck/pig vocals, warp OPEN/default editor sound, status ticks, and in-play choice sounds have real call sites.
- Map music 0 and -1 preserve the current track, matching `check_midi`. Same-name playing tracks skip reopen. `load_game` stops current and pending music before applying the slot. These are covered by host tests.
- `stopcd` stopping the stream is an intentional documented divergence; FreeDink's `dc_stopcd` is a no-op (`dinkc_bindings.cpp:1061-1064`, `docs/GOTCHAS.md`).
- Music refill runs on the main thread; the AICA callback does not read GD-ROM. The current 64 KiB-per-channel stream buffer, alternating mono bounce halves, and polling through save/menu/overlay work are retained. Findings 1–3 concern track semantics, not a recommendation to redesign this buffering.
- Host conversion is mono 22050 Hz MIDI→ADPCM, uses GM then optional GS soundfonts, sums stereo before limiting, and never rewrites `DINK_DATA`.

## Suggested implementation/test order

1. Preserve current music on a missing new file and lock numeric CD fallback behavior.
2. Apply one ownership/survive policy to unowned and owned loops across draw, load, restart, and `kill_all_sounds`.
3. Implement `loopmidi` default-off semantics and explicit end-of-track state.
4. Correct stereo sample/byte math, then decide and enforce an AICA budget policy without arbitrary slot victims.
5. Restore START/Continue/inventory cues and the remaining DinkC command routing.
6. Make conversion/staging fail closed; add DC-side tests for missing files, stereo truncation/accounting, loop lifetime, and reinit. Host stubs alone cannot validate these paths.

No runtime RAM, VRAM, AICA, or disc budgets change under this documentation-only PR.

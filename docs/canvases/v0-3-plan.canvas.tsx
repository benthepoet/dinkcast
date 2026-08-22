/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * Cursor canvas: v0.3.0 title menu + VMU plan.
 * Open in Cursor. Not part of `make host` or the Dreamcast ELF.
 */
import {
  Callout,
  Divider,
  Grid,
  H1,
  H2,
  H3,
  Pill,
  Row,
  Stack,
  Stat,
  Table,
  Text,
} from "cursor/canvas";

export default function V03Plan() {
  return (
    <Stack gap={24}>
      <Stack gap={8}>
        <H1>v0.3.0 — title + VMU</H1>
        <Text tone="secondary">
          Planned slice after v0.2.0. Spec: docs/V0.3.md. Playtest resume after
          ELF reset. This is not 14.6, audio 12, or campaign leftovers.
        </Text>
        <Row gap={8} wrap>
          <Pill active>From v0.2.0 host slice</Pill>
          <Pill tone="info">17 before 12</Pill>
          <Pill tone="warning">14.6 gated</Pill>
        </Row>
      </Stack>

      <Callout tone="info" title="Goal">
        Official START menu can Load, and SAVEBOT.c can Save, through a
        compact VMU blob under 8 KB. Not a PC savegame clone.
      </Callout>

      <Grid columns={3} gap={16}>
        <Stat value="5" label="PRs after this spec" />
        <Stat value="4" label="Flycast pictures to accept" />
        <Stat value="Out" label="12 / 14.6 / campaign Open" tone="warning" />
      </Grid>

      <H2>In this tag</H2>
      <Table
        stickyHeader
        striped
        headers={["#", "Work", "FreeDink", "Unlocks"]}
        rows={[
          [
            "1",
            "17.1 save blob",
            "save_game fields, sparse spmap",
            "Host file. Player, items, globals. Under 8 KB.",
          ],
          [
            "2",
            "DinkC save/load",
            "dc_save_game / load_game / game_exist",
            "&savegameinfo via load_game_small. start-2.c load().",
          ],
          [
            "3",
            "START menu",
            "START.c sprites, not main()",
            "Splash then seq 196 + start-1/2/4. Pad click. No mouse.",
          ],
          [
            "4",
            "17.2 VMU",
            "Maple first VMU + vmu_pkg",
            "Slots 1–10. Soft-fail if missing.",
          ],
          [
            "5",
            "Start pause",
            "ESCAPE.c Continue; Start = pause",
            "Continue / Title. Save is the machine only.",
          ],
        ]}
      />

      <H2>Done when (Flycast)</H2>
      <Table
        headers={["Picture", "What"]}
        rows={[
          ["START wordmark + buttons", "D-pad + A after Splash.bmp"],
          ["New Game reaches the house", "start-1.c click()"],
          ["Save machine, reset, Load", "Same map + inventory"],
          ["No VMU fails soft", "New Game still works"],
        ]}
      />

      <H2>Out</H2>
      <Text>
        AICA 12, 14.6, campaign Open pictures, burning-house exit, mouse brain
        13, START.c sound/MIDI, PC savegame files, hardware/ODE as a gate.
      </Text>

      <Divider />
      <H3>PR sequence</H3>
      <Text tone="secondary">
        docs/v0.3-plan, then 17.1, dinkc/save-load, title/start-menu, 17.2 VMU,
        play/start-pause. Human gate after each merge. Not a named V7.
      </Text>
    </Stack>
  );
}

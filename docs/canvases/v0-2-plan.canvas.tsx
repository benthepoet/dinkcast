/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * Cursor canvas: v0.2.0 campaign DinkC plan.
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

export default function V02Plan() {
  return (
    <Stack gap={24}>
      <Stack gap={8}>
        <H1>v0.2.0 — campaign DinkC</H1>
        <Text tone="secondary">
          Tagged 2026-08-22. Spec: docs/V0.2.md. Host grafts #102–#108 plus
          playtest #109. This is not 14.6, audio, or VMU.
        </Text>
        <Row gap={8} wrap>
          <Pill active>Tagged v0.2.0</Pill>
          <Pill tone="info">
            goto #102 · spawn #104 · load #105 · lock #106 · follow #107 ·
            brain #108
          </Pill>
          <Pill tone="warning">Flycast Done-when still Open</Pill>
        </Row>
      </Stack>

      <Callout tone="info" title="Stamp">
        Requester tagged this slice. Host tests lock the six grafts. The four
        Flycast pictures below were not accepted before the tag.
      </Callout>

      <Grid columns={3} gap={16}>
        <Stat value="6 / 6" label="Host grafts landed" tone="success" />
        <Stat value="4 open" label="Flycast Done-when pictures" tone="warning" />
        <Stat value="Out" label="14.6 / 12 / 17" tone="warning" />
      </Grid>

      <H2>In this tag</H2>
      <Table
        stickyHeader
        striped
        headers={["#", "Work", "FreeDink", "Unlocks"]}
        rowTone={[
          "success",
          "success",
          "success",
          "success",
          "success",
          "success",
        ]}
        rows={[
          [
            "1",
            "goto / labels (#102)",
            "locate_goto",
            "Host locked. Flycast shop/gossip still a picture.",
          ],
          [
            "2",
            "spawn (#104)",
            "dc_spawn",
            "Host locked. Bombs, dragons, cutscene scripts.",
          ],
          [
            "3",
            "load_screen + draw_screen (#105)",
            "game_load_screen / draw_screen_game",
            "Holes, letter, caves — not edge warp. Flycast picture still open.",
          ],
          [
            "4",
            "screenlock (#106)",
            "dc_screenlock + get_hard",
            "Arenas cannot walk off. No lock-bar gfx. Flycast once reachable.",
          ],
          [
            "5",
            "sp_target / follow on BrainSpr (#107)",
            "process_target / process_follow",
            "Host: BrainSpr fields, not g_target[]. Pill/dragon ATTACK; Quackers.",
          ],
          [
            "6",
            "get_sprite_with_this_brain (#108)",
            "dc_get_sprite_with_this_brain",
            "Host: live scan + rand + next. EN-PILL1 die unlock. Cult / dragon / castle.",
          ],
        ]}
      />

      <H2>Done when (Flycast — still Open)</H2>
      <Table
        headers={["Picture", "Script / system"]}
        rows={[
          ["Holes or letter actually swap", "S1-HOLE* / S1-LTR.c"],
          ["A goto loop does not fall through", "S1-LG.c and/or S2-OUT.c"],
          ["Quackers tracks Dink", "S1-DUCK.c sp_follow"],
          ["Cannot walk off a locked arena", "screenlock once reachable"],
        ]}
      />

      <H2>Also in #109 (village leftovers)</H2>
      <Text>
        HUD LEFTALIGN paper, wizard Screen live remake, AlkNuts free_items.
        Burning-house exit (s1-h1-s freeze/warp) is logged, not fixed.
      </Text>

      <H2>Out</H2>
      <Text>
        14.6 per-frame dir.ff, AICA (12), VMU save (17), say_stop_xy, bow
        charge, hardware/ODE, filling all 186 DinkC names.
      </Text>

      <Divider />
      <H3>PR sequence</H3>
      <Text tone="secondary">
        Host sequence #102–#108 merged. Playtest + stamp #109. Not a named V7.
      </Text>
    </Stack>
  );
}

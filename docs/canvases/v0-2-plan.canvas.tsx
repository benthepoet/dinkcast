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
          Planned slice after v0.1.0. Spec: docs/V0.2.md. Village PLAYTEST Open
          is empty. This is not 14.6, audio, or VMU.
        </Text>
        <Row gap={8} wrap>
          <Pill active>From v0.1.0 village</Pill>
          <Pill tone="info">goto host #102</Pill>
          <Pill tone="warning">Not full campaign</Pill>
        </Row>
      </Stack>

      <Callout tone="info" title="Goal">
        Unblock scripted progress past Stonebrook: holes/letter, goto loops,
        spawn, screenlock, and targeting/follow. Map edges already work.
      </Callout>

      <Grid columns={3} gap={16}>
        <Stat value="2 / 6" label="Host grafts landed" tone="info" />
        <Stat value="4" label="Flycast pictures to accept" />
        <Stat value="Out" label="14.6 / 12 / 17" tone="warning" />
      </Grid>

      <H2>In this tag</H2>
      <Table
        stickyHeader
        striped
        headers={["#", "Work", "FreeDink", "Unlocks"]}
        rowTone={["success", "success", "danger", "warning", "warning", "info"]}
        rows={[
          ["1", "goto / labels (#102)", "locate_goto", "Host locked. Flycast shop/gossip still a picture."],
          ["2", "spawn (host)", "dc_spawn", "Host locked. Bombs, dragons, cutscene scripts."],
          [
            "3",
            "load_screen + draw_screen",
            "game_load_screen / draw_screen_game",
            "Holes, letter, caves — not edge warp",
          ],
          ["4", "screenlock", "dc_screenlock + get_hard", "Arenas cannot walk off"],
          [
            "5",
            "sp_target / follow on BrainSpr",
            "process_target / process_follow",
            "Pill/dragon ATTACK; Quackers",
          ],
          [
            "6",
            "get_sprite_with_this_brain",
            "dc_get_sprite_with_this_brain",
            "Cult / dragon / castle scripts",
          ],
        ]}
      />

      <H2>Done when (Flycast)</H2>
      <Table
        headers={["Picture", "Script / system"]}
        rows={[
          ["Holes or letter actually swap", "S1-HOLE* / S1-LTR.c"],
          ["A goto loop does not fall through", "S1-LG.c and/or S2-OUT.c"],
          ["Quackers tracks Dink", "S1-DUCK.c sp_follow"],
          ["Cannot walk off a locked arena", "screenlock once reachable"],
        ]}
      />

      <H2>Out</H2>
      <Text>
        14.6 per-frame dir.ff, AICA (12), VMU save (17), inventory count/free
        items unless a village boot picture needs them, say_stop_xy, bow charge,
        hardware/ODE, filling all 186 DinkC names.
      </Text>

      <Divider />
      <H3>PR sequence</H3>
      <Text tone="secondary">
        One concern per branch. Human gate after each merge. Not a named V7.
      </Text>
    </Stack>
  );
}

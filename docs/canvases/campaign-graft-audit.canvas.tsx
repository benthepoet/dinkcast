/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * Cursor canvas: campaign graft audit (v0.1.0 + later host notes).
 * Open in Cursor. Not part of `make host` or the Dreamcast ELF.
 */
import {
  BarChart,
  Callout,
  Divider,
  Grid,
  H1,
  H2,
  H3,
  PieChart,
  Pill,
  Row,
  Stack,
  Stat,
  Table,
  Text,
} from "cursor/canvas";

export default function CampaignGraftAudit() {
  return (
    <Stack gap={24}>
      <Stack gap={8}>
        <H1>Campaign graft audit</H1>
        <Text tone="secondary">
          Refreshed after v0.3.0. FreeDink graft vs official 1.08 Story/
          (381 files). Full write-up: docs/CAMPAIGN-AUDIT.md.
        </Text>
        <Row gap={8} wrap>
          <Pill active>Village playable</Pill>
          <Pill tone="info">goto host #102</Pill>
          <Pill tone="warning">Full campaign incomplete</Pill>
        </Row>
      </Stack>

      <Callout tone="warning" title="Do not treat title leftovers as village P0">
        Walking off Stonebrook is engine edge/warp (14.1–14.2), not DinkC
        load_screen. New game is leave_title, not START-1.c set_mode.
        Magic is held X. Bow is instant power 100.
      </Callout>

      <Grid columns={4} gap={16}>
        <Stat value="v0.3+" label="START + VMU + village" tone="success" />
        <Stat value="~125" label="Commands used in Story/" />
        <Stat value="~52%" label="Binding coverage" tone="warning" />
        <Stat value="~55–65%" label="Plan full-campaign confidence" />
      </Grid>

      <H2>FreeDink DinkC bindings</H2>
      <Text tone="secondary" size="small">
        Source: gnu_freedink dinkc_bindings_init (186) vs dinkcast k_fn[] (98).
        Stub means in the table but silent or log-only.
      </Text>
      <Grid columns="1fr 1fr" gap={24}>
        <PieChart
          donut
          size={220}
          data={[
            { label: "Real handler", value: 74, tone: "success" },
            { label: "Stub / partial", value: 24, tone: "warning" },
            { label: "Missing from table", value: 88, tone: "danger" },
          ]}
        />
        <BarChart
          horizontal
          height={220}
          categories={["In k_fn[]", "Used in Story/", "Missing+used"]}
          series={[
            {
              name: "Count",
              data: [104, 125, 45],
              tone: "info",
            },
          ]}
          showValues
        />
      </Grid>

      <H2>Unblock the campaign first</H2>
      <Text tone="secondary" size="small">
        Named FreeDink functions. Plan bite ids stay. Visual gates still apply.
        v0.2.0 is items 1–6 (docs/V0.2.md).
      </Text>
      <Table
        stickyHeader
        striped
        headers={["Priority", "Hole", "FreeDink", "Why it matters"]}
        columnAlign={["left", "left", "left", "left"]}
        rowTone={[
          "success",
          "success",
          "success",
          "success",
          "success",
          "success",
          "warning",
          "info",
          "info",
          "neutral",
        ]}
        rows={[
          [
            "1",
            "goto",
            "locate_goto",
            "Host #102. Flycast gossip/shop loop still a picture.",
          ],
          [
            "2",
            "spawn",
            "dc_spawn",
            "Host: sprite 1000, parent continues. Bombs, dragons.",
          ],
          [
            "3",
            "load_screen / draw_screen / fade",
            "game_load_screen / draw_screen_game",
            "Host #105. Holes, caves, scripted warps — not map edges.",
          ],
          [
            "4",
            "screenlock",
            "dc_screenlock",
            "Host #106. Boss and castle arenas. No lock-bar gfx.",
          ],
          [
            "5",
            "sp_target → BrainSpr",
            "process_target / process_follow",
            "Host #107. BrainSpr fields, not g_target[]. Pill/dragon ATTACK and Quackers follow.",
          ],
          [
            "6",
            "get_sprite_with_this_brain",
            "dc_get_sprite_with_this_brain",
            "Host this PR. Live scan + rand + next. ~25 combat/cult scripts.",
          ],
          [
            "7",
            "sp_frame_delay",
            "dc_sp_frame_delay",
            "Missing. Boss and enemy timing.",
          ],
          [
            "8",
            "count_item / free_items / kill_this_item",
            "dc_count_item / dc_free_items",
            "Boot puzzle, bombs, elixir.",
          ],
          [
            "9",
            "say_stop_xy / say_xy",
            "dc_say_stop_xy",
            "King / letter / save-machine text.",
          ],
          [
            "10+",
            "17 / 12 / 14.6",
            "savegame / sfx / TOC reads",
            "VMU, AICA, enter-path RAM. After script holes.",
          ],
        ]}
      />

      <H2>Brains 0–17</H2>
      <Text tone="secondary" size="small">
        update_frame.cpp vs brains.c. Player is player_step, not brain_switch.
      </Text>
      <Table
        striped
        headers={["ID", "Name", "Status", "Gap"]}
        rowTone={[
          "warning",
          "warning",
          "success",
          "success",
          "success",
          "success",
          "warning",
          "success",
          "warning",
          "danger",
          "danger",
          "warning",
          "success",
          "neutral",
          "neutral",
          "success",
          "success",
          "warning",
        ]}
        rows={[
          ["0", "none", "Gap", "No process_follow"],
          ["1", "human", "Mixed", "No diagonal slide; base_idle unused; no +N exp floater"],
          ["2", "bounce", "Aligned", "Playfield constants vs getpic"],
          ["3", "duck", "DIE aligned", "No follow / idle SFX"],
          ["4", "pig", "Aligned", "SFX nit"],
          ["5", "one_time", "Aligned", "bg_baked type 0; y-sort still Open"],
          ["6", "repeat", "Gap", "seq_orig after brains_apply"],
          ["7", "one_time stay", "Aligned", "hidden=1"],
          ["8", "text", "Mixed", "saybox for say(); hit numbers OK"],
          ["9", "pill", "Gap", "No process_target"],
          ["10", "dragon", "Gap", "No ATTACK wait"],
          ["11", "missile", "Gap", "No HIT script / blood / sfx"],
          ["12", "scale", "Aligned", "Pickup shrink"],
          ["13", "mouse", "Deferred", "Pad port"],
          ["14", "button", "Deferred", "Title leftover"],
          ["15", "shadow", "Aligned", ""],
          ["16", "people", "Aligned", "Follow still missing"],
          ["17", "missile expire", "Gap", "Same as 11"],
        ]}
      />

      <H2>Already grafted</H2>
      <Text>
        Title through V6, talk/hit, DinkC VM + attach + external, live sprite
        cmds, edge/warp/parm_seq, 14.4 residency, distill, damage, weapons,
        touch, inventory, HUD, spmap types 1 and 3, item keep fiber 1000,
        VM goto (#102).
      </Text>

      <Divider />

      <H3>Village PLAYTEST Open is empty</H3>
      <Text>
        Confirmed 2026-08-22: Ethel outdoor house 409, smash y-sort, pig-pen
        fence. Those were occupancy and paint, not DinkC. Campaign holes continue
        at audio 12, VMU 17, and 14.6.
      </Text>
    </Stack>
  );
}

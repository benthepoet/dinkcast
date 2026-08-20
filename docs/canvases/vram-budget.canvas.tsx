/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * Cursor canvas: start-house VRAM vs plan §1.2.
 * Open in Cursor. Not part of `make host` or the Dreamcast ELF.
 */
import {
  Callout,
  Card,
  CardBody,
  CardHeader,
  Divider,
  Grid,
  H1,
  H2,
  Row,
  Stack,
  Stat,
  Swatch,
  Table,
  Text,
  UsageBar,
} from "cursor/canvas";

const FB_KB = 1200; // 640×480 RGB565 ×2
const TILE_KB = 512;
const FONT_KB = 16;
const CHOICE_BOX_KB = 640; // seq 30 frames 2–4, POT-padded ARGB1555
const CHOICE_ARROW_KB = 56; // 7+7 × 64×32
const DINK_KB = 16; // current idle/walk frame only (ds-i4 44×98 → 64×128)
const HOUSE_SPRITES_KB = 1004; // 62 unique EdGfx; measured from official dir.ff
const SPRITE_CAP_KB = 4096;
const TEX_CAP_KB = 6656; // plan 6.5 MB
const VRAM_KB = 8192;

const choiceKb = CHOICE_BOX_KB + CHOICE_ARROW_KB;
const spriteLiveKb = HOUSE_SPRITES_KB + choiceKb + DINK_KB;
const texLiveKb = TILE_KB + FONT_KB + spriteLiveKb;
const vramLiveKb = FB_KB + texLiveKb;
const spriteHeadroomKb = SPRITE_CAP_KB - spriteLiveKb;

function mb(kb: number): string {
  return `${(kb / 1024).toFixed(2)}`;
}

function pct(n: number, d: number): string {
  return `${((n / d) * 100).toFixed(0)}%`;
}

export default function VramBudget() {
  return (
    <Stack gap={24}>
      <Stack gap={8}>
        <H1>Play VRAM vs plan caps</H1>
        <Text tone="secondary">
          Start-house occupancy after the choice overlay landed. Framebuffer is
          hardware; everything else is a texture we malloc. House sprites are
          measured from official BMP sizes in dir.ff (62 unique frames, POT
          ARGB1555). Device still has no mem_log.
        </Text>
      </Stack>

      <Grid columns={4} gap={16}>
        <Stat
          value={pct(vramLiveKb, VRAM_KB)}
          label="8 MB chip used"
          tone="success"
        />
        <Stat
          value={pct(texLiveKb, TEX_CAP_KB)}
          label="6.5 MB texture cap"
          tone="success"
        />
        <Stat
          value={pct(spriteLiveKb, SPRITE_CAP_KB)}
          label="4 MB sprite_tex cap"
          tone="success"
        />
        <Stat
          value={`${choiceKb} KB`}
          label="Choice overlay pinned"
          tone="warning"
        />
      </Grid>

      <H2>8 MB PowerVR2</H2>
      <UsageBar
        total={VRAM_KB}
        topLeftLabel="Resident play (start house, girl not yet created)"
        topRightLabel={`${mb(vramLiveKb)} / 8.00 MB`}
        segments={[
          { id: "fb", value: FB_KB, color: "gray" },
          { id: "tiles", value: TILE_KB, color: "blue" },
          { id: "sprites", value: HOUSE_SPRITES_KB, color: "purple" },
          { id: "choice", value: choiceKb, color: "orange" },
          { id: "misc", value: FONT_KB + DINK_KB, color: "green" },
        ]}
      />
      <Row gap={16} wrap>
        <Row gap={8} align="center">
          <Swatch color="gray" />
          <Text size="small">Framebuffers {FB_KB} KB</Text>
        </Row>
        <Row gap={8} align="center">
          <Swatch color="blue" />
          <Text size="small">Tiles {TILE_KB} KB</Text>
        </Row>
        <Row gap={8} align="center">
          <Swatch color="purple" />
          <Text size="small">House EdGfx {HOUSE_SPRITES_KB} KB</Text>
        </Row>
        <Row gap={8} align="center">
          <Swatch color="orange" />
          <Text size="small">Choice overlay {choiceKb} KB</Text>
        </Row>
        <Row gap={8} align="center">
          <Swatch color="green" />
          <Text size="small">Font + Dink {FONT_KB + DINK_KB} KB</Text>
        </Row>
      </Row>
      <Text tone="tertiary" size="small">
        Source: plan §1.2 + host test_edraw unique 62 + POT math on freeware
        1.08 BMPs. Creating the girl (seq 331) adds 128 KB.
      </Text>

      <H2>sprite_tex 4 MB</H2>
      <UsageBar
        total={SPRITE_CAP_KB}
        topLeftLabel={`${mb(spriteHeadroomKb)} MB headroom`}
        topRightLabel={`${mb(spriteLiveKb)} / 4.00 MB`}
        segments={[
          { id: "edraw", value: HOUSE_SPRITES_KB, color: "purple" },
          { id: "choice", value: choiceKb, color: "orange" },
          { id: "dink", value: DINK_KB, color: "green" },
        ]}
      />

      <H2>Pools vs caps</H2>
      <Table
        headers={["Pool", "Cap", "Now", "Notes"]}
        columnAlign={["left", "right", "right", "left"]}
        rowTone={[
          "neutral",
          "success",
          "success",
          "success",
          "success",
          "neutral",
        ]}
        rows={[
          [
            "Framebuffers",
            "1.17 MB",
            "1.17 MB",
            "640×480 RGB565 ×2; not in the texture cap",
          ],
          [
            "tile_tex",
            "512 KB",
            "512 KB",
            "At cap; 512×512 RGB565 policy A",
          ],
          [
            "sprite_tex",
            "4 MB",
            "1.68 MB",
            "House 1004 KB + choice 696 KB + Dink 16 KB",
          ],
          ["HUD / font", "128 KB", "16 KB", "128×64 ARGB1555; V6 status still empty"],
          ["title_tex", "1 MB", "0", "Freed on leave-title (4.2)"],
          ["Texture total", "6.5 MB", "2.19 MB", "Tiles + sprites + font"],
        ]}
      />

      <Divider />

      <H2>Choice overlay (always resident)</H2>
      <Table
        headers={["Asset", "Source px", "VRAM (POT ARGB1555)", "Why padded"]}
        columnAlign={["left", "left", "right", "left"]}
        rows={[
          ["seq 30 fr 2", "192×331", "256 KB", "256×512"],
          ["seq 30 fr 3", "200×250", "128 KB", "256×256"],
          ["seq 30 fr 4", "189×330", "256 KB", "256×512"],
          ["arrows 456+457", "54×23 ×14", "56 KB", "64×32 each; 7 frames/side"],
        ]}
      />
      <Text tone="secondary">
        Loaded once at leave-title, kept off the 96-slot screen table. Unpacked
        pixels are ~378 KB; POT pad makes 696 KB resident even when the menu is
        closed. Mom walk (4 dirs × 10 frames) is 640 KB of the house total.
      </Text>

      <Callout tone="success" title="Occupancy is comfortable on the start house">
        About 42% of the 8 MB chip, 34% of the 6.5 MB texture cap, and 42% of
        sprite_tex. Creating the girl adds 128 KB. This is occupancy, not a leak
        proof: 14.3 (20 crossings) is still deferred and mem_log is not wired.
      </Callout>

      <Card>
        <CardHeader>Watch list — not tight yet, will be</CardHeader>
        <CardBody>
          <Stack gap={12}>
            <Text>
              Choice overlay is 40% of current sprite_tex use and never evicts.
              Busy outdoor (96 unique) plus combat plus that pin is the first
              pool that can actually fill. A 96-slot screen of 128×128 frames
              plus 696 KB overlay is ~3.7 / 4.0 MB.
            </Text>
            <Text>
              V6 HUD: seq 180 STAT-03 is 640×80. A raw POT upload is 256 KB,
              double the 128 KB HUD atlas cap. Needs an atlas (or crop), not
              one texture per BMP.
            </Text>
            <Text>
              Each uploaded frame also keeps an ARGB1555 CPU copy. House +
              overlay is ~1.7 MB of main RAM on top of VRAM. edraw already
              evicts unused pixels on swap because 96 decoded frames can OOM
              the 16 MB heap.
            </Text>
          </Stack>
        </CardBody>
      </Card>
    </Stack>
  );
}

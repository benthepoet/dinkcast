/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * Cursor canvas: sprite_tex packing vs residency (start house).
 * Open in Cursor. Not part of `make host` or the Dreamcast ELF.
 */
import {
  BarChart,
  Callout,
  Divider,
  H1,
  H2,
  Stack,
  Table,
  Text,
} from "cursor/canvas";

export default function SpriteTexPacking() {
  return (
    <Stack gap={24}>
      <Stack gap={8}>
        <H1>sprite_tex: residency beats packing</H1>
        <Text tone="secondary">
          Start-house numbers. Each frame is its own POT ARGB1555 texture
          today. Source pixels are 606 KB of house + 378 KB of choice; padding
          each BMP to the next power of two makes 1004 + 696 KB. Cap is 4 MB.
        </Text>
      </Stack>

      <H2>Start-house sprite_tex after each change (KB)</H2>
      <BarChart
        horizontal
        stacked
        yMax={4096}
        height={280}
        valueSuffix=" KB"
        categories={[
          "Today (all frames uploaded)",
          "Atlas every house frame",
          "Pack choice into 256×1024",
          "VRAM only current mom facing",
          "Choice upload only while open",
          "Facing + choice-on-open (menu closed)",
        ]}
        series={[
          { name: "Editor stills", data: [364, 1024, 364, 364, 364, 364] },
          { name: "Mom walk dirs", data: [640, 0, 640, 160, 640, 160] },
          { name: "Choice overlay", data: [696, 696, 512, 696, 0, 0] },
          { name: "Dink current frame", data: [16, 16, 16, 16, 16, 16] },
        ]}
        referenceLines={[
          { value: 4096, label: "4 MB cap", tone: "warning" },
        ]}
      />
      <Text tone="tertiary" size="small">
        Source: test_edraw unique 62 + POT math on freeware 1.08 dir.ff BMPs.
        Atlas row folds stills+walk into one 512×1024 sheet (same bytes as
        today’s house POT). Choice-on-open is while the menu is closed.
      </Text>

      <H2>What actually wastes bytes</H2>
      <Table
        headers={["Set", "Source pixels", "Today (per-frame POT)", "Packing note"]}
        columnAlign={["left", "right", "right", "left"]}
        rows={[
          [
            "House stills (inn, fire, props)",
            "218 KB",
            "364 KB",
            "Must stay uploaded; all on screen at once",
          ],
          [
            "Mom 4 walk dirs × 10",
            "388 KB",
            "640 KB",
            "Only one facing draws; other 480 KB is idle VRAM",
          ],
          [
            "Choice seq 30 + arrows",
            "378 KB",
            "696 KB",
            "Tall 192×331 pads to 256×512; menu is usually closed",
          ],
          ["Dink current frame", "~9 KB", "16 KB", "Already one frame"],
        ]}
      />

      <Callout tone="info" title="A sprite atlas does not shrink the start house">
        62 frames are 303k texels. That does not fit in 512×512 (262k). The
        next POT sheet is 512×1024 = 1024 KB, which matches the 1004 KB we
        already spend on per-frame POT. Tiles needed an atlas because 50 is
        an awkward cell size; these sprites already pad to 64×128.
      </Callout>

      <H2>Worth doing vs not</H2>
      <Table
        headers={["Move", "Save on this screen", "Cost / risk"]}
        columnAlign={["left", "right", "left"]}
        rowTone={["success", "success", "neutral", "warning", "danger"]}
        rows={[
          [
            "Upload choice only while the menu is open",
            "696 KB closed",
            "Evict after pvr_wait_ready. Re-upload from the CPU copy (already resident). No fopen.",
          ],
          [
            "VRAM only the walk facing being drawn",
            "480 KB (mom)",
            "CPU pixels stay (no-fopen). Upload 10 frames on turn. Same rule as Dink’s current frame.",
          ],
          [
            "Pack the three choice panels into one 256×1024",
            "184 KB always",
            "New UV blit path. Worse than not uploading it when closed.",
          ],
          [
            "PAL8 / keep 8-bit BMPs",
            "~half if it worked",
            "PVR has 1024 palette slots (four PAL8 banks). Each BMP has its own 256 colors. Cannot mix 62 palettes.",
          ],
          [
            "VQ",
            "up to ~8:1 on square pads",
            "Needs square. Colorkey edges sparkle. Plan allows it; punch-through sprites look wrong.",
          ],
        ]}
      />

      <Divider />

      <Text>
        The 4 MB cap is threatened by a 96-slot outdoor of large frames, not
        by POT waste on the start house. Residency (do not upload what is not
        on screen) is the efficient pack. An atlas engine is a later bite if
        a real screen misses sprite_tex, not a packing project for 40% occupancy.
      </Text>
    </Stack>
  );
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_SCRIPT_H
#define DINKCAST_SCRIPT_H

#include "mapscr.h"

/* Bind editor screen so talk/hit stubs can print script[16]. */
void script_bind_screen(const struct MapScreen *scr);

/* Bite 10.3 stubs: log only. DinkC is 11. */
void script_on_main(int script_id);
/* Load unique sprite + screen story files (11.0). No run. */
int script_preload_screen(void);
/* 11.6: screen MAIN then type-1 sprite main() in rank order. */
int script_attach_screen(void);
void script_on_talk(int sprite);
void script_on_hit(int sprite);

/* Last printf line (no newline). Host tests. */
const char *script_stub_log(void);

#endif

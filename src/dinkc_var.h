/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_DINKC_VAR_H
#define DINKCAST_DINKC_VAR_H

#define DINKC_MAX_VARS 250
#define DINKC_VAR_NAME 20
#define DINKC_GLOBAL_SCOPE 0

void dinkc_var_init(void);
/* 1.08: local scope then global. Unknown → 0. */
int dinkc_var_get(const char *name, int scope, int sprite);
void dinkc_var_set(const char *name, int value, int scope, int sprite);
/* Local (scope=script slot) or global. */
int dinkc_var_make(const char *name, int value, int scope);
int dinkc_var_make_global(const char *name, int value);

#endif

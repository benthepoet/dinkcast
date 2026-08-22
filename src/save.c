/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "save.h"

#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "player.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <dc/maple.h>
#include <dc/maple/vmu.h>
#include <dc/vmu_pkg.h>
#include <kos.h>
#endif

static char g_info[80] = "Level &level";
static char g_dir[256] = "build";

static int put_u8(uint8_t *d, size_t cap, size_t *o, unsigned v)
{
    if (*o + 1 > cap) {
        return -1;
    }
    d[(*o)++] = (uint8_t)v;
    return 0;
}

static int put_u16(uint8_t *d, size_t cap, size_t *o, unsigned v)
{
    if (*o + 2 > cap) {
        return -1;
    }
    d[(*o)++] = (uint8_t)v;
    d[(*o)++] = (uint8_t)(v >> 8);
    return 0;
}

static int put_i32(uint8_t *d, size_t cap, size_t *o, int v)
{
    uint32_t u = (uint32_t)v;

    if (*o + 4 > cap) {
        return -1;
    }
    d[(*o)++] = (uint8_t)u;
    d[(*o)++] = (uint8_t)(u >> 8);
    d[(*o)++] = (uint8_t)(u >> 16);
    d[(*o)++] = (uint8_t)(u >> 24);
    return 0;
}

static int put_bytes(uint8_t *d, size_t cap, size_t *o, const void *p, size_t n)
{
    if (*o + n > cap) {
        return -1;
    }
    memcpy(d + *o, p, n);
    *o += n;
    return 0;
}

static int get_u8(const uint8_t *s, size_t n, size_t *o, unsigned *v)
{
    if (*o + 1 > n) {
        return -1;
    }
    *v = s[(*o)++];
    return 0;
}

static int get_u16(const uint8_t *s, size_t n, size_t *o, unsigned *v)
{
    if (*o + 2 > n) {
        return -1;
    }
    *v = (unsigned)s[*o] | ((unsigned)s[*o + 1] << 8);
    *o += 2;
    return 0;
}

static int get_i32(const uint8_t *s, size_t n, size_t *o, int *v)
{
    uint32_t u;

    if (*o + 4 > n) {
        return -1;
    }
    u = (uint32_t)s[*o] | ((uint32_t)s[*o + 1] << 8) |
        ((uint32_t)s[*o + 2] << 16) | ((uint32_t)s[*o + 3] << 24);
    memcpy(v, &u, sizeof(u));
    *o += 4;
    return 0;
}

static int get_bytes(const uint8_t *s, size_t n, size_t *o, void *p, size_t k)
{
    if (*o + k > n) {
        return -1;
    }
    memcpy(p, s + *o, k);
    *o += k;
    return 0;
}

static void fill_info(char *dst, size_t n)
{
    const char *src = g_info;
    size_t i = 0;

    while (*src != '\0' && i + 1 < n) {
        if (src[0] == '&') {
            char name[20];
            int k = 0, val;

            while (src[k] != '\0' && k < 19 &&
                   (src[k] == '&' || src[k] == '-' || src[k] == '_' ||
                    (src[k] >= '0' && src[k] <= '9') ||
                    (src[k] >= 'A' && src[k] <= 'Z') ||
                    (src[k] >= 'a' && src[k] <= 'z'))) {
                name[k] = src[k];
                k++;
            }
            name[k] = '\0';
            val = dinkc_var_get(name, DINKC_GLOBAL_SCOPE, 1);
            i += (size_t)snprintf(dst + i, n - i, "%d", val);
            src += k;
            continue;
        }
        dst[i++] = *src++;
    }
    dst[i] = '\0';
}

int save_pack(uint8_t *dst, size_t cap, size_t *n, const struct Player *pl)
{
    size_t o = 0;
    int i, nv, ns;
    char info[80];

    if (dst == NULL || n == NULL) {
        return -1;
    }
    memset(info, 0, sizeof(info));
    fill_info(info, sizeof(info));
    if (put_i32(dst, cap, &o, (int)DINK_SAVE_MAGIC) != 0 ||
        put_i32(dst, cap, &o, (int)DINK_SAVE_VERSION) != 0) {
        return -1;
    }
    if (put_i32(dst, cap, &o,
                dinkc_var_get("&player_map", DINKC_GLOBAL_SCOPE, 1)) != 0 ||
        put_i32(dst, cap, &o, pl != NULL ? pl->x : 0) != 0 ||
        put_i32(dst, cap, &o, pl != NULL ? pl->y : 0) != 0 ||
        put_i32(dst, cap, &o, pl != NULL ? pl->dir : 2) != 0 ||
        put_i32(dst, cap, &o, pl != NULL ? pl->hitpoints : 0) != 0 ||
        put_i32(dst, cap, &o, pl != NULL ? pl->defense : 0) != 0 ||
        put_i32(dst, cap, &o, pl != NULL ? pl->strength : 0) != 0 ||
        put_i32(dst, cap, &o, DINK_BASE_WALK) != 0 ||
        put_i32(dst, cap, &o, pl != NULL ? pl->base_idle : DINK_BASE_IDLE) !=
            0 ||
        put_i32(dst, cap, &o,
                pl != NULL ? pl->base_attack : DINK_BASE_ATTACK) != 0 ||
        put_i32(dst, cap, &o, pl != NULL ? pl->base_hit : DINK_BASE_ATTACK) !=
            0 ||
        put_i32(dst, cap, &o,
                dinkc_var_get("&cur_weapon", DINKC_GLOBAL_SCOPE, 1)) != 0 ||
        put_i32(dst, cap, &o,
                dinkc_var_get("&cur_magic", DINKC_GLOBAL_SCOPE, 1)) != 0) {
        return -1;
    }
    if (put_bytes(dst, cap, &o, info, sizeof(info)) != 0) {
        return -1;
    }
    for (i = 0; i < 16; i++) {
        int act = 0, seq = 0, fr = 0;
        char name[16];

        name[0] = '\0';
        dinkc_cmd_inv_get(0, i, &act, name, sizeof(name), &seq, &fr);
        if (put_u8(dst, cap, &o, (unsigned)act) != 0 ||
            put_bytes(dst, cap, &o, name, 16) != 0 ||
            put_i32(dst, cap, &o, seq) != 0 || put_i32(dst, cap, &o, fr) != 0) {
            return -1;
        }
    }
    for (i = 0; i < 8; i++) {
        int act = 0, seq = 0, fr = 0;
        char name[16];

        name[0] = '\0';
        dinkc_cmd_inv_get(1, i, &act, name, sizeof(name), &seq, &fr);
        if (put_u8(dst, cap, &o, (unsigned)act) != 0 ||
            put_bytes(dst, cap, &o, name, 16) != 0 ||
            put_i32(dst, cap, &o, seq) != 0 || put_i32(dst, cap, &o, fr) != 0) {
            return -1;
        }
    }
    nv = dinkc_var_global_count();
    if (put_u16(dst, cap, &o, (unsigned)nv) != 0) {
        return -1;
    }
    for (i = 0; i < nv; i++) {
        char name[20];
        int val = 0;

        memset(name, 0, sizeof(name));
        if (dinkc_var_global_at(i, name, sizeof(name), &val) != 0 ||
            put_bytes(dst, cap, &o, name, 20) != 0 ||
            put_i32(dst, cap, &o, val) != 0) {
            return -1;
        }
    }
    ns = dinkc_cmd_spmap_count();
    if (put_u16(dst, cap, &o, (unsigned)ns) != 0) {
        return -1;
    }
    for (i = 0; i < ns; i++) {
        int map = 0, ed = 0, type = 0, seq = 0, fr = 0;

        if (dinkc_cmd_spmap_at(i, &map, &ed, &type, &seq, &fr) != 0 ||
            put_u16(dst, cap, &o, (unsigned)map) != 0 ||
            put_u8(dst, cap, &o, (unsigned)ed) != 0 ||
            put_u8(dst, cap, &o, (unsigned)type) != 0 ||
            put_u16(dst, cap, &o, (unsigned)(uint16_t)seq) != 0 ||
            put_u8(dst, cap, &o, (unsigned)fr) != 0) {
            return -1;
        }
    }
    if (o > DINK_SAVE_MAX) {
        return -1;
    }
    *n = o;
    return 0;
}

int save_unpack(const uint8_t *src, size_t n, struct Player *pl)
{
    size_t o = 0;
    int magic = 0, ver = 0, map = 0, x = 0, y = 0, dir = 2;
    int hp = 0, def = 0, str = 0, bw = 0, bi = 0, ba = 0, bh = 0, cw = 0,
        cm = 0;
    unsigned nv = 0, ns = 0, i;
    char info[80];

    if (src == NULL) {
        return -1;
    }
    if (get_i32(src, n, &o, &magic) != 0 || magic != (int)DINK_SAVE_MAGIC ||
        get_i32(src, n, &o, &ver) != 0 || ver != (int)DINK_SAVE_VERSION) {
        return -1;
    }
    dinkc_var_init();
    dinkc_cmd_reset_inv();
    if (get_i32(src, n, &o, &map) != 0 || get_i32(src, n, &o, &x) != 0 ||
        get_i32(src, n, &o, &y) != 0 || get_i32(src, n, &o, &dir) != 0 ||
        get_i32(src, n, &o, &hp) != 0 || get_i32(src, n, &o, &def) != 0 ||
        get_i32(src, n, &o, &str) != 0 || get_i32(src, n, &o, &bw) != 0 ||
        get_i32(src, n, &o, &bi) != 0 || get_i32(src, n, &o, &ba) != 0 ||
        get_i32(src, n, &o, &bh) != 0 || get_i32(src, n, &o, &cw) != 0 ||
        get_i32(src, n, &o, &cm) != 0 ||
        get_bytes(src, n, &o, info, sizeof(info)) != 0) {
        return -1;
    }
    info[sizeof(info) - 1] = '\0';
    memcpy(g_info, info, sizeof(g_info));
    for (i = 0; i < 16; i++) {
        unsigned act = 0;
        int seq = 0, fr = 0;
        char name[16];

        if (get_u8(src, n, &o, &act) != 0 ||
            get_bytes(src, n, &o, name, 16) != 0 ||
            get_i32(src, n, &o, &seq) != 0 || get_i32(src, n, &o, &fr) != 0) {
            return -1;
        }
        name[15] = '\0';
        dinkc_cmd_inv_put(0, (int)i, (int)act, name, seq, fr);
    }
    for (i = 0; i < 8; i++) {
        unsigned act = 0;
        int seq = 0, fr = 0;
        char name[16];

        if (get_u8(src, n, &o, &act) != 0 ||
            get_bytes(src, n, &o, name, 16) != 0 ||
            get_i32(src, n, &o, &seq) != 0 || get_i32(src, n, &o, &fr) != 0) {
            return -1;
        }
        name[15] = '\0';
        dinkc_cmd_inv_put(1, (int)i, (int)act, name, seq, fr);
    }
    if (get_u16(src, n, &o, &nv) != 0) {
        return -1;
    }
    for (i = 0; i < nv; i++) {
        char name[20];
        int val = 0;

        memset(name, 0, sizeof(name));
        if (get_bytes(src, n, &o, name, 20) != 0 ||
            get_i32(src, n, &o, &val) != 0) {
            return -1;
        }
        name[19] = '\0';
        if (name[0] == '&') {
            dinkc_var_make_global(name, val);
            dinkc_var_set(name, val, DINKC_GLOBAL_SCOPE, 1);
        }
    }
    if (get_u16(src, n, &o, &ns) != 0) {
        return -1;
    }
    for (i = 0; i < ns; i++) {
        unsigned mapu = 0, ed = 0, type = 0, sequ = 0, fr = 0;

        if (get_u16(src, n, &o, &mapu) != 0 || get_u8(src, n, &o, &ed) != 0 ||
            get_u8(src, n, &o, &type) != 0 || get_u16(src, n, &o, &sequ) != 0 ||
            get_u8(src, n, &o, &fr) != 0) {
            return -1;
        }
        dinkc_cmd_spmap_put((int)mapu, (int)ed, (int)type, (int)(int16_t)sequ,
                            (int)fr);
    }
    dinkc_var_set("&player_map", map, DINKC_GLOBAL_SCOPE, 1);
    dinkc_var_set("&cur_weapon", cw, DINKC_GLOBAL_SCOPE, 1);
    dinkc_var_set("&cur_magic", cm, DINKC_GLOBAL_SCOPE, 1);
    if (pl != NULL) {
        pl->x = x;
        pl->y = y;
        pl->dir = dir;
        pl->hitpoints = hp;
        pl->defense = def;
        pl->strength = str;
        (void)bw;
        pl->base_idle = bi;
        pl->base_attack = ba;
        pl->base_hit = bh;
        pl->seq = bi + dir;
        pl->frame = 1;
    }
    return 0;
}

void save_set_info(const char *info)
{
    if (info == NULL) {
        return;
    }
    strncpy(g_info, info, sizeof(g_info) - 1);
    g_info[sizeof(g_info) - 1] = '\0';
}

const char *save_get_info(void)
{
    return g_info;
}

void save_set_dir(const char *dir)
{
    if (dir == NULL || dir[0] == '\0') {
        return;
    }
    strncpy(g_dir, dir, sizeof(g_dir) - 1);
    g_dir[sizeof(g_dir) - 1] = '\0';
}

const char *save_host_path(int slot, char *dst, size_t n)
{
    if (dst == NULL || n < 8 || slot < 1 || slot > DINK_SAVE_SLOTS) {
        return NULL;
    }
    snprintf(dst, n, "%s/save%02d.bin", g_dir, slot);
    return dst;
}

#ifndef _arch_dreamcast
static int save_read_blob(int slot, uint8_t *dst, size_t cap, size_t *outn)
{
    char path[320];
    FILE *fp;
    size_t n;

    if (dst == NULL || outn == NULL || slot < 1 || slot > DINK_SAVE_SLOTS) {
        return -1;
    }
    if (save_host_path(slot, path, sizeof(path)) == NULL) {
        return -1;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    n = fread(dst, 1, cap, fp);
    fclose(fp);
    *outn = n;
    return 0;
}

int save_game_slot(int slot, const struct Player *pl)
{
    uint8_t buf[DINK_SAVE_MAX];
    size_t n = 0;
    char path[320];
    FILE *fp;

    if (slot < 1 || slot > DINK_SAVE_SLOTS) {
        return -1;
    }
    if (save_pack(buf, sizeof(buf), &n, pl) != 0) {
        return -1;
    }
    if (save_host_path(slot, path, sizeof(path)) == NULL) {
        return -1;
    }
    fp = fopen(path, "wb");
    if (fp == NULL) {
        printf("save_game no file %s\n", path);
        return -1;
    }
    if (fwrite(buf, 1, n, fp) != n) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

int save_game_exist(int slot)
{
    char path[320];
    FILE *fp;

    if (slot < 1 || slot > DINK_SAVE_SLOTS) {
        return 0;
    }
    if (save_host_path(slot, path, sizeof(path)) == NULL) {
        return 0;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }
    fclose(fp);
    return 1;
}
#else
static maple_device_t *first_vmu(void)
{
    return maple_enum_type(0, MAPLE_FUNC_MEMCARD);
}

/* 32×32 4bpp: green square on black. Plan 17.2. */
static uint16 g_icon_pal[16];
static uint8 g_icon_px[512];

static void vmu_icon_init(void)
{
    int y, x, i = 0;

    memset(g_icon_pal, 0, sizeof(g_icon_pal));
    memset(g_icon_px, 0, sizeof(g_icon_px));
    g_icon_pal[1] = 0x07E0;
    for (y = 0; y < 32; y++) {
        for (x = 0; x < 32; x += 2) {
            uint8 p = 0;

            if (x >= 8 && x < 24 && y >= 8 && y < 24) {
                p = 0x11;
            }
            g_icon_px[i++] = p;
        }
    }
}

static int save_read_blob(int slot, uint8_t *dst, size_t cap, size_t *outn)
{
    maple_device_t *dev;
    file_t fd;
    uint8_t *pkgbuf;
    ssize_t n;
    vmu_pkg_t pkg;
    char path[64];

    if (dst == NULL || outn == NULL || slot < 1 || slot > DINK_SAVE_SLOTS) {
        return -1;
    }
    dev = first_vmu();
    if (dev == NULL) {
        printf("load_game no VMU\n");
        return -1;
    }
    snprintf(path, sizeof(path), "/vmu/%c%d/DINK%02d", (char)('a' + dev->port),
             (int)dev->unit, slot);
    fd = fs_open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    n = fs_total(fd);
    if (n < 1 || n > 128 * 1024) {
        fs_close(fd);
        return -1;
    }
    pkgbuf = (uint8_t *)malloc((size_t)n);
    if (pkgbuf == NULL) {
        fs_close(fd);
        return -1;
    }
    if (fs_read(fd, pkgbuf, (size_t)n) != n) {
        free(pkgbuf);
        fs_close(fd);
        return -1;
    }
    fs_close(fd);
    if (vmu_pkg_parse(pkgbuf, &pkg) < 0) {
        free(pkgbuf);
        return -1;
    }
    if (pkg.data == NULL || pkg.data_len < 1 || (size_t)pkg.data_len > cap) {
        free(pkgbuf);
        return -1;
    }
    memcpy(dst, pkg.data, (size_t)pkg.data_len);
    *outn = (size_t)pkg.data_len;
    free(pkgbuf);
    return 0;
}

int save_game_slot(int slot, const struct Player *pl)
{
    uint8_t blob[DINK_SAVE_MAX];
    size_t n = 0;
    maple_device_t *dev;
    vmu_pkg_t pkg;
    uint8 *out = NULL;
    int outn = 0;
    file_t fd;
    char path[64];

    if (slot < 1 || slot > DINK_SAVE_SLOTS) {
        return -1;
    }
    if (save_pack(blob, sizeof(blob), &n, pl) != 0) {
        return -1;
    }
    dev = first_vmu();
    if (dev == NULL) {
        printf("save_game no VMU\n");
        return -1;
    }
    vmu_icon_init();
    memset(&pkg, 0, sizeof(pkg));
    strcpy(pkg.desc_short, "Dinkcast");
    strcpy(pkg.desc_long, "Dink Smallwood save");
    strcpy(pkg.app_id, "DINKCAST");
    pkg.icon_cnt = 1;
    pkg.icon_pal = g_icon_pal;
    pkg.icon_data = g_icon_px;
    pkg.data_len = (uint32)n;
    pkg.data = blob;
    if (vmu_pkg_build(&pkg, &out, &outn) < 0) {
        printf("save_game vmu_pkg fail\n");
        return -1;
    }
    snprintf(path, sizeof(path), "/vmu/%c%d/DINK%02d", (char)('a' + dev->port),
             (int)dev->unit, slot);
    fd = fs_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        printf("save_game vmu open fail %s\n", path);
        free(out);
        return -1;
    }
    if (fs_write(fd, out, outn) != outn) {
        fs_close(fd);
        free(out);
        return -1;
    }
    fs_close(fd);
    free(out);
    return 0;
}

int save_game_exist(int slot)
{
    maple_device_t *dev;
    file_t fd;
    char path[64];

    if (slot < 1 || slot > DINK_SAVE_SLOTS) {
        return 0;
    }
    dev = first_vmu();
    if (dev == NULL) {
        return 0;
    }
    snprintf(path, sizeof(path), "/vmu/%c%d/DINK%02d", (char)('a' + dev->port),
             (int)dev->unit, slot);
    fd = fs_open(path, O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    fs_close(fd);
    return 1;
}
#endif

int save_load_slot(int slot, struct Player *pl)
{
    uint8_t buf[DINK_SAVE_MAX];
    size_t n = 0;

    if (save_read_blob(slot, buf, sizeof(buf), &n) != 0) {
        return -1;
    }
    return save_unpack(buf, n, pl);
}

void save_info_line(int slot, char *dst, size_t n)
{
    uint8_t buf[DINK_SAVE_MAX];
    size_t bn = 0;

    if (dst == NULL || n < 1) {
        return;
    }
    dst[0] = '\0';
    if (slot < 1 || slot > DINK_SAVE_SLOTS) {
        snprintf(dst, n, "Nevermind");
        return;
    }
    if (save_read_blob(slot, buf, sizeof(buf), &bn) != 0) {
        snprintf(dst, n, "Slot %d - Empty", slot);
        return;
    }
    if (bn >= 4 + 4 + 13 * 4 + 80) {
        char info[80];

        memcpy(info, buf + 4 + 4 + 13 * 4, 80);
        info[79] = '\0';
        if (info[0] != '\0') {
            snprintf(dst, n, "Slot %d - %s", slot, info);
            return;
        }
    }
    snprintf(dst, n, "Slot %d", slot);
}

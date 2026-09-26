#include "game.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>

static bool wide_path(const char *p,wchar_t out[512]) {
    return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,-1,out,512)>0;
}
static FILE *save_open(const char *path,const char *mode) {
    wchar_t p[512],m[16];
    if(!wide_path(path,p)||!MultiByteToWideChar(CP_UTF8,0,mode,-1,m,16))return NULL;
    return _wfopen(p,m);
}
static int save_remove(const char *path) {wchar_t p[512];return wide_path(path,p)?_wremove(p):-1;}
static int save_rename(const char *a,const char *b) {
    wchar_t aa[512],bb[512];return wide_path(a,aa)&&wide_path(b,bb)?_wrename(aa,bb):-1;
}
#else
#define save_open fopen
#define save_remove remove
#define save_rename rename
#endif

#define OLD_SETTINGS(X)                                                                            \
    X(preset)                                                                                      \
    X(view)                                                                                        \
    X(bloom)                                                                                       \
    X(outline) X(detail) X(shadows) X(particles) X(shake) X(fov) X(fps) X(fog) X(white) X(traffic) \
        X(camera) X(sfx) X(music) X(volume) X(showfps)
#define V2_SETTINGS(X) OLD_SETTINGS(X) X(steering) X(hints)
#define ALL_SETTINGS(X) V2_SETTINGS(X) X(lighting) X(weather) X(auto_lod)
static void put32(uint8_t *b, unsigned *n, uint32_t v) {
    for (int i = 0; i < 4; i++)
        b[(*n)++] = (uint8_t)(v >> (i * 8));
}
static uint32_t get32(const uint8_t *b, unsigned *n) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
        v |= (uint32_t)b[(*n)++] << (i * 8);
    return v;
}
static uint32_t checksum(const uint8_t *b, unsigned n) {
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < n; i++) {
        h ^= b[i];
        h *= 16777619u;
    }
    return h;
}
static bool valid(const CoastProfile *p) {
    const Settings *s = &p->settings;
    if (p->selected >= CAR_COUNT || p->theme >= THEME_COUNT || !(p->cars & 1) ||
        !(p->cars & (1u << p->selected)) || !(p->themes & (1u << p->theme)))
        return false;
    uint32_t masks[] = {p->paints, p->antennas, p->hats,   p->themes,
                        p->decals, p->wheels,   p->engines};
    for (int i = 0; i < 7; i++)
        if (!(masks[i] & 1))
            return false;
    for (int c = 0; c < CAR_COUNT; c++) {
        const CarStyle *a = &p->styles[c];
        if (a->paint >= PAINT_COUNT || a->antenna >= ANTENNA_COUNT || a->hat >= HAT_COUNT ||
            a->decal >= DECAL_COUNT || a->wheel >= WHEEL_COUNT || a->engine >= ENGINE_COUNT)
            return false;
        if (!(p->paints & (1u << a->paint)) || !(p->antennas & (1u << a->antenna)) ||
            !(p->hats & (1u << a->hat)) || !(p->decals & (1u << a->decal)) ||
            !(p->wheels & (1u << a->wheel)) || !(p->engines & (1u << a->engine)))
            return false;
        for (int k = 0; k < 4; k++)
            if (p->upgrades[c][k] > 5)
                return false;
    }
    if (p->money > 1000000000u || s->preset < 0 || s->preset > 3 || s->view < 2 || s->view > 5 ||
        s->bloom < 0 || s->bloom > 3 || s->outline < 1 || s->outline > 3 || s->detail < 0 ||
        s->detail > 2 || s->traffic < 0 || s->traffic > 2 || s->camera < 0 || s->camera > 2 ||
        s->volume < 0 || s->volume > 10 || (s->fps != 30 && s->fps != 60) || s->steering < 0 ||
        s->steering > 2)
        return false;
    int flags[] = {s->shadows, s->particles, s->shake, s->fov,      s->fog,     s->white,   s->sfx,
                   s->music,   s->showfps,   s->hints, s->lighting, s->weather, s->auto_lod};
    for (unsigned i = 0; i < sizeof flags / sizeof flags[0]; i++)
        if (flags[i] < 0 || flags[i] > 1)
            return false;
    return true;
}
static bool load_one(CoastProfile *p, const char *path) {
    uint8_t b[512];
    FILE *f = save_open(path, "rb");
    if (!f)
        return false;
    size_t size = fread(b, 1, sizeof b, f);
    bool io = ferror(f) != 0;
    fclose(f);
    if (io || (size != 153 && size != 338 && size != 350 && size != 470) || memcmp(b, "CC3D", 4))
        return false;
    unsigned n = 4;
    uint32_t version = get32(b, &n);
    if ((version == 1 && size != 153) || (version == 2 && size != 338) ||
        (version == 3 && size != 350) || (version == 4 && size != 470) || version < 1 ||
        version > 4)
        return false;
    unsigned end = (unsigned)size - 4, cn = end;
    if (get32(b, &cn) != checksum(b, end))
        return false;
    CoastProfile q;
    profile_defaults(&q, false);
    q.money = get32(b, &n);
    q.best = get32(b, &n);
    q.run_count = get32(b, &n);
    q.cars = get32(b, &n);
    q.paints = get32(b, &n);
    q.antennas = get32(b, &n);
    q.hats = get32(b, &n);
    q.themes = get32(b, &n);
    if (version == 1) {
        q.selected = b[n++];
        uint8_t paint = b[n++], antenna = b[n++], hat = b[n++];
        q.theme = b[n++];
        for (int c = 0; c < CAR_COUNT; c++)
            q.styles[c] = (CarStyle){.paint = paint, .antenna = antenna, .hat = hat};
        for (int c = 0; c < 8; c++)
            for (int k = 0; k < 4; k++)
                q.upgrades[c][k] = b[n++];
#define READ_OLD(name) q.settings.name = (int)get32(b, &n);
        OLD_SETTINGS(READ_OLD)
#undef READ_OLD
    } else {
        q.decals = get32(b, &n);
        q.wheels = get32(b, &n);
        q.engines = get32(b, &n);
        q.selected = b[n++];
        q.theme = b[n++];
        for (int c = 0; c < (version >= 4 ? CAR_COUNT : 20); c++)
            for (int k = 0; k < 4; k++)
                q.upgrades[c][k] = b[n++];
        for (int c = 0; c < (version >= 4 ? CAR_COUNT : 20); c++) {
            CarStyle *a = &q.styles[c];
            a->paint = b[n++];
            a->antenna = b[n++];
            a->hat = b[n++];
            a->decal = b[n++];
            a->wheel = b[n++];
            a->engine = b[n++];
        }
#define READ_NEW(name) q.settings.name = (int)get32(b, &n);
        V2_SETTINGS(READ_NEW)
        if (version >= 3) {
            READ_NEW(lighting) READ_NEW(weather) READ_NEW(auto_lod)
        }
#undef READ_NEW
    }
    if (n != end || !valid(&q))
        return false;
    *p = q;
    return true;
}
bool profile_load(CoastProfile *p, const char *path, bool n3ds) {
    profile_defaults(p, n3ds);
    if (load_one(p, path))
        return true;
    char bak[512];
    if (snprintf(bak, sizeof bak, "%s.bak", path) >= (int)sizeof bak)
        return false;
    return load_one(p, bak);
}
bool profile_save(const CoastProfile *p, const char *path) {
    if (!valid(p))
        return false;
    char tmp[512], bak[512];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp ||
        snprintf(bak, sizeof bak, "%s.bak", path) >= (int)sizeof bak)
        return false;
    uint8_t b[512];
    memcpy(b, "CC3D", 4);
    unsigned n = 4;
    put32(b, &n, 4);
    put32(b, &n, p->money);
    put32(b, &n, p->best);
    put32(b, &n, p->run_count);
    put32(b, &n, p->cars);
    put32(b, &n, p->paints);
    put32(b, &n, p->antennas);
    put32(b, &n, p->hats);
    put32(b, &n, p->themes);
    put32(b, &n, p->decals);
    put32(b, &n, p->wheels);
    put32(b, &n, p->engines);
    b[n++] = p->selected;
    b[n++] = p->theme;
    for (int c = 0; c < CAR_COUNT; c++)
        for (int k = 0; k < 4; k++)
            b[n++] = p->upgrades[c][k];
    for (int c = 0; c < CAR_COUNT; c++) {
        const CarStyle *a = &p->styles[c];
        b[n++] = a->paint;
        b[n++] = a->antenna;
        b[n++] = a->hat;
        b[n++] = a->decal;
        b[n++] = a->wheel;
        b[n++] = a->engine;
    }
#define WRITE(name) put32(b, &n, (uint32_t)p->settings.name);
    ALL_SETTINGS(WRITE)
#undef WRITE
    uint32_t sum = checksum(b, n);
    put32(b, &n, sum);
    FILE *f = save_open(tmp, "wb");
    if (!f)
        return false;
    bool ok = fwrite(b, 1, n, f) == n;
    if (fflush(f))
        ok = false;
    if (fclose(f))
        ok = false;
    if (!ok) {
        save_remove(tmp);
        return false;
    }
    CoastProfile check;
    if (load_one(&check, path)) {
        save_remove(bak);
        if (save_rename(path, bak)) {
            save_remove(tmp);
            return false;
        }
    } else
        save_remove(path);
    if (save_rename(tmp, path)) {
        save_rename(bak, path);
        return false;
    }
    return true;
}

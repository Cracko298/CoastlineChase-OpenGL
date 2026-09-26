#include "render.h"
#ifdef COAST_DESKTOP
#include "desktop.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>
static const Color WHITE = {.86f, .94f, 1, 1}, MUTED = {.38f, .51f, .64f, 1},
                   CYAN = {.18f, .9f, 1, 1}, PINK = {1, .23f, .55f, 1}, YELLOW = {1, .72f, .25f, 1},
                   PANEL = {.026f, .044f, .075f, .97f}, BG = {.013f, .022f, .04f, 1};

static const uint8_t LETTERS[36][7] = {
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},
    {30, 1, 1, 14, 1, 1, 30},     {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},
    {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15, 1, 1, 14},   {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
    {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31},
    {31, 16, 16, 30, 16, 16, 16}, {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
    {14, 4, 4, 4, 4, 4, 14},      {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17},
    {16, 16, 16, 16, 16, 16, 31}, {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
    {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13},
    {30, 17, 17, 30, 20, 18, 17}, {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},
    {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10},
    {17, 17, 10, 4, 10, 17, 17},  {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31}};
uint8_t coast_glyph(char c, int row) {
    if (c >= 'a' && c <= 'z')
        c -= 32;
    if (c >= '0' && c <= '9')
        return LETTERS[c - '0'][row];
    if (c >= 'A' && c <= 'Z')
        return LETTERS[10 + c - 'A'][row];
    switch (c) {
    case ':':
        return row == 2 || row == 5 ? 4 : 0;
    case '.':
        return row == 6 ? 4 : 0;
    case ',':
        return row == 5 ? 4 : row == 6 ? 8 : 0;
    case '/':
        return row < 5 ? (1u << row) : 0;
    case '-':
        return row == 3 ? 14 : 0;
    case '+':
        return row == 3 ? 14 : row == 2 || row == 4 ? 4 : 0;
    case '&': {
        static const uint8_t a[] = {12, 18, 20, 8, 21, 18, 13};
        return a[row];
    }
    case '$': {
        static const uint8_t a[] = {4, 15, 20, 14, 5, 30, 4};
        return a[row];
    }
    case '%': {
        static const uint8_t a[] = {25, 25, 2, 4, 8, 19, 19};
        return a[row];
    }
    case '>':
        return row == 1 || row == 5 ? 8 : row == 2 || row == 4 ? 4 : row == 3 ? 2 : 0;
    case '<':
        return row == 1 || row == 5 ? 2 : row == 2 || row == 4 ? 4 : row == 3 ? 8 : 0;
    case '!':
        return row < 5 || row == 6 ? 4 : 0;
    case '?': {
        static const uint8_t a[] = {14, 17, 1, 2, 4, 0, 4};
        return a[row];
    }
    case '[':
        return row == 0 || row == 6 ? 14 : 8;
    case ']':
        return row == 0 || row == 6 ? 14 : 2;
    case '*':
        return row == 2 ? 21 : row == 3 ? 14 : row == 4 ? 21 : 0;
    default:
        return 0;
    }
}
void r_text(Render *r, float x, float y, float scale, Color c, const char *text) {
#ifdef COAST_DESKTOP
    text = desktop_text(text);
#endif
    float start = x;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            y += 9 * scale;
            x = start;
            continue;
        }
        for (int row = 0; row < 7; row++) {
            unsigned bits = coast_glyph(*p, row);
            int run = -1;
            for (int col = 0; col <= 5; col++) {
                bool on = col < 5 && (bits & (1u << (4 - col)));
                if (on && run < 0)
                    run = col;
                if (!on && run >= 0) {
                    r_rect(r, x + run * scale, y + row * scale, (col - run) * scale, scale, c);
                    run = -1;
                }
            }
        }
        x += 6 * scale;
    }
}
static void right(Render *r, float x, float y, float s, Color c, const char *t) {
    r_text(r, x - (float)strlen(t) * 6 * s, y, s, c, t);
}
static void bar(Render *r, float x, float y, float w, float value, Color c) {
    r_rect(r, x, y, w, 4, (Color){.08f, .12f, .18f, 1});
    r_rect(r, x, y, w * clampf(value, 0, 1), 4, c);
}
void render_hud(Render *r, const Game *g) {
    char b[100];
    bool title = g->screen == TITLE || (g->screen == HELP && g->return_screen == TITLE) ||
                 (g->screen == SETTINGS && g->return_screen == TITLE);
    if (title) {
        r_rect(r, 0, 0, 400, 78, (Color){.012f, .018f, .036f, .84f});
        r_text(r, 19, 16, 3.0f, WHITE, "COASTLINE");
        r_text(r, 20, 45, 2, PINK, "C H A S E");
        r_text(r, 21, 67, 1, MUTED, "ISLAND CITY / V0.5");
        r_rect(r, 14, 211, 372, 21, PANEL);
        r_text(r, 23, 218, 1, CYAN, "32 RIDES / CUSTOM BUILDS / ENDLESS PURSUIT");
    } else if (g->screen == SHOP || g->screen == GARAGE) {
        int model = preview_car(g);
        CarStyle style = preview_style(g);
        CarSpec c = car_tuned(&g->profile, model, style.engine, style.wheel);
        if (g->screen == SHOP && g->tab == SHOP_UPGRADES &&
            g->profile.upgrades[model][g->cursor] < 5) {
            if (g->cursor == 0) {
                c.speed += .45f;
                c.accel += .4f;
            }
            if (g->cursor == 1)
                c.grip += .35f;
            if (g->cursor == 2)
                c.armor += 20;
        }
        r_rect(r, 0, 0, 400, 40, PANEL);
        r_text(r, 14, 9, 1.8f, WHITE, CARS[model].name);
        r_text(r, 15, 29, 1, CYAN,
               g->screen == GARAGE                   ? "YOUR CURRENT CAR"
               : shop_equipped(g, g->tab, g->cursor) ? "CURRENTLY FITTED"
                                                     : "LIVE PREVIEW / NOT YET FITTED");
        r_rect(r, 10, 176, 380, 57, PANEL);
        snprintf(b, sizeof b, "TOP %d KM/H   POWER %.1f   GRIP %.1f", (int)(c.speed * 3.6f),
                 c.accel, c.grip);
        r_text(r, 18, 183, 1, WHITE, b);
        snprintf(b, sizeof b, "HULL %d   %s", (int)c.armor, ENGINES[style.engine].name);
        r_text(r, 18, 199, 1, MUTED, b);
        r_text(r, 18, 219, 1, CYAN,
               g->screen == SHOP ? "LEFT/RIGHT ROTATE / X RESET VIEW"
                                 : "CHOOSE A GARAGE CATEGORY BELOW");
    } else if (g->screen == RESULT) {
        r_rect(r, 25, 35, 350, 166, PANEL);
        r_text(r, 45, 51, 2, g->reason == 2 ? CYAN : PINK,
               g->reason == 0   ? "RIDE WRECKED"
               : g->reason == 1 ? "BUSTED"
                                : "RUN BANKED");
        snprintf(b, sizeof b, "$%d EARNED", g->last_payout);
        r_text(r, 45, 82, 2, YELLOW, b);
        snprintf(b, sizeof b, "%d M / %d S / WAVE %d", (int)g->distance, (int)g->run_time, g->wave);
        r_text(r, 45, 112, 1, WHITE, b);
        snprintf(b, sizeof b, "DRIFT %d PTS / CASH +$%d", (int)g->drift_score,
                 (int)(g->drift_score / 10));
        r_text(r, 45, 133, 1, MUTED, b);
        snprintf(b, sizeof b, "BEST %u M / BANK $%u", (unsigned)g->profile.best,
                 (unsigned)g->profile.money);
        r_text(r, 45, 158, 1, CYAN, b);
        r_text(r, 45, 181, 1, MUTED, g->world.name);
    } else {
        r_rect(r, 8, 8, 101, 33, PANEL);
        snprintf(b, sizeof b, "%03d", (int)(vehicle_speed(&g->player) * 3.6f));
        r_text(r, 15, 14, 2.1f, WHITE, b);
        r_text(r, 59, 29, 1, MUTED, "KM/H");
        r_rect(r, 256, 8, 136, 33, PANEL);
        snprintf(b, sizeof b, "$%05d", g->earned);
        right(r, 384, 14, 1.5f, YELLOW, b);
        snprintf(b, sizeof b, "HEAT %d / UNITS %d", (int)ceilf(g->heat),
                 game_police_budget(game_pressure(g)));
        right(r, 384, 32, 1, PINK, b);
        if (g->notice_time > 0) {
            float width = fminf(388, strlen(g->notice) * 6 + 16);
            r_rect(r, (400 - width) / 2, 47, width, 19, PANEL);
            r_text(r, (400 - width) / 2 + 8, 53, 1, WHITE, g->notice);
        } else if (g->run_time < 16) {
            r_rect(r, 40, 47, 320, 19, PANEL);
            r_text(r, 48, 53, 1, CYAN, "A GAS / B BRAKE / R DRIFT / L SMALL NITRO BOOST");
        }
        if (g->giant > 0 || g->shield > 0 || g->magnet > 0) {
            const char *power = g->giant > 0    ? "MEGA RIDE"
                                : g->shield > 0 ? "SHIELD"
                                                : "CASH MAGNET";
            float seconds = g->giant > 0 ? g->giant : g->shield > 0 ? g->shield : g->magnet;
            snprintf(b, sizeof b, "%s %dS", power, (int)ceilf(seconds));
            r_text(r, 8, 210, 1, PAINTS[3], b);
        }
        if (g->boosting)
            r_text(r, 170, 207, 1.3f, CYAN, "NITRO");
        else if (g->drift_grace > 0) {
            snprintf(b, sizeof b, "DRIFT X%d / %d PTS",
                     1 + (int)fminf(3, floorf(g->drift_chain / 2)), (int)g->drift_score);
            r_rect(r, 122, 202, 198, 20, PANEL);
            r_text(r, 130, 208, 1, PINK, b);
        }
        if (g->bust > .1f) {
            r_rect(r, 106, 177, 188, 22, PANEL);
            r_text(r, 114, 183, 1, PINK, "BUSTING");
            bar(r, 169, 184, 115, g->bust / 5, PINK);
        }
        if (game_heli_budget(game_pressure(g)) > 0) {
            snprintf(b, sizeof b, "AIR UNITS %d", game_heli_budget(game_pressure(g)));
            r_text(r, 8, 230, 1, PINK, b);
        }
        if (g->run_time < 16) {
            r_rect(r, 13, 220, 374, 14, PANEL);
            r_text(r, 20, 224, 1, MUTED, "STEER GENTLY / BRAKE BEFORE CORNERS / X CAMERA");
        }
        if (g->screen == PAUSE || g->screen == SETTINGS || g->screen == CITYMAP ||
            g->screen == HELP) {
            r_rect(r, 128, 104, 144, 28, PANEL);
            r_text(r, 146, 114, 1.4f, WHITE, g->screen == CITYMAP ? "CITY MAP" : "PAUSED");
        }
        if (g->flash > 0)
            r_rect(r, 0, 0, 400, 240, (Color){1, .04f, .07f, g->flash});
    }
    if (g->profile.settings.showfps) {
        snprintf(b, sizeof b, "%.0f FPS", g->measured_fps);
        right(r, 393, 232, 1, MUTED, b);
    }
    if (g->save_failed) {
        r_rect(r, 10, 215, 380, 18, (Color){.22f, .025f, .04f, 1});
        r_text(r, 18, 221, 1, WHITE, "SAVE FAILED / CHECK SD CARD SPACE");
    }
    if (g->screen == TITLE && g->notice_time > 0 && strstr(g->notice, "AUDIO")) {
        r_rect(r, 8, 184, 384, 18, PANEL);
        r_text(r, 16, 190, 1, YELLOW, g->notice);
    }
}
static void header(Render *r, const char *title, const Game *g) {
    char b[40];
    r_rect(r, 0, 0, 320, 37, PANEL);
    r_text(r, 12, 12, 1.5f, WHITE, title);
    snprintf(b, sizeof b, "$%u", (unsigned)g->profile.money);
    right(r, 308, 17, 1, YELLOW, b);
    r_rect(r, 12, 36, 296, 1, (Color){.12f, .32f, .39f, 1});
}
static void footer(Render *r, const char *s) {
    r_rect(r, 0, 213, 320, 27, PANEL);
    r_text(r, 12, 224, 1, MUTED, s);
}
static void row(Render *r, float y, const char *name, const char *value, bool selected) {
    r_rect(r, 10, y, 300, 25, selected ? (Color){.045f, .16f, .21f, 1} : PANEL);
    if (selected)
        r_rect(r, 10, y, 3, 25, CYAN);
    r_text(r, 20, y + 9, 1, selected ? WHITE : MUTED, name);
    if (value)
        right(r, 300, y + 9, 1, selected ? CYAN : MUTED, value);
}
static Color district_color(int k) {
    static const Color c[] = {{.015f, .075f, .12f, 1}, {.31f, .16f, .32f, 1}, {.19f, .19f, .25f, 1},
                              {.12f, .26f, .20f, 1},   {.13f, .24f, .32f, 1}, {.07f, .29f, .15f, 1},
                              {.32f, .22f, .12f, 1},   {.34f, .30f, .18f, 1}, {.24f, .23f, .35f, 1},
                              {.41f, .40f, .31f, 1},   {.41f, .40f, .31f, 1}};
    return c[k <= BRIDGE_NS ? k : 0];
}
static void cliprect(Render *r, float x, float y, float w, float h, float bx, float by, float bw,
                     float bh, Color c) {
    float x0 = fmaxf(x, bx), y0 = fmaxf(y, by), x1 = fminf(x + w, bx + bw),
          y1 = fminf(y + h, by + bh);
    if (x1 > x0 && y1 > y0)
        r_rect(r, x0, y0, x1 - x0, y1 - y0, c);
}
static void map_route(Render *r, const Game *g, float bx, float by, float width, float height,
                      float ox, float oy, float scale) {
    if (g->waypoint < 0)
        return;
    int node = world_nearest(&g->world, g->player.x, g->player.z);
    for (int step = 0; step < 96 && node != g->waypoint; step++) {
        Vec3 a = world_center(&g->world, node);
        int next = -1;
        for (int d = 0; d < 4; d++) {
            int n = world_neighbor(&g->world, node, d);
            if (n >= 0 && g->gps_field[n] >= 0 && g->gps_field[n] < g->gps_field[node]) {
                next = n;
                break;
            }
        }
        if (next < 0)
            break;
        Vec3 b = world_center(&g->world, next);
        for (int i = 0; i < 5; i++) {
            float x = ox + (a.x + (b.x - a.x) * i / 5) * scale,
                  y = oy - (a.z + (b.z - a.z) * i / 5) * scale;
            cliprect(r, x - 1, y - 1, 2, 2, bx, by, width, height, YELLOW);
        }
        node = next;
    }
}
static void map_land(Render *r, const World *world, int x, int z, float ox, float oy, float scale,
                     float bx, float by, float width, float height, Color color) {
    if (x < 0 || z < 0 || x >= MAP_SIDE || z >= MAP_SIDE)
        return;
    const Tile *tile = world_tile(world, x, z);
    bool all = world->shore_kind[z * MAP_SIDE + x] == 2;
    if (all) {
        cliprect(r, ox + x * CELL * scale, oy - (z + 1) * CELL * scale, CELL * scale, CELL * scale,
                 bx, by, width, height, color);
        return;
    }
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++) {
            float px = x * CELL + (i + .5f) * SHORE_STEP, pz = z * CELL + (j + .5f) * SHORE_STEP;
            if (world_coast(world, px, pz) >= 0)
                cliprect(r, ox + (px - SHORE_STEP / 2) * scale, oy - (pz + SHORE_STEP / 2) * scale,
                         SHORE_STEP * scale, SHORE_STEP * scale, bx, by, width, height, color);
        }
    if (tile->kind >= BRIDGE_EW) {
        Vec3 c = world_center(world, z * 32 + x);
        float w = tile->kind == BRIDGE_EW ? CELL : ROAD, h = tile->kind == BRIDGE_EW ? ROAD : CELL;
        cliprect(r, ox + (c.x - w / 2) * scale, oy - (c.z + h / 2) * scale, w * scale, h * scale,
                 bx, by, width, height, color);
    }
}
static void minimap(Render *r, const Game *g) {
    float bx = 12, by = 49, w = 146, h = 147, scale = .41f, ox = bx + w / 2 - g->player.x * scale,
          oy = by + h / 2 + g->player.z * scale;
    r_rect(r, bx, by, w, h, district_color(OCEAN));
    int cx = (int)(g->player.x / CELL), cz = (int)(g->player.z / CELL);
    for (int z = cz - 4; z <= cz + 4; z++)
        for (int x = cx - 4; x <= cx + 4; x++) {
            const Tile *t = world_tile(&g->world, x, z);
            map_land(r, &g->world, x, z, ox, oy, scale, bx, by, w, h,
                     district_color(t->kind ? t->kind : MARINA));
            if (!t->kind)
                continue;
            int n = z * 32 + x;
            Vec3 a = world_center(&g->world, n);
            for (int d = 0; d < 4; d++) {
                int q = world_neighbor(&g->world, n, d);
                if (q < 0)
                    continue;
                Vec3 end = world_center(&g->world, q);
                for (int k = 0; k < 6; k++) {
                    float f = k / 12.f;
                    cliprect(r, ox + (a.x + (end.x - a.x) * f) * scale - 1,
                             oy - (a.z + (end.z - a.z) * f) * scale - 1, 2, 2, bx, by, w, h,
                             (Color){.36f, .39f, .43f, 1});
                }
            }
        }
    map_route(r, g, bx, by, w, h, ox, oy, scale);
    for (int i = 0; i < 25; i++) {
        const Pickup *p = &g->pickups[i];
        if (p->active)
            cliprect(r, ox + p->px * scale - 1, oy - p->pz * scale - 1, 3, 3, bx, by, w, h,
                     p->type == 1   ? (Color){.3f, 1, .4f, 1}
                     : p->type == 2 ? CYAN
                                    : YELLOW);
    }
    for (int i = 0; i < MAX_ACTORS; i++) {
        const Actor *a = &g->actors[i];
        if (a->active)
            cliprect(r, ox + a->v.x * scale - 1.5f, oy - a->v.z * scale - 1.5f, 3, 3, bx, by, w, h,
                     a->kind ? PINK : MUTED);
    }
    for (int i = 0; i < MAX_HELIS; i++)
        if (g->helis[i].active)
            cliprect(r, ox + g->helis[i].p.x * scale - 2, oy - g->helis[i].p.z * scale - 2, 5, 5,
                     bx, by, w, h, YELLOW);
    for (int i = 0; i < MAX_DROPS; i++)
        if (g->drops[i].active)
            cliprect(r, ox + g->drops[i].p.x * scale - 2, oy - g->drops[i].p.z * scale - 2, 5, 5,
                     bx, by, w, h, PAINTS[3]);
    if (g->sprint_node >= 0) {
        Vec3 p = world_center(&g->world, g->sprint_node);
        cliprect(r, ox + p.x * scale - 2, oy - p.z * scale - 2, 5, 5, bx, by, w, h, YELLOW);
    }
    float mx = bx + w / 2, my = by + h / 2;
    r_rect(r, mx - 2, my - 2, 5, 5, WHITE);
    r_line2(r, mx, my, mx + sinf(g->player.yaw) * 8, my - cosf(g->player.yaw) * 8, 2, CYAN);
    r_text(r, bx + 4, by + 5, 1, WHITE, "N");
    r_text(r, bx + 5, by + h - 10, 1, WHITE, "TOUCH FOR CITY MAP");
}
static void fullmap(Render *r, const Game *g) {
    header(r, g->world.name, g);
    float x0 = 12, y0 = 45, cell = 5;
    r_rect(r, x0, y0, 160, 160, district_color(OCEAN));
    for (int z = 0; z < 32; z++)
        for (int x = 0; x < 32; x++) {
            int node = z * 32 + x, kind = g->world.tiles[node].kind;
            Color c = district_color(kind ? kind : MARINA);
            if (g->visited[node]) {
                c.r *= 1.5f;
                c.g *= 1.5f;
                c.b *= 1.5f;
            }
            map_land(r, &g->world, x, z, x0, y0 + 160, cell / CELL, x0, y0, 160, 160, c);
        }
    map_route(r, g, x0, y0, 160, 160, x0, y0 + 160, cell / CELL);
    int n = world_node(g->player.x, g->player.z);
    if (n >= 0)
        r_rect(r, x0 + (n % 32) * cell, y0 + (31 - n / 32) * cell, cell, cell, WHITE);
    if (g->waypoint >= 0)
        r_rect(r, x0 + (g->waypoint % 32) * cell, y0 + (31 - g->waypoint / 32) * cell, cell, cell,
               YELLOW);
    if (g->sprint_node >= 0) {
        int q = g->sprint_node;
        r_rect(r, x0 + (q % 32) * cell, y0 + (31 - q / 32) * cell, cell, cell, YELLOW);
    }
    for (int i = 0; i < MAX_DROPS; i++)
        if (g->drops[i].active) {
            int q = world_node(g->drops[i].p.x, g->drops[i].p.z);
            if (q >= 0)
                r_rect(r, x0 + (q % 32) * cell, y0 + (31 - q / 32) * cell, cell, cell, PAINTS[3]);
        }
    r_text(r, 184, 48, 1, CYAN, "TOUCH LAND");
    r_text(r, 184, 60, 1, MUTED, "TO SET ROUTE");
    for (int i = 1; i <= 8; i++) {
        r_rect(r, 184, 76 + (i - 1) * 13, 5, 5, district_color(i));
        const char *labels[] = {"",      "DOWNTOWN", "OLD TOWN", "SUBURBS", "HARBOR",
                                "PARKS", "INDUSTRY", "BEACH",    "CIVIC"};
        r_text(r, 194, 75 + (i - 1) * 13, 1, MUTED, labels[i]);
    }
    char b[64];
    snprintf(b, sizeof b, "%d LAND TILES", g->world.land_count);
    r_text(r, 184, 186, 1, WHITE, b);
    snprintf(b, sizeof b, "SEED %08X", (unsigned)g->seed);
    r_text(r, 12, 206, 1, MUTED, b);
    footer(r, "A/B BACK / X CLEAR / Y BONUS ROUTE");
}
void render_bottom(Render *r, const Game *g) {
    render_reset(r, 320, 240);
    r_rect(r, 0, 0, 320, 240, BG);
    char b[100];
    if (g->screen == PLAY) {
        header(r, "PURSUIT", g);
        minimap(r, g);
        r_text(r, 174, 51, 1, MUTED, "HULL");
        snprintf(b, sizeof b, "%d%%", (int)fmaxf(0, g->player.hp));
        right(r, 308, 51, 1, WHITE, b);
        bar(r, 174, 64, 134, g->player.hp / 100, g->player.hp < 30 ? PINK : CYAN);
        r_text(r, 174, 78, 1, MUTED, "NITRO");
        bar(r, 174, 90, 134, g->player.nitro / 100, CYAN);
        snprintf(b, sizeof b, "%d M / %d S", (int)g->distance, (int)g->run_time);
        r_text(r, 174, 105, 1, WHITE, b);
        if (g->sprint_node >= 0) {
            r_text(r, 174, 124, 1, YELLOW, "CITY SPRINT +150");
            snprintf(b, sizeof b, "%dS / GOLD TILE", (int)ceilf(g->sprint_time));
            r_text(r, 174, 140, 1, WHITE, b);
            r_text(r, 174, 156, 1, CYAN, "MAP: Y SET ROUTE");
            snprintf(b, sizeof b, "LANDMARKS %d/%d", g->landmark_count, LANDMARK_COUNT);
            r_text(r, 174, 178, 1, MUTED, b);
        } else {
            r_text(r, 174, 124, 1, MUTED, "CONTRACTS");
            const char *labels[] = {"DRIVE 1500 M", "DRIFT 120 M", "SURVIVE 3 MIN",
                                    "VISIT 12 TILES"};
            for (int i = 0; i < 4; i++) {
                snprintf(b, sizeof b, "%s %s", g->mission_bits & (1 << i) ? "+" : "-", labels[i]);
                r_text(r, 174, 138 + i * 15, 1, g->mission_bits & (1 << i) ? CYAN : WHITE, b);
            }
        }
        r_text(r, 12, 201, 1, MUTED, "A GAS / B BRAKE / R DRIFT / L BOOST");
        footer(r, "START PAUSE / SELECT SETTINGS");
        return;
    }
    if (g->screen == CITYMAP) {
        fullmap(r, g);
        return;
    }
    if (g->screen == GARAGE) {
        header(r, "YOUR GARAGE", g);
        r_text(r, 12, 42, 1, MUTED, "CHOOSE WHAT TO CUSTOMIZE");
        for (int i = 0; i < SHOP_TABS; i++) {
            float x = 12 + (i % 3) * 100, y = 52 + (i / 3) * 50;
            bool active = i == g->cursor;
            r_rect(r, x, y, 94, 42, active ? (Color){.05f, .17f, .22f, 1} : PANEL);
            r_rect(r, x, y, 94, 2, active ? CYAN : (Color){.1f, .2f, .25f, 1});
            r_text(r, x + 6, y + 11, 1, active ? WHITE : MUTED, SHOP_NAMES[i]);
            snprintf(b, sizeof b, "%d OPTIONS", shop_count(i));
            r_text(r, x + 6, y + 28, 1, active ? CYAN : MUTED, b);
        }
        footer(r, "D-PAD CHOOSE / A OPEN / B BACK");
        return;
    }
    if (g->screen == SHOP) {
        header(r, SHOP_NAMES[g->tab], g);
        #ifdef COAST_DESKTOP
        snprintf(b, sizeof b, "Q/E CATEGORY %d/%d / UP-DOWN ITEM", g->tab + 1, SHOP_TABS);
#else
        snprintf(b, sizeof b, "L/R CATEGORY %d/%d / UP-DOWN ITEM", g->tab + 1, SHOP_TABS);
#endif
        r_text(r, 12, 42, 1, CYAN, b);
        int first = (g->cursor / 4) * 4, n = shop_count(g->tab);
        for (int j = 0; j < 4; j++) {
            int i = first + j;
            if (i >= n)
                break;
            if (g->tab == SHOP_UPGRADES) {
                int level = g->profile.upgrades[g->profile.selected][i];
                if (level >= 5)
                    snprintf(b, sizeof b, "MAX");
                else
                    snprintf(b, sizeof b, "%d/5 $%d", level, shop_price(g, g->tab, i));
            } else if (shop_equipped(g, g->tab, i))
                snprintf(b, sizeof b, "FITTED");
            else if (shop_owned(g, g->tab, i))
                snprintf(b, sizeof b, "OWNED");
            else
                snprintf(b, sizeof b, "$%d", shop_price(g, g->tab, i));
            row(r, 52 + j * 28, shop_name(g->tab, i), b, i == g->cursor);
        }
        r_text(r, 12, 168, 1, g->notice_time > 0 ? YELLOW : MUTED,
               g->notice_time > 0 ? g->notice : shop_description(g->tab, g->cursor));
        r_rect(r, 12, 187, 144, 22, PANEL);
        r_rect(r, 164, 187, 144, 22, PANEL);
        r_text(r, 23, 194, 1, CYAN, "< PREV PAGE");
        snprintf(b, sizeof b, "NEXT >  %d/%d", g->cursor + 1, n);
        r_text(r, 175, 194, 1, CYAN, b);
        footer(r, shop_owned(g, g->tab, g->cursor) ? "B CATEGORIES    A FIT / UPGRADE"
                                                   : "B CATEGORIES    A BUY / PREVIEW");
        if (g->confirm_purchase) {
            r_rect(r, 0, 0, 320, 240, (Color){0, 0, 0, .7f});
            r_rect(r, 20, 63, 280, 125, PANEL);
            r_text(r, 34, 79, 1.6f, WHITE, "CONFIRM PURCHASE");
            r_text(r, 34, 106, 1, CYAN, shop_name(g->tab, g->cursor));
            snprintf(b, sizeof b, "COST $%d / BANK $%u", shop_price(g, g->tab, g->cursor),
                     (unsigned)g->profile.money);
            r_text(r, 34, 125, 1, YELLOW, b);
            r_rect(r, 32, 146, 112, 28, (Color){.10f, .12f, .16f, 1});
            r_rect(r, 159, 146, 128, 28, (Color){.035f, .23f, .26f, 1});
            r_text(r, 43, 156, 1, WHITE, "B CANCEL");
            r_text(r, 174, 156, 1, WHITE, "A BUY + FIT");
        }
        return;
    }
    if (g->screen == SETTINGS) {
        header(r, "GRAPHICS", g);
        int first = (g->settings_cursor / 5) * 5;
        for (int j = 0; j < 5; j++) {
            int i = first + j;
            if (i >= SETTING_COUNT)
                break;
            setting_value(&g->profile.settings, i, b, sizeof b);
            row(r, 46 + j * 30, setting_name(i), b, i == g->settings_cursor);
        }
        #ifdef COAST_DESKTOP
        snprintf(b, sizeof b, "PAGE %d/4 / Q-E TO CHANGE PAGE", g->settings_cursor / 5 + 1);
#else
        snprintf(b, sizeof b, "PAGE %d/4 / L-R SHOULDER TO PAGE", g->settings_cursor / 5 + 1);
#endif
        r_text(r, 13, 202, 1, CYAN, b);
        footer(r, "LEFT/RIGHT CHANGE / B BACK");
        return;
    }
    if (g->screen == HELP) {
        header(r, "QUICK CONTROLS", g);
        const char *lines[] = {
            "CIRCLE PAD OR D-PAD: STEER",          "A: GAS / B: BRAKE THEN REVERSE",
            "R: DRIFT / L+A: SMALL NITRO BOOST",   "X: CAMERA / HOLD Y: ROAD RECOVERY",
            "START: PAUSE / SELECT: SETTINGS",     "TOUCH RADAR: MAP AND ROUTE PLANNER",
            "TIME + DAMAGE BRING MORE POLICE.",    "GREEN AIRDROPS GIVE FUN POWER-UPS.",
            "COP IMPACTS: LIGHT SPEED-BASED DAMAGE.", "SLOW NEAR COPS? 5S ARREST COUNTDOWN.",
            "DRIFT SCORE PAYS CASH WHEN RUN ENDS."};
        for (int i = 0; i < 11; i++)
            r_text(r, 12, 48 + i * 14, 1, i < 6 ? WHITE : MUTED, lines[i]);
        footer(r, "A/B BACK");
        return;
    }
    header(r,
           g->screen == TITLE   ? "AFTER HOURS"
           : g->screen == PAUSE ? "PAUSED"
                                : "ONE MORE RUN?",
           g);
    static const char *title[] = {"START A NEW CITY", "YOUR GARAGE", "SETTINGS", "HOW TO PLAY",
                                  "QUIT TO HOMEBREW"};
    static const char *pause[] = {"RESUME DRIVING", "CITY MAP / SET ROUTE", "SETTINGS", "CONTROLS",
                                  "END RUN + BANK CASH"};
    static const char *result[] = {"NEW CITY / DRIVE AGAIN", "YOUR GARAGE", "MAIN MENU",
                                   "QUIT TO HOMEBREW"};
    int n = g->screen == RESULT ? 4 : 5;
    for (int i = 0; i < n; i++)
        row(r, 55 + i * 28,
            g->screen == TITLE   ? title[i]
            : g->screen == PAUSE ? pause[i]
                                 : result[i],
            i == g->cursor ? ">" : NULL, i == g->cursor);
    if (g->screen == TITLE) {
        snprintf(b, sizeof b, "BEST %u M / %s", (unsigned)g->profile.best,
                 g->new3ds ? "NEW 3DS" : "OLD 3DS / 2DS");
        r_text(r, 12, 202, 1, MUTED, b);
    }
    footer(r, "D-PAD / CIRCLE PAD / TOUCH / A SELECT");
}

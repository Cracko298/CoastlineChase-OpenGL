#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
const CarSpec CARS[CAR_COUNT] = {{"TIDELINE", 0, 19, 7.5f, 7.5f, 110, 2.15f, 4.25f, 1.15f},
                                 {"POCKET ROCKET", 350, 18, 8.0f, 8.8f, 90, 1.8f, 3.3f, 1.4f},
                                 {"SUNSET GT", 850, 22, 7.8f, 7.7f, 110, 2.2f, 4.6f, 1.05f},
                                 {"DRIFTWAVE", 1300, 21, 8, 6.7f, 115, 2.2f, 4.5f, 1.15f},
                                 {"BEACH VAN", 1800, 16, 6.0f, 7, 175, 2.6f, 5.5f, 2.2f},
                                 {"INTERCEPTOR", 2600, 23, 8.6f, 8, 140, 2.3f, 4.9f, 1.4f},
                                 {"NEON EXOTIC", 4100, 26, 9, 7.8f, 100, 2.45f, 4.8f, .9f},
                                 {"COAST CRUSHER", 5200, 18, 6.8f, 7, 220, 2.9f, 5.3f, 2.1f},
                                 {"SEA BREEZE", 650, 20, 7.5f, 8, 95, 2.0f, 3.9f, 1},
                                 {"DUNE BUGGY", 1000, 18, 8.3f, 8.4f, 95, 2.1f, 3.5f, 1.5f},
                                 {"NIGHT CAB", 750, 19, 7, 8, 130, 2.3f, 4.9f, 1.4f},
                                 {"SURF WAGON", 1250, 19, 7.1f, 7.5f, 145, 2.2f, 5.1f, 1.5f},
                                 {"RAT ROD", 2350, 23, 8.8f, 6.8f, 110, 2.0f, 4.5f, 1.2f},
                                 {"HARBOR PICKUP", 1700, 18, 7.2f, 7.8f, 180, 2.5f, 5.5f, 1.8f},
                                 {"SOFT SERVE", 2150, 15, 5.7f, 7, 185, 2.65f, 5.7f, 2.7f},
                                 {"BUBBLE BUG", 500, 17, 7.3f, 9, 85, 1.75f, 3.0f, 1.55f},
                                 {"RALLY FOX", 3000, 22, 8.5f, 9.2f, 130, 2.05f, 4.0f, 1.45f},
                                 {"MIDNIGHT LIMO", 3500, 20, 6.8f, 6.8f, 175, 2.3f, 7.1f, 1.4f},
                                 {"CITY SHUTTLE", 3900, 15, 5.2f, 7.2f, 245, 2.85f, 8.0f, 2.8f},
                                 {"GULLWING", 4500, 24, 8.3f, 8.2f, 125, 2.3f, 4.45f, 1.0f},
                                 {"MIDNIGHT EXPRESS", 6500, 17, 4.8f, 6.5f, 300, 3.0f, 9.0f, 3.6f},
                                 {"BIG RIG", 5600, 18, 5.0f, 6.5f, 280, 2.9f, 11.0f, 3.4f},
                                 {"DOUBLE DECKER", 4800, 15, 4.6f, 7.0f, 270, 2.9f, 8.8f, 4.7f},
                                 {"NEON RIDER", 2200, 23, 9.0f, 9.5f, 75, .85f, 2.6f, 1.8f},
                                 {"SIDEWALK SURFER", 300, 12, 5.5f, 8.5f, 65, .65f, 1.8f, 1.9f},
                                 {"FORKLIFT FRENZY", 1600, 13, 6.2f, 9, 190, 2.0f, 3.8f, 2.6f},
                                 {"GOLF GETAWAY", 800, 14, 6.5f, 9, 100, 1.6f, 2.9f, 2.0f},
                                 {"TURBO TUK TUK", 1450, 17, 7.8f, 8.5f, 100, 1.55f, 3.0f, 2.0f},
                                 {"HOT DOG HOT ROD", 2750, 18, 6.8f, 7.5f, 165, 2.4f, 5.6f, 2.5f},
                                 {"RUBBER DUCK", 1900, 16, 7.2f, 8.5f, 125, 2.0f, 3.6f, 1.9f},
                                 {"UFO COMMUTER", 7000, 22, 8.2f, 8.8f, 135, 3.2f, 3.8f, 1.8f},
                                 {"MONOWHEEL", 3300, 21, 8.8f, 9, 90, 1.1f, 2.2f, 2.0f}};
const char *SHOP_NAMES[SHOP_TABS] = {"CARS",    "TUNING",   "PAINT",     "DECALS",     "WHEELS",
                                     "ENGINES", "ANTENNAS", "ROOF GEAR", "CITY THEMES"};
const char *DECAL_NAMES[DECAL_COUNT] = {"CLEAN BODY",   "RACING STRIPE", "TWIN STRIPES",
                                        "FLAME RUNNER", "LIGHTNING",     "NUMBER 08",
                                        "CHECKER FLAG", "OCEAN WAVES"};
const PartSpec ENGINES[ENGINE_COUNT] = {
    {"FACTORY", "BALANCED STOCK ENGINE", 0, 0, 0, 0},
    {"ECO TOURING", "EXTRA NITRO RECHARGE", 220, -.5f, .3f, 0},
    {"TORQUE V6", "STRONGER LOW SPEED PULL", 450, .4f, 1.5f, 0},
    {"TURBO FOUR", "MODEST TOP SPEED GAIN", 700, 1.8f, 1, 0},
    {"ELECTRIC SWAP", "QUICK SMOOTH ACCELERATION", 1100, .8f, 2, 0},
    {"BLOWN V8", "HOOD BLOWER / HEAVY PULL", 1550, 2.3f, 1.8f, -.2f}};
const PartSpec WHEELS[WHEEL_COUNT] = {{"FACTORY", "BALANCED ROAD TIRES", 0, 0, 0, 0},
                                      {"FIVE SPOKE", "LIGHT ALLOYS / +GRIP", 180, 0, .1f, .2f},
                                      {"WHITEWALL", "CLASSIC CRUISER STYLE", 250, 0, 0, 0},
                                      {"ALL TERRAIN", "WIDE TREAD / +GRIP", 380, -.2f, 0, .5f},
                                      {"DEEP DISH", "LOW PROFILE / +GRIP", 480, 0, 0, .35f},
                                      {"AERO DISC", "SMOOTH SOLID WHEEL FACE", 560, .2f, 0, .1f},
                                      {"RALLY MULTISPOKE", "MAXIMUM ROAD GRIP", 750, 0, 0, .8f}};
const Color PAINTS[PAINT_COUNT] = {{.1f, 1, 1, 1},    {1, .2f, .65f, 1},  {1, .65f, .15f, 1},
                                   {.65f, 1, .2f, 1}, {.55f, .35f, 1, 1}, {1, .95f, .8f, 1},
                                   {1, .2f, .18f, 1}, {.25f, .55f, 1, 1}};
const char *PAINT_NAMES[PAINT_COUNT] = {"ARCTIC CYAN", "HOT PINK",     "TANGERINE",
                                        "LIME FIZZ",   "ULTRAVIOLET",  "PEARL WHITE",
                                        "CHERRY RED",  "ELECTRIC BLUE"};
const char *ANTENNA_NAMES[ANTENNA_COUNT] = {"CLEAN ROOF",  "CLASSIC WHIP",   "TWIN WHIPS",
                                            "STAR SIGNAL", "SATELLITE DISH", "HEART SIGNAL",
                                            "SURF FLAG",   "ORBIT RINGS"};
const char *HAT_NAMES[HAT_COUNT] = {"NO HAT",     "TAXI SIGN", "TOP HAT", "SURFBOARD",
                                    "PARTY CONE", "CROWN",     "UFO",     "MILK CARTON"};
const Theme THEMES[THEME_COUNT] = {{"MIAMI NIGHTS",
                                    {.22f, .9f, 1, 1},
                                    {1, .24f, .64f, 1},
                                    {.022f, .012f, .065f, 1},
                                    {.015f, .075f, .13f, 1}},
                                   {"CHALK COAST",
                                    {.88f, .94f, 1, 1},
                                    {1, .83f, .58f, 1},
                                    {.012f, .015f, .025f, 1},
                                    {.025f, .04f, .06f, 1}},
                                   {"VAPOR SUNSET",
                                    {1, .4f, .68f, 1},
                                    {.4f, .88f, 1, 1},
                                    {.095f, .025f, .105f, 1},
                                    {.06f, .035f, .14f, 1}},
                                   {"JADE HARBOR",
                                    {.2f, 1, .7f, 1},
                                    {1, .8f, .2f, 1},
                                    {.008f, .04f, .05f, 1},
                                    {.01f, .075f, .07f, 1}},
                                   {"AMBER DISTRICT",
                                    {1, .7f, .22f, 1},
                                    {1, .35f, .15f, 1},
                                    {.05f, .02f, .01f, 1},
                                    {.055f, .04f, .015f, 1}}};
float clampf(float x, float a, float b) {
    return x < a ? a : x > b ? b : x;
}
float angle_delta(float a, float b) {
    float d = fmodf(a - b + PI, 2 * PI);
    if (d < 0)
        d += 2 * PI;
    return d - PI;
}
float vehicle_speed(const Vehicle *v) {
    return sqrtf(v->vx * v->vx + v->vz * v->vz);
}
uint32_t hash_cell(int x, int z, uint32_t seed) {
    uint32_t h = (uint32_t)x * 0x8da6b343u ^ (uint32_t)z * 0xd8163841u ^ seed;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    return h ^ (h >> 16);
}
uint32_t random_u32(Game *g) {
    uint32_t x = g->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng = x ? x : 1234567;
    return g->rng;
}
void settings_preset(Settings *s, int p) {
    s->preset = p;
    s->view = p == 0 ? 2 : p == 1 ? 3 : 4;
    s->bloom = p == 0 ? 0 : p == 1 ? 1 : 2;
    s->outline = 1;
    s->detail = p == 0 ? 0 : p == 1 ? 1 : 2;
    s->shadows = p != 0;
    s->particles = p != 0;
    s->shake = 1;
    s->fov = 1;
    s->fps = p == 2 ? 60 : 30;
    s->fog = 1;
    s->white = 0;
    s->lighting = p != 0;
    s->weather = 1;
    s->auto_lod = 1;
}
void profile_defaults(CoastProfile *p, bool n) {
    memset(p, 0, sizeof *p);
    p->cars = p->paints = p->antennas = p->hats = p->themes = p->decals = p->wheels = p->engines =
        1;
    settings_preset(&p->settings, n ? 2 : 1);
    p->settings.traffic = 1;
    p->settings.camera = 1;
    p->settings.sfx = p->settings.music = p->settings.hints = 1;
    p->settings.volume = 6;
    p->settings.steering = 1;
}
int shop_count(int t) {
    static const int n[] = {CAR_COUNT,     4,           PAINT_COUNT,
                            DECAL_COUNT,   WHEEL_COUNT, ENGINE_COUNT,
                            ANTENNA_COUNT, HAT_COUNT,   THEME_COUNT};
    return t >= 0 && t < SHOP_TABS ? n[t] : 0;
}
const char *shop_name(int t, int i) {
    static const char *u[] = {"ENGINE TUNE", "TIRE COMPOUND", "ARMOR", "NITRO TANK"};
    if (i < 0 || i >= shop_count(t))
        return "";
    switch (t) {
    case SHOP_CARS:
        return CARS[i].name;
    case SHOP_UPGRADES:
        return u[i];
    case SHOP_PAINT:
        return PAINT_NAMES[i];
    case SHOP_DECALS:
        return DECAL_NAMES[i];
    case SHOP_WHEELS:
        return WHEELS[i].name;
    case SHOP_ENGINES:
        return ENGINES[i].name;
    case SHOP_ANTENNAS:
        return ANTENNA_NAMES[i];
    case SHOP_HATS:
        return HAT_NAMES[i];
    default:
        return THEMES[i].name;
    }
}
const char *shop_description(int t, int i) {
    static const char *up[] = {"SMALL SPEED AND POWER GAINS", "BETTER CORNERING GRIP",
                               "REDUCES COLLISION DAMAGE", "LONGER BOOST DURATION"};
    if (t == SHOP_UPGRADES)
        return up[i];
    if (t == SHOP_ENGINES)
        return ENGINES[i].description;
    if (t == SHOP_WHEELS)
        return WHEELS[i].description;
    if (t == SHOP_CARS)
        return "OWN EACH CAR / BUILD ITS LOADOUT";
    if (t == SHOP_THEMES)
        return "PREVIEW THE WHOLE CITY PALETTE";
    return "SAVED SEPARATELY FOR EACH CAR";
}
int shop_price(const Game *g, int t, int i) {
    if (i < 0 || i >= shop_count(t))
        return 0;
    if (t == SHOP_CARS)
        return CARS[i].price;
    if (t == SHOP_UPGRADES) {
        int l = g->profile.upgrades[g->profile.selected][i];
        return l >= 5 ? 0 : 120 + l * l * 100;
    }
    if (t == SHOP_ENGINES)
        return ENGINES[i].price;
    if (t == SHOP_WHEELS)
        return WHEELS[i].price;
    return i == 0 ? 0 : t == SHOP_THEMES ? 350 + i * 180 : 60 + i * 60;
}
static uint32_t owned_mask(const CoastProfile *p, int t) {
    switch (t) {
    case SHOP_CARS:
        return p->cars;
    case SHOP_PAINT:
        return p->paints;
    case SHOP_DECALS:
        return p->decals;
    case SHOP_WHEELS:
        return p->wheels;
    case SHOP_ENGINES:
        return p->engines;
    case SHOP_ANTENNAS:
        return p->antennas;
    case SHOP_HATS:
        return p->hats;
    case SHOP_THEMES:
        return p->themes;
    default:
        return 0;
    }
}
bool shop_owned(const Game *g, int t, int i) {
    if (i < 0 || i >= shop_count(t))
        return false;
    if (t == SHOP_UPGRADES)
        return g->profile.upgrades[g->profile.selected][i] >= 5;
    return (owned_mask(&g->profile, t) & (1u << i)) != 0;
}
bool shop_equipped(const Game *g, int t, int i) {
    const CoastProfile *p = &g->profile;
    const CarStyle *s = &p->styles[p->selected];
    switch (t) {
    case SHOP_CARS:
        return p->selected == i;
    case SHOP_PAINT:
        return s->paint == i;
    case SHOP_DECALS:
        return s->decal == i;
    case SHOP_WHEELS:
        return s->wheel == i;
    case SHOP_ENGINES:
        return s->engine == i;
    case SHOP_ANTENNAS:
        return s->antenna == i;
    case SHOP_HATS:
        return s->hat == i;
    case SHOP_THEMES:
        return p->theme == i;
    default:
        return false;
    }
}
void game_purchase(Game *g) {
    CoastProfile *p = &g->profile;
    int t = g->tab, i = g->cursor;
    g->confirm_purchase = false;
    if (i < 0 || i >= shop_count(t))
        return;
    bool own = shop_owned(g, t, i);
    if (t == SHOP_UPGRADES && own) {
        game_notice(g, "ALREADY FULLY UPGRADED");
        return;
    }
    int cost = own ? 0 : shop_price(g, t, i);
    if (p->money < (uint32_t)cost) {
        game_notice(g, "NOT ENOUGH CASH - KEEP DRIVING");
        return;
    }
    p->money -= cost;
    CarStyle *s = &p->styles[p->selected];
    switch (t) {
    case SHOP_CARS:
        p->cars |= 1u << i;
        p->selected = i;
        break;
    case SHOP_UPGRADES:
        p->upgrades[p->selected][i]++;
        break;
    case SHOP_PAINT:
        p->paints |= 1u << i;
        s->paint = i;
        break;
    case SHOP_DECALS:
        p->decals |= 1u << i;
        s->decal = i;
        break;
    case SHOP_WHEELS:
        p->wheels |= 1u << i;
        s->wheel = i;
        break;
    case SHOP_ENGINES:
        p->engines |= 1u << i;
        s->engine = i;
        break;
    case SHOP_ANTENNAS:
        p->antennas |= 1u << i;
        s->antenna = i;
        break;
    case SHOP_HATS:
        p->hats |= 1u << i;
        s->hat = i;
        break;
    case SHOP_THEMES:
        p->themes |= 1u << i;
        p->theme = i;
        break;
    }
    g->dirty = true;
    g->audio_events |= 2;
    game_notice(g, cost ? "PURCHASED AND FITTED" : "FITTED TO YOUR CAR");
}
CarSpec car_tuned(const CoastProfile *p, int car, int engine_override, int wheel_override) {
    CarSpec c = CARS[car];
    const CarStyle *s = &p->styles[car];
    int e = engine_override < 0 ? s->engine : engine_override,
        w = wheel_override < 0 ? s->wheel : wheel_override;
    c.speed += p->upgrades[car][0] * .45f + ENGINES[e].speed + WHEELS[w].speed;
    c.accel += p->upgrades[car][0] * .4f + ENGINES[e].accel + WHEELS[w].accel;
    c.grip += p->upgrades[car][1] * .35f + ENGINES[e].grip + WHEELS[w].grip;
    c.armor += p->upgrades[car][2] * 20;
    return c;
}
int preview_car(const Game *g) {
    return g->screen == SHOP && g->tab == SHOP_CARS ? g->cursor : g->profile.selected;
}
CarStyle preview_style(const Game *g) {
    CarStyle s = g->profile.styles[preview_car(g)];
    if (g->screen == SHOP)
        switch (g->tab) {
        case SHOP_PAINT:
            s.paint = g->cursor;
            break;
        case SHOP_DECALS:
            s.decal = g->cursor;
            break;
        case SHOP_WHEELS:
            s.wheel = g->cursor;
            break;
        case SHOP_ENGINES:
            s.engine = g->cursor;
            break;
        case SHOP_ANTENNAS:
            s.antenna = g->cursor;
            break;
        case SHOP_HATS:
            s.hat = g->cursor;
            break;
        default:
            break;
        }
    return s;
}
int preview_theme(const Game *g) {
    return g->screen == SHOP && g->tab == SHOP_THEMES ? g->cursor : g->profile.theme;
}
static int *visual_setting(Settings *s, int i) {
    int *fields[] = {&s->preset,  &s->view,      &s->bloom,  &s->outline, &s->detail,
                     &s->shadows, &s->particles, &s->shake,  &s->fov,     &s->fps,
                     &s->fog,     &s->white,     &s->camera, &s->showfps, &s->lighting,
                     &s->weather, &s->auto_lod};
    return i >= 0 && i < SETTING_COUNT ? fields[i] : NULL;
}
const char *setting_name(int i) {
    static const char *names[] = {
        "GRAPHICS PRESET", "VIEW DISTANCE",  "BLOOM GLOW",    "OUTLINE WIDTH", "SCENERY DETAIL",
        "CAR SHADOWS",     "PARTICLES",      "SCREEN SHAKE",  "SPEED FOV",     "FRAME LIMIT",
        "DISTANCE FOG",    "WHITE OUTLINES", "CAMERA HEIGHT", "FPS DISPLAY",   "LIGHT POOLS",
        "WEATHER EFFECTS", "ADAPTIVE DETAIL"};
    return i >= 0 && i < SETTING_COUNT ? names[i] : "";
}
void setting_value(const Settings *s, int i, char *o, int n) {
    static const char *quality[] = {"OFF", "LOW", "MEDIUM", "HIGH"};
    if (i == 0) {
        #ifdef COAST_DESKTOP
        const char *q[] = {"LOW", "BALANCED", "HIGH / PC", "CUSTOM"};
#else
        const char *q[] = {"LOW / OLD 3DS", "BALANCED", "HIGH / NEW 3DS", "CUSTOM"};
#endif
        snprintf(o, n, "%s", q[s->preset]);
        return;
    }
    if (i == 1) {
        snprintf(o, n, "%d BLOCKS", s->view);
        return;
    }
    if (i == 2) {
        snprintf(o, n, "%s", quality[s->bloom]);
        return;
    }
    if (i == 3) {
        snprintf(o, n, "%d PX", s->outline);
        return;
    }
    if (i == 4) {
        snprintf(o, n, "%s", quality[s->detail + 1]);
        return;
    }
    if (i == 9) {
        snprintf(o, n, "%d FPS", s->fps);
        return;
    }
    if (i == 12) {
        snprintf(o, n, "%s", s->camera == 0 ? "LOW" : s->camera == 1 ? "CHASE" : "HIGH");
        return;
    }
    Settings copy = *s;
    int *v = visual_setting(&copy, i);
    snprintf(o, n, "%s", v && *v ? "ON" : "OFF");
}
void setting_change(Game *g, int i, int d) {
    Settings *s = &g->profile.settings;
    if (i == 0) {
        settings_preset(s, ((s->preset == 3 ? 1 : s->preset) + d + 3) % 3);
        g->dirty = true;
        return;
    }
    int *v = visual_setting(s, i);
    if (!v)
        return;
    if (i == 9) {
        *v = *v == 30 ? 60 : 30;
    } else {
        int lo = i == 1   ? 2
                 : i == 3 ? 1
                          : 0,
            hi = i == 1              ? 5
                 : i == 2 || i == 3  ? 3
                 : i == 4 || i == 12 ? 2
                                     : 1;
        *v += d;
        if (*v > hi)
            *v = lo;
        if (*v < lo)
            *v = hi;
    }
    s->preset = 3;
    g->dirty = true;
}

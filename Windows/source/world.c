#include "game.h"
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
const int ROAD_DX[4] = {0, 1, 0, -1}, ROAD_DZ[4] = {1, 0, -1, 0};
static const Tile WATER = {0};
const Tile *world_tile(const World *w, int x, int z) {
    return x < 0 || z < 0 || x >= MAP_SIDE || z >= MAP_SIDE ? &WATER : &w->tiles[z * MAP_SIDE + x];
}
int world_node(float x, float z) {
    int cx = (int)floorf(x / CELL), cz = (int)floorf(z / CELL);
    return cx < 0 || cz < 0 || cx >= MAP_SIDE || cz >= MAP_SIDE ? -1 : cz * MAP_SIDE + cx;
}
Vec3 world_center(const World *w, int n) {
    if (n < 0 || n >= MAP_TILES)
        n = w->spawn;
    return (Vec3){(n % MAP_SIDE + .5f) * CELL + w->tiles[n].ox, 0,
                  (n / MAP_SIDE + .5f) * CELL + w->tiles[n].oz};
}
int world_neighbor(const World *w, int n, int dir) {
    if (n < 0 || n >= MAP_TILES || dir < 0 || dir > 3 || !(w->tiles[n].roads & (1 << dir)))
        return -1;
    int x = n % MAP_SIDE + ROAD_DX[dir], z = n / MAP_SIDE + ROAD_DZ[dir];
    return x < 0 || z < 0 || x >= MAP_SIDE || z >= MAP_SIDE ? -1 : z * MAP_SIDE + x;
}
static bool permits(int kind, int dir) {
    return kind != OCEAN && (kind != BRIDGE_EW || (dir & 1)) && (kind != BRIDGE_NS || !(dir & 1));
}
static void connect(World *w) {
    for (int z = 0; z < MAP_SIDE; z++)
        for (int x = 0; x < MAP_SIDE; x++) {
            Tile *t = &w->tiles[z * MAP_SIDE + x];
            t->roads = 0;
            for (int d = 0; d < 4; d++)
                if (permits(t->kind, d) &&
                    permits(world_tile(w, x + ROAD_DX[d], z + ROAD_DZ[d])->kind, (d + 2) % 4))
                    t->roads |= 1 << d;
        }
}
void world_distances(const World *w, int target, int16_t field[MAP_TILES]) {
    for (int i = 0; i < MAP_TILES; i++)
        field[i] = -1;
    if (target < 0 || target >= MAP_TILES || !w->tiles[target].kind)
        return;
    uint16_t queue[MAP_TILES];
    int head = 0, tail = 0;
    queue[tail++] = target;
    field[target] = 0;
    while (head < tail) {
        int n = queue[head++];
        for (int d = 0; d < 4; d++) {
            int next = world_neighbor(w, n, d);
            if (next >= 0 && field[next] < 0) {
                field[next] = field[n] + 1;
                queue[tail++] = next;
            }
        }
    }
}
int world_nearest(const World *w, float x, float z) {
    int n = world_node(x, z);
    if (n >= 0 && w->tiles[n].kind)
        return n;
    int best = w->spawn;
    float distance = 1e30f;
    for (int i = 0; i < MAP_TILES; i++)
        if (w->tiles[i].kind) {
            Vec3 p = world_center(w, i);
            float dx = p.x - x, dz = p.z - z, d = dx * dx + dz * dz;
            if (d < distance) {
                distance = d;
                best = i;
            }
        }
    return best;
}
static float ellipse(float x, float z, float cx, float cz, float rx, float rz) {
    float a = (x - cx) / rx, b = (z - cz) / rz;
    return a * a + b * b;
}
static uint32_t wrand(uint32_t *state) {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}
static void carve_roads(World *w) {
    uint8_t available[MAP_TILES], kept[MAP_TILES] = {0}, seen[MAP_TILES] = {0};
    uint16_t stack[MAP_TILES];
    for (int i = 0; i < MAP_TILES; i++)
        available[i] = w->tiles[i].roads;
    int top = 0;
    stack[top++] = w->spawn;
    seen[w->spawn] = 1;
    while (top) {
        int n = stack[top - 1], start = hash_cell(n, 3, w->seed) % 4, next = -1, dir = 0;
        for (int k = 0; k < 4; k++) {
            int d = (start + k) % 4, q = world_neighbor(w, n, d);
            if (q >= 0 && !seen[q]) {
                next = q;
                dir = d;
                break;
            }
        }
        if (next < 0) {
            top--;
            continue;
        }
        kept[n] |= 1 << dir;
        kept[next] |= 1 << ((dir + 2) % 4);
        seen[next] = 1;
        stack[top++] = next;
    }
    for (int n = 0; n < MAP_TILES; n++)
        for (int d = 0; d < 2; d++) {
            int q = world_neighbor(w, n, d);
            if (q < 0)
                continue;
            if (w->tiles[n].kind >= BRIDGE_EW || w->tiles[q].kind >= BRIDGE_EW ||
                hash_cell(n, d, w->seed + 551) % 100 < 48) {
                kept[n] |= 1 << d;
                kept[q] |= 1 << ((d + 2) % 4);
            }
        }
    for (int n = 0; n < MAP_TILES; n++)
        w->tiles[n].roads = kept[n] & available[n];
}

static void make_coast(World *w) {
    for (int gz = 0; gz < SHORE_SIDE; gz++)
        for (int gx = 0; gx < SHORE_SIDE; gx++) {
            float x = gx * SHORE_STEP, z = gz * SHORE_STEP, best = -128;
            int tx = (int)(x / CELL), tz = (int)(z / CELL);
            for (int j = tz - 1; j <= tz + 1; j++)
                for (int i = tx - 1; i <= tx + 1; i++) {
                    int kind = world_tile(w, i, j)->kind;
                    if (!kind || kind >= BRIDGE_EW)
                        continue;
                    Vec3 c = world_center(w, j * MAP_SIDE + i);
                    float radius = 46 + hash_cell(i, j, w->seed + 817) % 5;
                    best = fmaxf(best, radius - hypotf(x - c.x, z - c.z));
                }
            w->shore[gz * SHORE_SIDE + gx] = (int16_t)roundf(clampf(best, -128, 127) * 128);
        }
    for (int tz = 0; tz < MAP_SIDE; tz++)
        for (int tx = 0; tx < MAP_SIDE; tx++) {
            bool land = false, water = false;
            for (int z = 0; z <= 4; z++)
                for (int x = 0; x <= 4; x++) {
                    if (w->shore[(tz * 4 + z) * SHORE_SIDE + tx * 4 + x] >= 0)
                        land = true;
                    else
                        water = true;
                }
            w->shore_kind[tz * MAP_SIDE + tx] = land ? (water ? 1 : 2) : 0;
        }
    static const char *places[] = {"SANTA FE",  "PALMA",      "MIRAGE",   "CORAL",    "LAGUNA",
                                   "HAVANA",    "SANTA CRUZ", "MONTEREY", "ARCADIA",  "SAPPHIRE",
                                   "SANTO SOL", "VENICE",     "PACIFICA", "NEON BAY", "MARBELLA",
                                   "SUNSET",    "AURORA",     "SAN LUNA", "RIO AZUL", "SILVER PALM",
                                   "PORTO",     "DAYTONA",    "SOLANA",   "KEY WEST"};
    static const char *prefix[] = {"ISLAND", "ISLA", "ISLE OF", "ISLAND"};
    uint32_t h = hash_cell(91, 317, w->seed);
    snprintf(w->name, sizeof w->name, "%s %s", prefix[(h >> 12) % 4], places[h % 24]);
}
float world_coast(const World *w, float x, float z) {
    float gx = x / SHORE_STEP, gz = z / SHORE_STEP;
    int ix = (int)floorf(gx), iz = (int)floorf(gz);
    if (ix < 0 || iz < 0 || ix >= SHORE_SIDE - 1 || iz >= SHORE_SIDE - 1)
        return -128;
    float u = gx - ix, v = gz - iz;
    const int16_t *p = &w->shore[iz * SHORE_SIDE + ix];
    float a = p[0] / 128.f, b = p[1] / 128.f, c = p[SHORE_SIDE] / 128.f,
          d = p[SHORE_SIDE + 1] / 128.f;
    return u >= v ? a + (b - a) * u + (d - b) * v : a + (d - c) * u + (c - a) * v;
}
static bool lot_clear(const World *w, int node, const Building *b) {
    if (world_coast(w, b->x, b->z) < 1 || world_coast(w, b->x + b->w, b->z) < 1 ||
        world_coast(w, b->x, b->z + b->d) < 1 || world_coast(w, b->x + b->w, b->z + b->d) < 1)
        return false;
    Vec3 a = world_center(w, node);
    for (int d = 0; d < 4; d++) {
        int n = world_neighbor(w, node, d);
        if (n < 0)
            continue;
        Vec3 end = world_center(w, n);

        for (int k = 0; k <= 32; k++) {
            float t = k / 32.f, x = a.x + (end.x - a.x) * t, z = a.z + (end.z - a.z) * t;
            float dx = x - clampf(x, b->x, b->x + b->w), dz = z - clampf(z, b->z, b->z + b->d);
            if (dx * dx + dz * dz < 11.5f * 11.5f)
                return false;
        }
    }
    return true;
}
static void world_generate(World *w, uint32_t seed) {
    memset(w, 0, sizeof *w);
    w->seed = seed;
    w->cx = w->cz = INT_MIN;
    for (int i = 0; i < MAP_TILES; i++)
        w->chunks[i].x = INT_MIN;
    uint32_t rng = hash_cell(71, 99, seed) | 1u;
    w->layout = wrand(&rng) % 4;
    float theta = (wrand(&rng) % 628) / 100.f, ct = cosf(theta), st = sinf(theta);
    float rx = 8 + wrand(&rng) % 5, rz = 7 + wrand(&rng) % 5;
    float bx[8], bz[8], br[8];
    int blobs = 4 + wrand(&rng) % 5;
    for (int i = 0; i < blobs; i++) {
        float a = theta + i * 2 * PI / blobs + ((int)(wrand(&rng) % 70) - 35) * .01f;
        float spread = 5 + wrand(&rng) % 6;
        bx[i] = 16 + cosf(a) * spread;
        bz[i] = 16 + sinf(a) * spread;
        br[i] = 3 + wrand(&rng) % 4;
    }
    for (int z = 1; z < 31; z++)
        for (int x = 1; x < 31; x++) {
            float xx = (x - 16) * ct + (z - 16) * st, zz = -(x - 16) * st + (z - 16) * ct;
            float shape = ellipse(xx, zz, 0, 0, rx, rz);
            for (int i = 0; i < blobs; i++)
                shape = fminf(shape, ellipse(x, z, bx[i], bz[i], br[i], br[i] * .85f));
            if (w->layout == 1 && ellipse(xx, zz, 8, 2, 7, 8) < 1)
                shape = 2;
            if (w->layout == 2 && ellipse(xx, zz, -3, 1, 3.5f, 4.5f) < 1)
                shape = 2;
            if (w->layout == 3 && fabsf(zz) > (7 + 3 * sinf(xx * .25f)))
                shape += .35f;
            float threshold = .91f + (hash_cell(x, z, seed) & 255) / 1250.f;
            if (shape > threshold)
                continue;
            uint32_t h = hash_cell(x / 3, z / 3, seed + 32);
            w->tiles[z * 32 + x].kind = 1 + h % 8;
        }

    bool horizontal = wrand(&rng) & 1;
    int canal = 12 + wrand(&rng) % 8, width = 1 + wrand(&rng) % 3;
    for (int z = 1; z < 31; z++)
        for (int x = 1; x < 31; x++) {
            int v = horizontal ? z : x;
            if (v >= canal && v < canal + width)
                w->tiles[z * 32 + x].kind = OCEAN;
        }
    int crossing[30], count = 0;
    for (int row = 2; row < 30; row++) {
        int a = horizontal ? (canal - 1) * 32 + row : row * 32 + canal - 1;
        int b = horizontal ? (canal + width) * 32 + row : row * 32 + canal + width;
        if (w->tiles[a].kind && w->tiles[b].kind)
            crossing[count++] = row;
    }
    int first = count ? wrand(&rng) % count : 0;
    for (int k = 0; k < count; k++) {
        int index = (first + k) % count;
        if (k > 0 && abs(crossing[index] - crossing[first]) < 5)
            continue;
        int row = crossing[index];
        for (int v = canal; v < canal + width; v++)
            w->tiles[horizontal ? v * 32 + row : row * 32 + v].kind =
                horizontal ? BRIDGE_NS : BRIDGE_EW;
        if (k > 0)
            break;
    }
    connect(w);
    w->spawn = -1;
    int best = 999999;
    for (int n = 0; n < MAP_TILES; n++)
        if (w->tiles[n].kind && w->tiles[n].kind < BRIDGE_EW) {
            int dx = n % 32 - 16, dz = n / 32 - 16, cost = dx * dx + dz * dz;
            if (cost < best && w->tiles[n].roads) {
                best = cost;
                w->spawn = n;
            }
        }
    if (w->spawn < 0) {
        w->spawn = 16 * 32 + 16;
        w->tiles[w->spawn].kind = OLD_TOWN;
    }
    int16_t field[MAP_TILES];
    world_distances(w, w->spawn, field);
    for (int n = 0; n < MAP_TILES; n++)
        if (field[n] < 0)
            w->tiles[n] = (Tile){0};
    connect(w);
    carve_roads(w);
    for (int z = 1; z < 31; z++)
        for (int x = 1; x < 31; x++) {
            Tile *t = &w->tiles[z * 32 + x];
            if (!t->kind)
                continue;
            w->land_count++;
            if (t->kind >= BRIDGE_EW) {
                w->bridge_count++;
                continue;
            }
            uint32_t h = hash_cell(x, z, seed + 71);
            t->ox = (int)(h % 11) - 5;
            t->oz = (int)((h >> 8) % 11) - 5;
            bool coast = false;
            for (int d = 0; d < 4; d++) {
                int k = world_tile(w, x + ROAD_DX[d], z + ROAD_DZ[d])->kind;
                if (!k)
                    coast = true;
                if (k == BRIDGE_EW)
                    t->oz = 0;
                if (k == BRIDGE_NS)
                    t->ox = 0;
            }
            if (coast && t->kind != INDUSTRY)
                t->kind = h % 3 ? MARINA : BEACH;
            if (h % 11 == 0)
                t->landmark = 1 + (h >> 12) % LANDMARK_COUNT;
        }

    int n = wrand(&rng) % MAP_TILES;
    for (int family = 1; family <= LANDMARK_COUNT; family++) {
        for (int tries = 0; tries < MAP_TILES; tries++) {
            n = (n + 37) % MAP_TILES;
            Tile *t = &w->tiles[n];
            if (t->kind && t->kind < BRIDGE_EW && !t->landmark) {
                t->landmark = family;
                break;
            }
        }
    }
    make_coast(w);
    for (int i = 0; i < MAP_TILES; i++)
        world_chunk(w, i % 32, i / 32);
}
void world_init(World *w, uint32_t seed) {

    for (int attempt = 0; attempt < 16; attempt++) {
        world_generate(w, attempt ? hash_cell(attempt, 91, seed) : seed);
        if (w->land_count >= 160 && w->bridge_count >= 2)
            return;
    }
}
const char *landmark_name(int type) {
    static const char *names[] = {"",
                                  "STARLIGHT DINER",
                                  "OCEAN ARCADE",
                                  "GRAND CINEMA",
                                  "CLOCK SQUARE",
                                  "LIGHTHOUSE POINT",
                                  "BOTANICAL GARDENS",
                                  "FERRIS BOARDWALK",
                                  "DOCKSIDE CRANE",
                                  "ART DECO HOTEL",
                                  "NIGHT MARKET",
                                  "FUEL AND REPAIR",
                                  "OBSERVATORY",
                                  "NEON MECHANICS",
                                  "PALM MOTEL",
                                  "VINYL RECORDS",
                                  "CORNER SUPERMARKET",
                                  "FIRE STATION",
                                  "CITY HOSPITAL",
                                  "VIDEO RENTAL",
                                  "COAST LAUNDROMAT"};
    return type >= 0 && type <= LANDMARK_COUNT ? names[type] : "";
}
const char *district_name(int k) {
    static const char *names[] = {"OPEN OCEAN",     "DOWNTOWN",      "OLD TOWN",
                                  "GARDEN SUBURBS", "HARBOR FRONT",  "GREEN PARK",
                                  "DOCK WORKS",     "SANDY COAST",   "CIVIC QUARTER",
                                  "CHANNEL BRIDGE", "CHANNEL BRIDGE"};
    return k >= 0 && k <= BRIDGE_NS ? names[k] : "CITY";
}
const Chunk *world_chunk(World *w, int x, int z) {
    static const Chunk empty = {0};
    if (x < 0 || z < 0 || x >= 32 || z >= 32)
        return &empty;
    Chunk *c = &w->chunks[z * 32 + x];
    if (c->x == x && c->z == z)
        return c;
    memset(c, 0, sizeof *c);
    c->x = x;
    c->z = z;
    c->hash = hash_cell(x, z, w->seed);
    w->generated++;
    const Tile *t = world_tile(w, x, z);
    c->kind = t->kind;
    c->roads = t->roads;
    c->landmark = t->landmark;
    c->park = t->kind == PARK;
    if (!t->kind || t->kind >= BRIDGE_EW)
        return c;
    for (int d = 0; d < 4; d++)
        if (!world_tile(w, x + ROAD_DX[d], z + ROAD_DZ[d])->kind ||
            world_tile(w, x + ROAD_DX[d], z + ROAD_DZ[d])->kind >= BRIDGE_EW)
            c->coast |= 1 << d;

    for (int q = 0; q < 8 && c->count < 8; q++) {
        if ((c->park || t->kind == BEACH) && c->count >= 1)
            break;
        uint32_t h = hash_cell(x * 8 + q, z, w->seed + 43);
        Building b = {0};
        if (q < 4) {
            b.x = x * CELL + (q & 1 ? 46 : 2);
            b.z = z * CELL + (q & 2 ? 46 : 2);
            b.w = 15 + h % 3;
            b.d = 15 + (h >> 5) % 3;
        } else {
            int d = q - 4;
            if (t->roads & (1 << d))
                continue;
            b.x = x * CELL + 24 + ROAD_DX[d] * 23;
            b.z = z * CELL + 24 + ROAD_DZ[d] * 23;
            b.w = 14;
            b.d = 14;
        }

        if (!lot_clear(w, z * 32 + x, &b)) {
            b.w = b.d = 10;
            if (q < 4) {
                b.x = x * CELL + (q & 1 ? 45 : 9);
                b.z = z * CELL + (q & 2 ? 45 : 9);
            }
            if (!lot_clear(w, z * 32 + x, &b))
                continue;
        }
        b.h = t->kind == DOWNTOWN   ? 24 + (h >> 8) % 68
              : t->kind == SUBURBS  ? 5 + (h >> 8) % 7
              : t->kind == INDUSTRY ? 9 + (h >> 8) % 9
              : t->kind == MARINA   ? 8 + (h >> 8) % 28
              : t->kind == CIVIC    ? 14 + (h >> 8) % 25
                                    : 7 + (h >> 8) % 15;
        if (c->park || t->kind == BEACH)
            b.h = 4;
        if (c->landmark && c->count == 0)
            b.h = c->landmark == 4    ? 28
                  : c->landmark == 5  ? 32
                  : c->landmark == 9  ? 40
                  : c->landmark == 12 ? 18
                  : c->landmark == 14 ? 12
                                      : 6;
        b.windows_x = (uint8_t)fmaxf(1, floorf(b.w / (2.8f + h % 3)));
        b.windows_z = (uint8_t)fmaxf(1, floorf(b.d / 3.4f));
        b.floors = (uint8_t)fmaxf(1, floorf(b.h / 3.8f));
        b.door = (h >> 15) % 4;
        b.roof = t->kind == SUBURBS ? 1 : (h >> 19) % 4;
        b.style = (h >> 21) % 4;
        b.palette = (h >> 24) % 4;
        b.business = c->landmark && c->count == 0 ? 12 : (h >> 11) % 12;
        if (c->landmark >= 13 && c->count == 0)
            b.door = c->landmark == 13 || c->landmark == 17 ? 2 : 1;
        c->b[c->count++] = b;
    }
    return c;
}
void world_stream(World *w, float x, float z, int radius) {
    int cx = (int)floorf(x / CELL), cz = (int)floorf(z / CELL);
    if (cx == w->cx && cz == w->cz && radius == w->radius)
        return;
    w->cx = cx;
    w->cz = cz;
    w->radius = radius;
    for (int j = -radius; j <= radius; j++)
        for (int i = -radius; i <= radius; i++)
            world_chunk(w, cx + i, cz + j);
}
bool world_surface(const World *w, float x, float z) {
    int n = world_node(x, z);
    if (n < 0)
        return false;
    if (w->shore_kind[n] == 2 || (w->shore_kind[n] == 1 && world_coast(w, x, z) >= 0))
        return true;
    int k = w->tiles[n].kind;
    Vec3 p = world_center(w, n);
    if (k == BRIDGE_EW)
        return fabsf(z - p.z) <= ROAD / 2;
    if (k == BRIDGE_NS)
        return fabsf(x - p.x) <= ROAD / 2;
    return false;
}
static bool terrain(const World *w, float x, float z, float r) {
    int n = world_node(x, z);
    if (n >= 0 && w->shore_kind[n] == 2) {
        float lx = x - (n % MAP_SIDE) * CELL, lz = z - (n / MAP_SIDE) * CELL;
        if (lx >= r && lx <= CELL - r && lz >= r && lz <= CELL - r)
            return true;
    }
    return world_surface(w, x, z) && world_surface(w, x - r, z) && world_surface(w, x + r, z) &&
           world_surface(w, x, z - r) && world_surface(w, x, z + r);
}
static bool overlap(const Building *b, float x, float z, float r) {
    float dx = x - clampf(x, b->x, b->x + b->w), dz = z - clampf(z, b->z, b->z + b->d);
    return dx * dx + dz * dz < r * r;
}
bool world_solid(World *w, float x, float z, float r) {
    if (!terrain(w, x, z, r))
        return true;
    for (int j = (int)floorf((z - r) / CELL); j <= (int)floorf((z + r) / CELL); j++)
        for (int i = (int)floorf((x - r) / CELL); i <= (int)floorf((x + r) / CELL); i++) {
            const Chunk *c = world_chunk(w, i, j);
            for (int k = 0; k < c->count; k++)
                if (overlap(&c->b[k], x, z, r))
                    return true;
        }
    return false;
}
bool world_resolve(World *w, Vehicle *v, float r, float *impact) {
    bool hit = false;
    *impact = 0;
    int cx = (int)floorf(v->x / CELL), cz = (int)floorf(v->z / CELL);
    if (!terrain(w, v->x, v->z, r)) {
        float bx = v->x, bz = v->z;
        int k = world_tile(w, cx, cz)->kind;
        if (k >= BRIDGE_EW) {
            Vec3 p = world_center(w, cz * 32 + cx);
            if (k == BRIDGE_EW)
                bz = clampf(bz, p.z - ROAD / 2 + r + .02f, p.z + ROAD / 2 - r - .02f);
            else
                bx = clampf(bx, p.x - ROAD / 2 + r + .02f, p.x + ROAD / 2 - r - .02f);
        }
        for (int attempt = 0; attempt < 8 && !terrain(w, bx, bz, r); attempt++) {
            float val = world_coast(w, bx, bz);
            float nx = world_coast(w, bx + .5f, bz) - world_coast(w, bx - .5f, bz);
            float nz = world_coast(w, bx, bz + .5f) - world_coast(w, bx, bz - .5f);
            float len = hypotf(nx, nz);
            if (len < .001f)
                break;
            float push = fmaxf(.2f, r - val + .1f);
            bx += nx / len * push;
            bz += nz / len * push;
        }
        if (!terrain(w, bx, bz, r)) {
            Vec3 p = world_center(w, world_nearest(w, v->x, v->z));
            bx = p.x;
            bz = p.z;
        }
        float dx = bx - v->x, dz = bz - v->z, len = sqrtf(dx * dx + dz * dz);
        if (len > .0001f) {
            dx /= len;
            dz /= len;
            float vn = v->vx * dx + v->vz * dz;
            if (vn < 0) {
                *impact = -vn;
                v->vx -= vn * dx * 1.15f;
                v->vz -= vn * dz * 1.15f;
            }
        }
        v->x = bx;
        v->z = bz;
        hit = true;
    }
    for (int j = (int)floorf((v->z - r) / CELL); j <= (int)floorf((v->z + r) / CELL); j++)
        for (int i = (int)floorf((v->x - r) / CELL); i <= (int)floorf((v->x + r) / CELL); i++) {
            const Chunk *c = world_chunk(w, i, j);
            for (int k = 0; k < c->count; k++) {
                const Building *b = &c->b[k];
                if (!overlap(b, v->x, v->z, r))
                    continue;
                float dx = v->x - clampf(v->x, b->x, b->x + b->w),
                      dz = v->z - clampf(v->z, b->z, b->z + b->d), len = sqrtf(dx * dx + dz * dz),
                      push = r - len + .001f;
                if (len < .0001f) {
                    float d[4] = {v->x - b->x, b->x + b->w - v->x, v->z - b->z, b->z + b->d - v->z};
                    int a = 0;
                    for (int q = 1; q < 4; q++)
                        if (d[q] < d[a])
                            a = q;
                    dx = a == 0 ? -1 : a == 1 ? 1 : 0;
                    dz = a == 2 ? -1 : a == 3 ? 1 : 0;
                    push = r + d[a] + .001f;
                } else {
                    dx /= len;
                    dz /= len;
                }
                v->x += dx * push;
                v->z += dz * push;
                float vn = v->vx * dx + v->vz * dz;
                if (vn < 0) {
                    *impact = fmaxf(*impact, -vn);
                    v->vx -= 1.12f * vn * dx;
                    v->vz -= 1.12f * vn * dz;
                }
                hit = true;
            }
        }
    return hit;
}

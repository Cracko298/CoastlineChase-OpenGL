#include "render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __3DS__
#include <3ds.h>
#define ALLOC linearAlloc
#define FREE linearFree
#else
#define ALLOC malloc
#define FREE free
#endif
static const unsigned CAP[PASS_COUNT] = {42000, 66000, 108000, 42000};
static Vec3 sub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}
static float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static Vec3 norm(Vec3 a) {
    float l = sqrtf(dot(a, a));
    if (l < .001f)
        l = 1;
    return (Vec3){a.x / l, a.y / l, a.z / l};
}
static Vec3 cross(Vec3 a, Vec3 b) {
    return (Vec3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static Color scale(Color c, float s) {
    c.r *= s;
    c.g *= s;
    c.b *= s;
    return c;
}
static Color fogged(Render *r, Color c, float z) {
    if (!r->settings || !r->settings->fog)
        return c;
    float f = clampf((z - r->far_clip * .35f) / (r->far_clip * .65f), 0, .98f);
    c.r += (r->fog.r - c.r) * f;
    c.g += (r->fog.g - c.g) * f;
    c.b += (r->fog.b - c.b) * f;
    return c;
}
bool render_init(Render *r) {
    memset(r, 0, sizeof *r);
    for (int i = 0; i < PASS_COUNT; i++) {
        r->mesh[i].capacity = CAP[i];
        r->mesh[i].v = ALLOC(CAP[i] * sizeof(Vertex));
        if (!r->mesh[i].v) {
            render_free(r);
            return false;
        }
    }
    return true;
}
void render_free(Render *r) {
    for (int i = 0; i < PASS_COUNT; i++) {
        if (r->mesh[i].v)
            FREE(r->mesh[i].v);
        r->mesh[i].v = NULL;
    }
}
void render_reset(Render *r, int w, int h) {
    r->dirty = true;
    r->width = w;
    r->height = h;
    for (int i = 0; i < PASS_COUNT; i++)
        r->mesh[i].count = r->mesh[i].dropped = 0;
}
static void tri(Render *r, int pass, Vertex a, Vertex b, Vertex c) {
    Mesh *m = &r->mesh[pass];
    if (m->count + 3 > m->capacity) {
        m->dropped += 3;
        return;
    }
    m->v[m->count++] = a;
    m->v[m->count++] = b;
    m->v[m->count++] = c;
}
static Vertex project(Render *r, Vec3 p, Color c) {
    Vec3 v = sub(p, r->eye);
    float d = dot(v, r->forward);
    float near = .6f;
    return (Vertex){dot(v, r->right) * r->focal / (r->width / (float)r->height),
                    dot(v, r->up) * r->focal, near * (d - r->far_clip) / (r->far_clip - near), d,
                    fogged(r, c, d)};
}
static Vertex screen(Render *r, float x, float y, Color c) {
    return (Vertex){2 * x / r->width - 1, 1 - 2 * y / r->height, -.5f, 1, c};
}
static bool reject(Vertex a, Vertex b, Vertex c) {
    return (a.w < .6f && b.w < .6f && c.w < .6f) || (a.x < -a.w && b.x < -b.w && c.x < -c.w) ||
           (a.x > a.w && b.x > b.w && c.x > c.w) || (a.y < -a.w && b.y < -b.w && c.y < -c.w) ||
           (a.y > a.w && b.y > b.w && c.y > c.w) || (a.z > 0 && b.z > 0 && c.z > 0);
}
void r_quad(Render *r, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Color color) {
    Vertex aa = project(r, a, color), bb = project(r, b, color), cc = project(r, c, color),
           dd = project(r, d, color);
    if (!reject(aa, bb, cc))
        tri(r, PASS_SOLID, aa, bb, cc);
    if (!reject(aa, cc, dd))
        tri(r, PASS_SOLID, aa, cc, dd);
}
void r_rect(Render *r, float x, float y, float w, float h, Color c) {
    Vertex a = screen(r, x, y, c), b = screen(r, x + w, y, c), cc = screen(r, x + w, y + h, c),
           d = screen(r, x, y + h, c);
    tri(r, PASS_UI, a, b, cc);
    tri(r, PASS_UI, a, cc, d);
}
void r_line2(Render *r, float x, float y, float xx, float yy, float width, Color c) {
    float dx = xx - x, dy = yy - y, len = sqrtf(dx * dx + dy * dy);
    if (len < .01f)
        return;
    float ox = -dy / len * width * .5f, oy = dx / len * width * .5f;
    Vertex a = screen(r, x + ox, y + oy, c), b = screen(r, xx + ox, yy + oy, c),
           cc = screen(r, xx - ox, yy - oy, c), d = screen(r, x - ox, y - oy, c);
    tri(r, PASS_UI, a, b, cc);
    tri(r, PASS_UI, a, cc, d);
}
static void line_strip(Render *r, Vertex a, Vertex b, float width, int pass, Color color) {
    float dx = (b.x / b.w - a.x / a.w) * r->width, dy = (b.y / b.w - a.y / a.w) * r->height;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < .001f)
        return;
    float ox = -dy / len * width / r->width, oy = dx / len * width / r->height;
    Vertex c = a, d = b;
    a.x += ox * a.w;
    a.y += oy * a.w;
    b.x += ox * b.w;
    b.y += oy * b.w;
    c.x -= ox * c.w;
    c.y -= oy * c.w;
    d.x -= ox * d.w;
    d.y -= oy * d.w;
    a.c = b.c = c.c = d.c = color;
    tri(r, pass, a, b, d);
    tri(r, pass, a, d, c);
}
static void projected_line(Render *r, Vertex a, Vertex b, Color c, float width, bool glow) {
    if (a.w < .6f && b.w < .6f)
        return;
    if (a.w < .6f || b.w < .6f) {
        float t = (.6f - a.w) / (b.w - a.w);
        Vertex v = a;
        v.x += (b.x - a.x) * t;
        v.y += (b.y - a.y) * t;
        v.z += (b.z - a.z) * t;
        v.w = .6f;
        if (a.w < .6f)
            a = v;
        else
            b = v;
    }
    if (reject(a, b, b))
        return;
    Color col = fogged(r, c, (a.w + b.w) * .5f);
    a.z -= .00015f * a.w;
    b.z -= .00015f * b.w;
    a.z = fmaxf(a.z, -a.w);
    b.z = fmaxf(b.z, -b.w);
    line_strip(r, a, b, width, PASS_EDGE, col);
    if (glow && r->settings->bloom) {
        int layers = r->settings->bloom;
        if ((a.w + b.w) * .5f > 115)
            layers = 1;
        for (int i = layers; i > 0; i--) {
            Color halo = col;
            halo.a = i == 1 ? .20f : i == 2 ? .075f : .035f;
            line_strip(r, a, b, width + 3.8f * i, PASS_GLOW, halo);
        }
    }
}
void r_line(Render *r, Vec3 aa, Vec3 bb, Color c, float width, bool glow) {
    projected_line(r, project(r, aa, c), project(r, bb, c), c, width, glow);
}
static Vec3 local(Vec3 p, Vec3 c, float sn, float cs) {
    return (Vec3){c.x + p.x * cs + p.z * sn, c.y + p.y, c.z - p.x * sn + p.z * cs};
}
void r_box(Render *r, Vec3 c, Vec3 size, float yaw, Color fill, Color edge, bool glow) {
    float sn = sinf(yaw), cs = cosf(yaw);
    Vec3 p[8];
    for (int i = 0; i < 8; i++)
        p[i] = local((Vec3){(i & 1 ? .5f : -.5f) * size.x, (i & 2 ? .5f : -.5f) * size.y,
                            (i & 4 ? .5f : -.5f) * size.z},
                     c, sn, cs);
    Vertex projected[8];
    for (int i = 0; i < 8; i++)
        projected[i] = project(r, p[i], fill);
    static const int faces[5][4] = {
        {0, 1, 3, 2}, {4, 6, 7, 5}, {0, 2, 6, 4}, {1, 5, 7, 3}, {2, 3, 7, 6}};
    static const float shade[] = {.6f, .8f, .55f, .75f, 1};
    float ex = (r->eye.x - c.x) * cs - (r->eye.z - c.z) * sn;
    float ez = (r->eye.x - c.x) * sn + (r->eye.z - c.z) * cs;
    bool show[] = {ez<-size.z * .5f, ez> size.z * .5f, ex<-size.x * .5f, ex> size.x * .5f,
                   r->eye.y > c.y + size.y * .5f};
    for (int i = 0; i < 5; i++)
        if (show[i]) {
            Vertex v[4];
            for (int j = 0; j < 4; j++) {
                v[j] = projected[faces[i][j]];
                v[j].c = fogged(r, scale(fill, shade[i]), v[j].w);
            }
            if (!reject(v[0], v[1], v[2]))
                tri(r, PASS_SOLID, v[0], v[1], v[2]);
            if (!reject(v[0], v[2], v[3]))
                tri(r, PASS_SOLID, v[0], v[2], v[3]);
        }
    if (edge.a <= 0)
        return;
    static const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                     {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

    for (int i = 0; i < 12; i++)
        projected_line(r, projected[edges[i][0]], projected[edges[i][1]], edge,
                       (float)r->settings->outline, glow);
}

static bool visible(Render *r, Vec3 p, float radius) {
    Vec3 d = sub(p, r->eye);
    float z = dot(d, r->forward);
    if (z < -radius || z > r->far_clip + radius)
        return false;
    float x = fabsf(dot(d, r->right));
    return x < (z + radius) * 1.15f + radius;
}
static void ground(Render *r, float x, float z, float w, float d, float y, Color c) {
    r_quad(r, (Vec3){x, y, z}, (Vec3){x + w, y, z}, (Vec3){x + w, y, z + d}, (Vec3){x, y, z + d},
           c);
}
static void pyramid(Render *r, Vec3 p, float width, float height, Color fill, Color edge) {
    Vec3 top = {p.x, p.y + height, p.z};
    for (int i = 0; i < 4; i++) {
        float a = PI * .25f + i * PI * .5f, b = a + PI * .5f;
        Vec3 aa = {p.x + cosf(a) * width, p.y, p.z + sinf(a) * width},
             bb = {p.x + cosf(b) * width, p.y, p.z + sinf(b) * width};
        r_quad(r, aa, bb, top, top, scale(fill, .7f + i * .1f));
        r_line(r, aa, top, edge, .7f, false);
        r_line(r, aa, bb, edge, .7f, false);
    }
}
static void tree(Render *r, float x, float z, float h, float width, int kind, Color edge) {
    Color bark = {.10f, .068f, .048f, 1},
          leaf = kind == 3 ? (Color){.17f, .09f, .025f, 1} : (Color){.023f, .115f, .068f, 1};
    r_box(r, (Vec3){x, h * .3f, z}, (Vec3){.5f, h * .6f, .5f}, 0, bark, (Color){0}, false);
    if (kind == 0) {
        r_line(r, (Vec3){x, h * .5f, z}, (Vec3){x + .5f, h, z}, bark, 2, false);
        for (int i = 0; i < 6; i++) {
            float a = i * PI / 3;
            Vec3 mid = {x + .5f + sinf(a) * width * .65f, h + .5f, z + cosf(a) * width * .65f},
                 end = {x + .5f + sinf(a) * width, h - 1.8f, z + cosf(a) * width};
            r_line(r, (Vec3){x + .5f, h, z}, mid, scale(edge, .55f), 1.2f, false);
            r_line(r, mid, end, scale(edge, .55f), 1.2f, false);
        }
    } else if (kind == 1) {
        for (int j = 0; j < 3; j++)
            pyramid(r, (Vec3){x, h * .25f + j * h * .2f, z}, width * (1 - j * .23f), h * .55f, leaf,
                    scale(edge, .35f));
    } else {
        pyramid(r, (Vec3){x, h * .47f, z}, width, h * .48f, leaf, scale(edge, .32f));
        pyramid(r, (Vec3){x, h * .48f, z}, width, -h * .18f, leaf, scale(edge, .32f));
        if (r->settings->detail == 2)
            r_box(r, (Vec3){x + width * .5f, h * .56f, z},
                  (Vec3){width * .8f, h * .22f, width * .8f}, .5f, leaf, scale(edge, .2f), false);
    }
}
#include "scenery.inc"
static void facade(Render *r, const Building *b, int face, int detail, uint32_t hash, Color edge) {
    bool alongx = face == 0;
    float span = alongx ? b->w : b->d;
    int columns = alongx ? b->windows_x : b->windows_z;
    int rows = b->floors;
    int step = (rows + (detail == 2 ? 11 : 5)) / (detail == 2 ? 12 : 6);
    if (step < 1)
        step = 1;
    float baseline = alongx ? b->z - .025f : b->x - .025f;
    for (int f = 0; f < rows; f += step)
        for (int c = 0; c < columns; c++) {
            float offset = (c + 1) * span / (columns + 1), y = 2.9f + f * 3.8f;
            if (y + 1.3f > b->h - .5f)
                continue;
            float ww = fminf(1.6f, span / (columns + 1) * .55f);
            bool lit = hash_cell(c, f, hash) % 4 != 0;
            Color glass = lit ? (Color){.95f, .72f, .32f, 1} : (Color){.025f, .065f, .09f, 1};
            if (b->style == 2 && lit)
                glass = scale(edge, .60f);
            if (alongx)
                r_quad(r, (Vec3){b->x + offset - ww / 2, y, baseline},
                       (Vec3){b->x + offset + ww / 2, y, baseline},
                       (Vec3){b->x + offset + ww / 2, y + 1.35f, baseline},
                       (Vec3){b->x + offset - ww / 2, y + 1.35f, baseline}, glass);
            else
                r_quad(r, (Vec3){baseline, y, b->z + offset - ww / 2},
                       (Vec3){baseline, y, b->z + offset + ww / 2},
                       (Vec3){baseline, y + 1.35f, b->z + offset + ww / 2},
                       (Vec3){baseline, y + 1.35f, b->z + offset - ww / 2}, glass);
            if (lit && r->settings->lighting && f < 3 &&
                hypotf(b->x - r->eye.x, b->z - r->eye.z) < 70) {
                float sign = (alongx ? r->eye.z : r->eye.x) > baseline ? 1 : -1;
                Vec3 center = alongx ? (Vec3){b->x + offset, y + .67f, baseline + sign * .04f}
                                     : (Vec3){baseline + sign * .04f, y + .67f, b->z + offset};
                window_spill(r, center, alongx, ww, glass);
                if (f == 0 && c % 2 == 0) {
                    Vec3 pool = center;
                    if (alongx)
                        pool.z += sign * 2;
                    else
                        pool.x += sign * 2;
                    light_pool(r, pool, 4.5f, glass);
                }
            }
            if (lit) {
                Vec3 a = alongx ? (Vec3){b->x + offset - ww * .5f, y + .7f, baseline - .015f}
                                : (Vec3){baseline - .015f, y + .7f, b->z + offset - ww * .5f};
                Vec3 q = alongx ? (Vec3){a.x + ww, a.y, a.z} : (Vec3){a.x, a.y, a.z + ww};
                r_line(r, a, q, scale(glass, 1.4f), 1.3f, true);
            }
        }
}
static void building(Render *r, const Building *b, uint32_t hash, const Theme *t) {
    Vec3 p = {b->x + b->w / 2, b->h / 2, b->z + b->d / 2};
    if (!visible(r, p, b->h * .5f + 14))
        return;
    float d = hypotf(p.x - r->eye.x, p.z - r->eye.z);
    Color edge = r->settings->white ? (Color){.72f, .77f, .8f, 1}
                 : b->palette & 1   ? t->accent
                                    : t->outline;
    edge = scale(edge, .63f);
    Color fill = scale(edge, .22f + r->daylight * .18f);
    fill.a = 1;
    r_box(r, p, (Vec3){b->w, b->h, b->d}, 0, fill, edge, d < 90 && b->style == 1);
    if (d > (r->settings->detail ? 145 : 55))
        return;

    Building face = *b;
    if (r->eye.z > p.z)
        face.z = b->z + b->d + .05f;
    facade(r, &face, 0, r->settings->detail, hash, edge);
    face = *b;
    if (r->eye.x > p.x)
        face.x = b->x + b->w + .05f;
    facade(r, &face, 1, r->settings->detail, hash + 6, edge);
    float doorwidth = b->door == 2 ? b->w * .55f : b->door == 1 ? 2.8f : 1.6f;
    float doorz = r->eye.z > p.z ? b->z + b->d + .04f : b->z - .04f, doorx = b->x + b->w * .5f;
    Color door = {.04f, .07f, .085f, 1};
    r_quad(r, (Vec3){doorx - doorwidth / 2, .02f, doorz},
           (Vec3){doorx + doorwidth / 2, .02f, doorz}, (Vec3){doorx + doorwidth / 2, 2.6f, doorz},
           (Vec3){doorx - doorwidth / 2, 2.6f, doorz}, door);
    r_line(r, (Vec3){doorx - doorwidth / 2, 0, doorz}, (Vec3){doorx - doorwidth / 2, 2.6f, doorz},
           edge, .8f, true);
    r_line(r, (Vec3){doorx + doorwidth / 2, 0, doorz}, (Vec3){doorx + doorwidth / 2, 2.6f, doorz},
           edge, .8f, true);
    r_line(r, (Vec3){doorx - doorwidth / 2, 2.6f, doorz},
           (Vec3){doorx + doorwidth / 2, 2.6f, doorz}, edge, .8f, true);
    if (b->door == 1)
        r_line(r, (Vec3){doorx, 0, doorz - .02f}, (Vec3){doorx, 2.6f, doorz - .02f}, edge, .8f,
               false);
    if (b->door == 2)
        for (int i = 1; i < 6; i++)
            r_line(r, (Vec3){doorx - doorwidth / 2, i * .4f, doorz - .02f},
                   (Vec3){doorx + doorwidth / 2, i * .4f, doorz - .02f}, scale(edge, .6f), .6f,
                   false);
    if (b->door == 3) {
        r_box(r, (Vec3){doorx, 2.8f, b->z - .8f}, (Vec3){doorwidth + 2, .35f, 2}, 0,
              scale(t->accent, .18f), t->accent, true);
        r_line(r, (Vec3){doorx + .4f, 1.1f, doorz - .05f}, (Vec3){doorx + .4f, 1.5f, doorz - .05f},
               t->accent, 1, false);
    }
    if (d < 90 && r->settings->detail && b->business < 12) {
        static const char *shops[] = {"NEON CAFE", "VINYL",  "VIDEO CLUB", "RADIO 88",
                                      "MARKET",    "FIX IT", "LAUNDRY",    "OPEN 24H",
                                      "SURF SHOP", "PIZZA",  "MOTEL",      "ARCADE"};
        float signz = r->eye.z > p.z ? b->z + b->d + .08f : b->z - .08f;
        neon_sign(r, (Vec3){p.x, 3.9f, signz}, fminf(11, b->w - .5f), 1.5f, shops[b->business % 12],
                  t->accent);
        if (hash % 5 == 0 && d < 70) {
            float y = b->h + 3.5f;
            for (int side = -1; side <= 1; side += 2)
                r_box(r, (Vec3){p.x + side * 3, b->h + 1.5f, p.z}, (Vec3){.2f, 3, .2f}, 0, fill,
                      edge, false);
            neon_sign(r, (Vec3){p.x, y, p.z}, 12, 3.8f, hash & 1 ? "NEON 1986" : "RADIO 88",
                      t->outline);
        }
    }
    if (b->roof == 1) {
        Vec3 a = {b->x - .3f, b->h, b->z - .3f}, bb = {b->x + b->w + .3f, b->h, b->z - .3f},
             c = {b->x + b->w * .5f, b->h + 3, b->z - .3f}, d1 = {c.x, c.y, b->z + b->d + .3f};
        r_quad(r, a, bb, c, c, scale(fill, 1.4f));
        r_quad(r, a, c, d1, (Vec3){a.x, a.y, b->z + b->d + .3f}, scale(fill, 1.3f));
        r_quad(r, bb, c, d1, (Vec3){bb.x, bb.y, b->z + b->d + .3f}, fill);
        r_line(r, c, d1, edge, 1, false);
        r_line(r, a, c, edge, 1, false);
        r_line(r, bb, c, edge, 1, false);
    } else if (b->roof == 2) {
        r_box(r, (Vec3){p.x, b->h + 1, p.z}, (Vec3){b->w * .5f, 2, b->d * .45f}, 0, fill, edge,
              false);
        r_line(r, (Vec3){p.x, b->h + 2, p.z}, (Vec3){p.x, b->h + 7, p.z}, edge, 1, false);
    } else if (b->roof == 3) {
        r_box(r, (Vec3){p.x, b->h + 1.5f, p.z}, (Vec3){3, 3, 3}, 0, (Color){.08f, .09f, .1f, 1},
              edge, false);
    }
    if (b->style == 1 && d < 90) {
        r_box(r, (Vec3){b->x + b->w * .5f, 3.2f, b->z - .4f}, (Vec3){b->w * .7f, .5f, .8f}, 0,
              scale(t->accent, .25f), scale(t->accent, .7f), true);
    }
}
static void bridge(Render *r, const Chunk *c, const Theme *t) {
    float x = c->x * CELL, z = c->z * CELL;
    bool ew = c->kind == BRIDGE_EW;
    Vec3 center = {x + 32, -.65f, z + 32}, size = ew ? (Vec3){64, 1.3f, 20} : (Vec3){20, 1.3f, 64};
    r_box(r, center, size, 0, (Color){.07f, .08f, .10f, 1}, scale(t->outline, .4f), false);
    for (int side = -1; side <= 1; side += 2) {
        Vec3 a = ew ? (Vec3){x, .9f, z + 32 + side * 9.5f} : (Vec3){x + 32 + side * 9.5f, .9f, z},
             b = ew ? (Vec3){x + 64, .9f, a.z} : (Vec3){a.x, .9f, z + 64};
        r_line(r, a, b, t->outline, 1.4f, true);
        for (int i = 0; i < 5; i++) {
            Vec3 p = ew ? (Vec3){x + 8 + i * 12, 0, a.z} : (Vec3){a.x, 0, z + 8 + i * 12};
            r_line(r, p, (Vec3){p.x, 1.5f, p.z}, scale(t->outline, .6f), 1, false);
        }
        Vec3 tower =
            ew ? (Vec3){x + 32, 8, z + 32 + side * 11} : (Vec3){x + 32 + side * 11, 8, z + 32};
        r_box(r, tower, (Vec3){1.3f, 16, 1.3f}, 0, (Color){.075f, .09f, .11f, 1},
              scale(t->outline, .65f), false);
        Vec3 top = {tower.x, 16, tower.z};
        r_line(r, top, a, scale(t->accent, .65f), 1, true);
        r_line(r, top, b, scale(t->accent, .65f), 1, true);
    }
}
static void road_segment(Render *r, Vec3 a, Vec3 b, float width, Color color) {
    float dx = b.x - a.x, dz = b.z - a.z, len = hypotf(dx, dz);
    if (len < .01f)
        return;
    float ox = dz / len * width * .5f, oz = -dx / len * width * .5f;
    r_quad(r, (Vec3){a.x + ox, a.y, a.z + oz}, (Vec3){b.x + ox, b.y, b.z + oz},
           (Vec3){b.x - ox, b.y, b.z - oz}, (Vec3){a.x - ox, a.y, a.z - oz}, color);
}
static void roads(Render *r, Game *g, const Chunk *c) {
    int n = c->z * 32 + c->x;
    Vec3 a = world_center(&g->world, n);
    a.y = .02f;
    Color asphalt = {.028f + r->daylight * .045f, .035f + r->daylight * .045f,
                     .05f + r->daylight * .045f, 1};
    ground(r, a.x - 9, a.z - 9, 18, 18, .019f, asphalt);
    for (int d = 0; d < 4; d++) {
        int next = world_neighbor(&g->world, n, d);
        if (next < 0)
            continue;
        Vec3 b = world_center(&g->world, next);
        b.x = (a.x + b.x) * .5f;
        b.z = (a.z + b.z) * .5f;
        b.y = a.y;
        road_segment(r, a, b, 18, asphalt);
        float dx = b.x - a.x, dz = b.z - a.z, len = hypotf(dx, dz);
        dx /= len;
        dz /= len;
        for (float m = 14; m < len; m += 9) {
            Vec3 p = {a.x + dx * m, .028f, a.z + dz * m}, q = {p.x + dx * 4, .028f, p.z + dz * 4};
            road_segment(r, p, q, .25f, (Color){.48f, .39f, .22f, 1});
        }
        if (r->settings->detail && c->kind < BRIDGE_EW)
            for (int stripe = -3; stripe <= 3; stripe++) {
                Vec3 p = {a.x + dx * 12 + dz * stripe * 2, .03f, a.z + dz * 12 - dx * stripe * 2};
                Vec3 q = {p.x + dx * 2.5f, .03f, p.z + dz * 2.5f};
                road_segment(r, p, q, 1, (Color){.23f, .24f, .25f, 1});
            }
    }
}

static void coast_triangle(Render *r, Vec3 a, Vec3 b, Vec3 c, float fa, float fb, float fc,
                           Color base, Color edge) {
    Vec3 in[3] = {a, b, c}, out[5], coast[3];
    float f[3] = {fa, fb, fc};
    int n = 0, nc = 0;
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        if (f[i] >= 0)
            out[n++] = in[i];
        if ((f[i] >= 0) != (f[j] >= 0)) {
            float t = f[i] / (f[i] - f[j]);
            Vec3 q = {in[i].x + (in[j].x - in[i].x) * t, -.05f, in[i].z + (in[j].z - in[i].z) * t};
            out[n++] = q;
            coast[nc++] = q;
        }
    }
    if (n >= 3)
        r_quad(r, out[0], out[1], out[2], n == 4 ? out[3] : out[2], base);
    if (nc == 2) {
        coast[0].y = coast[1].y = .02f;
        r_line(r, coast[0], coast[1], edge, 1.1f, true);
    }
}
static void coast_tile(Render *r, const World *w, int tx, int tz, Color base, Color edge) {
    if (tx < 0 || tz < 0 || tx >= MAP_SIDE || tz >= MAP_SIDE)
        return;
    int ix = tx * 4, iz = tz * 4;
    if (w->shore_kind[tz * MAP_SIDE + tx] == 0)
        return;
    if (w->shore_kind[tz * MAP_SIDE + tx] == 2) {
        ground(r, tx * CELL, tz * CELL, CELL, CELL, -.05f, base);
        return;
    }
    for (int z = 0; z < 4; z++)
        for (int x = 0; x < 4; x++) {
            const int16_t *f = &w->shore[(iz + z) * SHORE_SIDE + ix + x];
            float xx = (ix + x) * SHORE_STEP, zz = (iz + z) * SHORE_STEP;
            Vec3 a = {xx, -.05f, zz}, b = {xx + SHORE_STEP, -.05f, zz},
                 c = {xx, -.05f, zz + SHORE_STEP}, d = {xx + SHORE_STEP, -.05f, zz + SHORE_STEP};
            coast_triangle(r, a, b, d, f[0], f[1], f[SHORE_SIDE + 1], base, edge);
            coast_triangle(r, a, d, c, f[0], f[SHORE_SIDE + 1], f[SHORE_SIDE], base, edge);
        }
}
static void street_props(Render *r, Game *g, const Chunk *c, const Theme *t, float distance) {
    if (!r->settings->detail || distance > 105 || c->kind >= BRIDGE_EW)
        return;
    float x = c->x * CELL, z = c->z * CELL;
    Color metal = {.055f, .07f, .09f, 1};
    for (int i = 0; i < 2; i++) {
        float px = x + (i ? 44 : 20), pz = z + (i ? 49 : 15);
        light_pool(r, (Vec3){px + (i ? -2 : 2), 0, pz}, 7.5f, (Color){1, .72f, .35f, 1});
        r_box(r, (Vec3){px, 2.5f, pz}, (Vec3){.22f, 5, .22f}, 0, metal, (Color){0}, false);
        r_line(r, (Vec3){px, 5, pz}, (Vec3){px + (i ? -2 : 2), 5, pz}, scale(t->outline, .4f), 1,
               false);
        r_box(r, (Vec3){px + (i ? -2 : 2), 5, pz}, (Vec3){1, .15f, .6f}, 0,
              (Color){.4f, .33f, .16f, 1}, (Color){.8f, .65f, .3f, 1}, true);
    }
    int phase = ((int)(g->run_time / 8) + (c->x + c->z) % 3) & 1;
    Color light = phase ? (Color){1, .13f, .12f, 1} : (Color){.15f, .8f, .3f, 1};
    r_box(r, (Vec3){x + 42, 2, z + 21}, (Vec3){.18f, 4, .18f}, 0, metal, (Color){0}, false);
    r_box(r, (Vec3){x + 42, 4.1f, z + 21}, (Vec3){.6f, 1.2f, .5f}, 0, metal,
          scale(t->outline, .25f), false);
    r_line(r, (Vec3){x + 41.85f, 4.35f, z + 20.72f}, (Vec3){x + 42.15f, 4.35f, z + 20.72f}, light,
           2, true);
    if (c->kind == PARK) {
        ground(r, x + 2, z + 12, 21, 2, .05f, (Color){.13f, .11f, .08f, 1});
        r_box(r, (Vec3){x + 52, .65f, z + 12}, (Vec3){7, 1.3f, 7}, 0, (Color){.05f, .10f, .12f, 1},
              scale(t->outline, .5f), false);
        ground(r, x + 49, z + 9, 6, 6, 1.31f, scale(t->outline, .22f));
        r_line(r, (Vec3){x + 52, 1.3f, z + 12},
               (Vec3){x + 52, 3.3f + sinf(g->time * 2) * .2f, z + 12}, scale(t->outline, .7f), 1.3f,
               true);
    }
    for (int j = 0; j < (c->kind == PARK ? 4 : 1); j++) {
        float px = x + (j & 1 ? 52 : 11), pz = z + (j & 2 ? 53 : 14);
        if (c->kind != PARK) {
            px = x + 21.8f;
            pz = z + 8;
        }
        if (c->kind == PARK || c->kind == BEACH || c->kind == MARINA || c->kind == SUBURBS) {
            int kind = c->kind == BEACH || c->kind == MARINA ? 0 : (int)((c->hash >> (j * 3)) % 4);
            float height = 6 + ((c->hash >> (j * 3)) % 7),
                  width = 2.3f + ((c->hash >> (j * 4)) % 4) * .65f;
            tree(r, px, pz, height, width, kind, t->outline);
        }
    }
    if (distance < 70) {
        r_box(r, (Vec3){x + 44, .7f, z + 4}, (Vec3){2.5f, 1.4f, 1.5f}, 0,
              (Color){.03f, .09f, .065f, 1}, scale(t->outline, .25f), false);
        r_box(r, (Vec3){x + 12, .65f, z + 42}, (Vec3){3, .3f, .9f}, 0,
              (Color){.11f, .075f, .04f, 1}, scale(t->outline, .2f), false);
        r_box(r, (Vec3){x + 12, 1.05f, z + 42.4f}, (Vec3){3, .7f, .15f}, 0,
              (Color){.11f, .075f, .04f, 1}, (Color){0}, false);
        if (c->kind == INDUSTRY) {
            r_box(r, (Vec3){x + 10, 2, z + 53}, (Vec3){13, 4, 5}, 0, (Color){.11f, .045f, .035f, 1},
                  scale(t->accent, .5f), false);
            for (int i = 0; i < 7; i++)
                r_line(r, (Vec3){x + 4 + i * 1.6f, .1f, z + 50.4f},
                       (Vec3){x + 4 + i * 1.6f, 3.9f, z + 50.4f}, scale(t->accent, .3f), .7f,
                       false);
        }
    }
}
static void city(Render *r, Game *g) {
    const Theme *t = &THEMES[preview_theme(g)];
    int cx = (int)floorf(r->focus.x / CELL), cz = (int)floorf(r->focus.z / CELL),
        radius = r->settings->view;
    ground(r, r->focus.x - r->far_clip * 1.4f, r->focus.z - r->far_clip * 1.4f, r->far_clip * 2.8f, r->far_clip * 2.8f,
           -.9f, t->sea);
    for (int ring = 0; ring <= radius; ring++)
        for (int z = cz - ring; z <= cz + ring; z++)
            for (int x = cx - ring; x <= cx + ring; x++) {
                if (ring && abs(x - cx) != ring && abs(z - cz) != ring)
                    continue;
                float xx = x * CELL, zz = z * CELL;
                if (!visible(r, (Vec3){xx + 32, 18, zz + 32}, 65))
                    continue;
                Chunk c = *world_chunk(&g->world, x, z);
                float distance = hypotf(xx + 32 - r->focus.x, zz + 32 - r->focus.z);
                Color base = c.kind == PARK               ? (Color){.025f, .075f, .05f, 1}
                             : c.kind == BEACH || !c.kind ? (Color){.14f, .10f, .065f, 1}
                                                          : (Color){.067f, .066f, .077f, 1};
                coast_tile(r, &g->world, x, z, base, scale(t->outline, .7f));
                if (!c.kind) {
                    if (distance < 150 && r->settings->detail)
                        for (int i = 0; i < 3; i++) {
                            float wave = sinf(g->time + z + i) * 1.5f;
                            r_line(r, (Vec3){xx + 8, -.82f, zz + 12 + i * 17 + wave},
                                   (Vec3){xx + 43, -.82f, zz + 14 + i * 17 + wave},
                                   scale(t->outline, .17f), .6f, false);
                        }
                    continue;
                }
                if (c.kind >= BRIDGE_EW) {
                    bridge(r, &c, t);
                    roads(r, g, &c);
                    continue;
                }
                roads(r, g, &c);
                for (int i = 0; i < c.count; i++)
                    building(r, &c.b[i], c.hash + i, t);
                street_props(r, g, &c, t, distance);
                if (distance < 170)
                    landmark(r, g, &c, t);
            }
}
static Vec3 carpoint(Render *r, Vehicle v, Vec3 p) {
    p.y += v.lean * p.x + v.pitch * p.z;
    p.x *= r->car_scale;
    p.y *= r->car_scale;
    p.z *= r->car_scale;
    return local(p, (Vec3){v.x, 0, v.z}, r->car_sn, r->car_cs);
}
static void carpart(Render *r, Vehicle v, Vec3 p, Vec3 size, Color fill, Color edge, bool glow) {
    size.x *= r->car_scale;
    size.y *= r->car_scale;
    size.z *= r->car_scale;
    r_box(r, carpoint(r, v, p), size, v.yaw, fill, edge, glow);
}
static void carline(Render *r, Vehicle v, Vec3 a, Vec3 b, Color c, float width, bool glow) {
    r_line(r, carpoint(r, v, a), carpoint(r, v, b), c, width, glow);
}
static void carquad(Render *r, Vehicle v, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Color fill) {
    r_quad(r, carpoint(r, v, a), carpoint(r, v, b), carpoint(r, v, c), carpoint(r, v, d), fill);
}
static void shell(Render *r, Vehicle v, float w, float length, float zback, float zfront, float low,
                  float high, float taper, Color fill, Color edge, bool open) {
    Vec3 p[8] = {{-w / 2, low, zback},
                 {w / 2, low, zback},
                 {-w / 2 * taper, high, zback + .25f},
                 {w / 2 * taper, high, zback + .25f},
                 {-w / 2, low, zfront},
                 {w / 2, low, zfront},
                 {-w / 2 * taper, high, zfront - .5f},
                 {w / 2 * taper, high, zfront - .5f}};
    (void)length;
    static const int f[5][4] = {
        {0, 1, 3, 2}, {4, 6, 7, 5}, {0, 2, 6, 4}, {1, 5, 7, 3}, {2, 3, 7, 6}};
    for (int i = 0; i < (open ? 4 : 5); i++)
        carquad(r, v, p[f[i][0]], p[f[i][1]], p[f[i][2]], p[f[i][3]], scale(fill, .65f + i * .08f));
    static const int e[][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                               {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (int i = 0; i < 12; i++)
        carline(r, v, p[e[i][0]], p[e[i][1]], edge, 1, true);
}
static Vec3 wheelpoint(Render *r, Vehicle v, Vec3 center, Vec3 p, float steer) {
    float sn = sinf(steer), cs = cosf(steer);
    return carpoint(
        r, v,
        (Vec3){center.x + p.x * cs + p.z * sn, center.y + p.y, center.z - p.x * sn + p.z * cs});
}
static void wheel(Render *r, Vehicle v, float x, float z, float radius, int style, Color edge,
                  bool front) {
    Vec3 center = {x, radius, z};
    float side = x < 0 ? -1 : 1, steer = front ? v.steer * .48f : 0;
    Color rubber = {.011f, .013f, .019f, 1},
          rim = style == 2 ? (Color){.55f, .54f, .49f, 1} : (Color){.20f, .25f, .29f, 1};
    int slices = r->settings->detail == 2 ? 10 : 8;
    float outer = side * .23f;
    for (int i = 0; i < slices; i++) {
        float a = i * 2 * PI / slices, b = (i + 1) * 2 * PI / slices;
        Vec3 aa = {outer, sinf(a) * radius, cosf(a) * radius},
             bb = {outer, sinf(b) * radius, cosf(b) * radius}, cc = {-outer, bb.y, bb.z},
             dd = {-outer, aa.y, aa.z};
        r_quad(r, wheelpoint(r, v, center, aa, steer), wheelpoint(r, v, center, bb, steer),
               wheelpoint(r, v, center, cc, steer), wheelpoint(r, v, center, dd, steer), rubber);
        Vec3 mid = {outer + side * .005f, 0, 0};
        r_quad(r, wheelpoint(r, v, center, aa, steer), wheelpoint(r, v, center, bb, steer),
               wheelpoint(r, v, center, mid, steer), wheelpoint(r, v, center, mid, steer),
               style == 5 ? rim : rubber);
        r_line(r, wheelpoint(r, v, center, aa, steer), wheelpoint(r, v, center, bb, steer),
               style == 2 ? rim : scale(edge, .34f), .7f, true);
    }
    int spokes = style == 6 ? 8 : style == 1 ? 5 : style == 4 ? 6 : 4;
    if (style != 5)
        for (int i = 0; i < spokes; i++) {
            float a = i * 2 * PI / spokes + v.wheel_spin;
            Vec3 aa = {outer + side * .01f, 0, 0},
                 bb = {aa.x, sinf(a) * radius * .68f, cosf(a) * radius * .68f};
            r_line(r, wheelpoint(r, v, center, aa, steer), wheelpoint(r, v, center, bb, steer), rim,
                   1, true);
        }
}
static void decals(Render *r, Vehicle v, float w, float l, int decal) {
    if (!decal)
        return;
    Color white = {.8f, .85f, .88f, 1};
    float front = l * .47f, back = -l * .46f;
    float top = 1.005f;
    if (decal == 1 || decal == 2) {
        int n = decal == 1 ? 1 : 2;
        for (int j = 0; j < n; j++) {
            float x = n == 1 ? 0 : (j ? 1 : -1) * w * .15f;
            carquad(r, v, (Vec3){x - .13f, top, back}, (Vec3){x + .13f, top, back},
                    (Vec3){x + .13f, top, front}, (Vec3){x - .13f, top, front}, white);
        }
    } else if (decal == 6) {
        for (int x = 0; x < 4; x++)
            for (int z = 0; z < 5; z++)
                if ((x + z) % 2 == 0) {
                    float xx = (x - 2) * w * .16f, zz = l * .25f + z * l * .043f;
                    carquad(r, v, (Vec3){xx, top, zz}, (Vec3){xx + w * .16f, top, zz},
                            (Vec3){xx + w * .16f, top, zz + l * .043f},
                            (Vec3){xx, top, zz + l * .043f}, white);
                }
    } else if (decal == 5) {
        for (int side = -1; side <= 1; side += 2) {
            float x = side * (w * .501f);
            carquad(r, v, (Vec3){x, .45f, -.55f}, (Vec3){x, .45f, .55f}, (Vec3){x, .85f, .55f},
                    (Vec3){x, .85f, -.55f}, white);
            carline(r, v, (Vec3){x + side * .015f, .52f, -.15f},
                    (Vec3){x + side * .015f, .78f, -.15f}, (Color){.02f, .02f, .03f, 1}, 1.3f,
                    false);
            carline(r, v, (Vec3){x + side * .015f, .52f, .15f},
                    (Vec3){x + side * .015f, .78f, .15f}, (Color){.02f, .02f, .03f, 1}, 1.3f,
                    false);
        }
    } else {
        for (int side = -1; side <= 1; side += 2)
            for (int j = 0; j < 9; j++) {
                float za = -l * .43f + j * l * .095f, zb = za + l * .095f;
                float ya = decal == 7   ? .66f + sinf(j * 1.2f) * .11f
                           : decal == 4 ? (j % 2 ? .48f : .83f)
                                        : .5f + (j % 3) * .13f;
                float yb = decal == 7   ? .66f + sinf((j + 1) * 1.2f) * .11f
                           : decal == 4 ? ((j + 1) % 2 ? .48f : .83f)
                                        : .5f + ((j + 1) % 3) * .13f;
                carline(r, v, (Vec3){side * w * .505f, ya, za}, (Vec3){side * w * .505f, yb, zb},
                        decal == 3   ? PAINTS[2]
                        : decal == 4 ? PAINTS[3]
                                     : PAINTS[0],
                        1.4f, false);
            }
    }
}
#include "unusual.inc"
void r_car(Render *r, Game *g, Vehicle v, int model, CarStyle style, int cop) {
    r->car_sn = sinf(v.yaw);
    r->car_cs = cosf(v.yaw);
    r->car_scale = 1;
    if (!cop && g->giant > 0 && g->screen != GARAGE && g->screen != SHOP &&
        fabsf(v.x - g->player.x) < .01f && fabsf(v.z - g->player.z) < .01f)
        r->car_scale = player_scale(g);
    const CarSpec *spec = &CARS[model];
    float w = spec->width, l = spec->length, h = spec->height;
    Color paint = cop ? (style.paint % 3 == 0   ? (Color){.75f, .85f, .98f, 1}
                         : style.paint % 3 == 1 ? (Color){.25f, .7f, 1, 1}
                                                : (Color){.7f, 1, .6f, 1})
                      : PAINTS[style.paint],
          edge = g->profile.settings.white ? (Color){.92f, .94f, 1, 1} : paint;
    Color fill = scale(paint, cop ? .12f : .20f), dark = {.018f, .022f, .032f, 1},
          glass = {.035f, .10f, .14f, 1};
    int antenna = style.antenna, hat = style.hat;
    float distance = hypotf(v.x - r->eye.x, v.z - r->eye.z);
    float roof = 1 + h * .48f;
    if (model >= 20) {
        roof = unusual_car(r, v, model, style, fill, edge);
        goto accessories;
    }
    if (distance > 48 && g->screen != GARAGE && g->screen != SHOP) {
        carpart(r, v, (Vec3){0, .6f, 0}, (Vec3){w, 1, l}, fill, edge, false);
        carpart(r, v, (Vec3){0, 1.25f, 0}, (Vec3){w * .8f, h * .55f, l * .52f}, glass, edge, false);
        for (int side = -1; side <= 1; side += 2)
            for (int z = -1; z <= 1; z += 2)
                carpart(r, v, (Vec3){side * w * .5f, .35f, z * l * .32f}, (Vec3){.35f, .7f, .75f},
                        dark, (Color){0}, false);
        if (cop)
            carpart(r, v, (Vec3){0, 1.6f, 0}, (Vec3){w * .7f, .22f, .3f}, dark,
                    ((int)(g->time * 7) & 1) ? PAINTS[6] : PAINTS[7], true);
        return;
    }
    if (r->settings->lighting && distance < 30) {
        Color lamp =
            cop ? (((int)(g->time * 7) & 1) ? PAINTS[6] : PAINTS[7]) : (Color){.7f, .82f, 1, 1};
        if (cop)
            light_pool(r, (Vec3){v.x, 0, v.z}, 4, lamp);
        Vec3 a = carpoint(r, v, (Vec3){-.7f, .065f, l * .48f}),
             b = carpoint(r, v, (Vec3){.7f, .065f, l * .48f});
        Vec3 c = carpoint(r, v, (Vec3){4, .065f, l * .48f + 16}),
             d = carpoint(r, v, (Vec3){-4, .065f, l * .48f + 16});
        Vertex aa = project(r, a, lamp), bb = project(r, b, lamp), cc = project(r, c, lamp),
               dd = project(r, d, lamp);
        aa.c.a = bb.c.a = .23f;
        cc.c.a = dd.c.a = 0;
        if (!reject(aa, bb, cc))
            tri(r, PASS_GLOW, aa, bb, cc);
        if (!reject(aa, cc, dd))
            tri(r, PASS_GLOW, aa, cc, dd);
    }
    if (r->settings->shadows) {
        carquad(r, v, (Vec3){-w * .65f, .013f, -l * .57f}, (Vec3){w * .65f, .013f, -l * .57f},
                (Vec3){w * .65f, .013f, l * .57f}, (Vec3){-w * .65f, .013f, l * .57f},
                (Color){.009f, .012f, .02f, 1});
    }
    shell(r, v, w, l, -l * .5f, l * .5f, .32f, .98f, .94f, fill, edge, false);
    float back = -l * .28f, front = l * .20f;
    if (model == 4 || model == 11 || model == 14 || model == 18) {
        back = -l * .44f;
        front = l * .39f;
    }
    if (model == 12) {
        back = -l * .37f;
        front = 0;
    }
    if (model == 13) {
        back = -l * .03f;
        front = l * .36f;
    }
    bool open = model == 8 || model == 9;
    shell(r, v, w * .84f, l, back, front, 1, roof, .86f, glass, edge, open);
    if (open) {
        carpart(r, v, (Vec3){0, 1.03f, back + .45f}, (Vec3){w * .65f, .35f, .45f}, dark, edge,
                false);
        if (model == 9) {
            carline(r, v, (Vec3){-w * .43f, roof + .25f, back}, (Vec3){w * .43f, roof + .25f, back},
                    edge, 1.5f, true);
            carline(r, v, (Vec3){-w * .43f, roof + .25f, back}, (Vec3){-w * .46f, .5f, -l * .48f},
                    edge, 1.5f, false);
            carline(r, v, (Vec3){w * .43f, roof + .25f, back}, (Vec3){w * .46f, .5f, -l * .48f},
                    edge, 1.5f, false);
        }
    }
    if (model == 13) {
        carquad(r, v, (Vec3){-w * .39f, 1.0f, -l * .46f}, (Vec3){w * .39f, 1.0f, -l * .46f},
                (Vec3){w * .39f, 1.0f, -l * .08f}, (Vec3){-w * .39f, 1.0f, -l * .08f}, dark);
    }
    if (model == 4 || model == 11 || model == 14 || model == 17 || model == 18) {
        int panes = model == 18 ? 5 : 3;
        for (int i = 0; i < panes; i++)
            for (int side = -1; side <= 1; side += 2) {
                float x = side * w * .421f, z = back + .5f + i * (front - back - .8f) / panes;
                carline(r, v, (Vec3){x, 1.03f, z}, (Vec3){x * .86f, roof, z + .15f},
                        scale(edge, .8f), 1, false);
            }
    }
    float radius = model == 7 ? .62f : model == 9 ? .47f : .38f;
    for (int i = 0; i < 4; i++)
        wheel(r, v, (i & 1 ? 1 : -1) * w * .48f, (i & 2 ? 1 : -1) * l * .32f, radius,
              model == 9 && style.wheel == 0 ? 3 : style.wheel, edge, (i & 2) != 0);
    for (int side = -1; side <= 1; side += 2) {
        carline(r, v, (Vec3){side * w * .34f, .7f, l * .505f},
                (Vec3){side * w * .15f, .7f, l * .505f}, (Color){.82f, .9f, 1, 1}, 2, true);
        carline(r, v, (Vec3){side * w * .35f, .73f, -l * .505f},
                (Vec3){side * w * .15f, .73f, -l * .505f},
                (Color){v.pitch > .01f ? 1 : .6f, .06f, .09f, 1}, 2, true);
        carpart(r, v, (Vec3){side * w * .55f, 1.14f, l * .10f}, (Vec3){.32f, .18f, .3f}, fill, edge,
                false);
    }
    if (model == 2 || model == 3 || model == 6 || model == 16)
        carpart(r, v, (Vec3){0, 1.12f, -l * .43f}, (Vec3){w * 1.07f, .12f, .4f}, fill, edge, false);
    if (model == 10 && !cop)
        carpart(r, v, (Vec3){0, roof + .22f, 0}, (Vec3){1.2f, .35f, .65f},
                (Color){.3f, .2f, .03f, 1}, PAINTS[2], true);
    if (model == 14) {
        carpart(r, v, (Vec3){0, roof + .35f, -.3f}, (Vec3){.8f, .7f, .8f},
                (Color){.3f, .16f, .04f, 1}, PAINTS[2], false);
        carpart(r, v, (Vec3){0, roof + .9f, -.3f}, (Vec3){1.05f, .65f, 1.05f},
                (Color){.3f, .17f, .22f, 1}, PAINTS[1], false);
    }
    if (model == 16)
        for (int i = 0; i < 4; i++)
            carpart(r, v, (Vec3){(i - 1.5f) * .4f, .85f, l * .52f}, (Vec3){.25f, .25f, .1f},
                    (Color){.3f, .29f, .22f, 1}, PAINTS[5], true);
    if (model == 19)
        for (int i = 0; i < 4; i++)
            carline(r, v, (Vec3){-w * .32f, 1.02f, -l * .45f + i * .16f},
                    (Vec3){w * .32f, 1.02f, -l * .45f + i * .16f}, edge, .8f, false);
    if (cop) {
        Color flash =
            ((int)(g->time * 8) & 1) ? (Color){1, .05f, .08f, 1} : (Color){.1f, .25f, 1, 1};
        carpart(r, v, (Vec3){0, roof + .17f, 0}, (Vec3){w * .7f, .24f, .4f}, scale(flash, .5f),
                flash, true);
        carpart(r, v, (Vec3){0, .65f, l * .55f}, (Vec3){w * .7f, .5f, .18f}, dark, edge, false);
        for (int side = -1; side <= 1; side += 2)
            carline(r, v, (Vec3){side * w * .51f, .72f, -l * .31f},
                    (Vec3){side * w * .51f, .72f, l * .31f}, cop == SWAT ? PAINTS[0] : PAINTS[7],
                    1.3f, false);
        if (cop == SWAT) {
            carpart(r, v, (Vec3){0, roof + .15f, -l * .22f}, (Vec3){w * .8f, .15f, l * .35f}, dark,
                    edge, false);
            for (int side = -1; side <= 1; side += 2)
                carline(r, v, (Vec3){side * w * .43f, 1.3f, front},
                        (Vec3){side * w * .43f, roof - .1f, front}, edge, 2, false);
        }
        return;
    }
accessories:
    if (model < 20)
        decals(r, v, w, l, style.decal);
    if (style.engine == 5 || model == 12) {
        carpart(r, v, (Vec3){0, 1.22f, l * .30f}, (Vec3){.75f, .45f, .75f},
                (Color){.12f, .15f, .18f, 1}, PAINTS[5], false);
        carpart(r, v, (Vec3){0, 1.49f, l * .32f}, (Vec3){.85f, .16f, .4f}, dark, edge, false);
    } else if (style.engine == 3) {
        carpart(r, v, (Vec3){0, 1.08f, l * .32f}, (Vec3){.7f, .15f, .65f}, dark, edge, false);
    } else if (style.engine == 4) {
        carline(r, v, (Vec3){-.4f, 1.005f, l * .28f}, (Vec3){.4f, 1.005f, l * .40f}, PAINTS[0],
                1.4f, true);
    }
    if (antenna) {
        float ah = antenna == 1 ? 2 : antenna == 2 ? 1.8f : 2.2f;
        for (int i = 0; i < (antenna == 2 ? 2 : 1); i++) {
            float ax = i ? -.65f : .65f;
            Vec3 tip = {ax + sinf(g->time * 8) * .13f, roof + ah, -l * .25f};
            carline(r, v, (Vec3){ax, roof, -l * .25f}, tip, edge, 1, true);
            if (antenna == 3) {
                for (int q = 0; q < 5; q++) {
                    float a = q * 2 * PI / 5;
                    carline(r, v, tip, (Vec3){tip.x + sinf(a) * .5f, tip.y + cosf(a) * .5f, tip.z},
                            edge, 1, true);
                }
            } else if (antenna == 4) {
                carpart(r, v, tip, (Vec3){.9f, .2f, .75f}, fill, edge, true);
            } else if (antenna == 5) {
                carline(r, v, (Vec3){tip.x - .45f, tip.y + .25f, tip.z},
                        (Vec3){tip.x, tip.y - .25f, tip.z}, PAINTS[1], 2, true);
                carline(r, v, (Vec3){tip.x, tip.y - .25f, tip.z},
                        (Vec3){tip.x + .45f, tip.y + .25f, tip.z}, PAINTS[1], 2, true);
            } else if (antenna == 6) {
                carpart(r, v, (Vec3){tip.x + .3f, tip.y - .15f, tip.z}, (Vec3){.6f, .45f, .08f},
                        fill, PAINTS[2], true);
            } else if (antenna == 7) {
                for (int q = 0; q < 8; q++) {
                    float a = q * PI / 4, b = (q + 1) * PI / 4;
                    carline(r, v, (Vec3){tip.x + cosf(a) * .5f, tip.y + sinf(a) * .5f, tip.z},
                            (Vec3){tip.x + cosf(b) * .5f, tip.y + sinf(b) * .5f, tip.z}, edge, 1,
                            true);
                }
            }
        }
    }
    switch (hat) {
    case 1:
        carpart(r, v, (Vec3){0, roof + .3f, 0}, (Vec3){1.3f, .5f, .6f}, (Color){.2f, .14f, .02f, 1},
                PAINTS[2], true);
        break;
    case 2:
        carpart(r, v, (Vec3){0, roof + .1f, 0}, (Vec3){1.8f, .2f, 1.5f}, dark, edge, true);
        carpart(r, v, (Vec3){0, roof + .7f, 0}, (Vec3){1.1f, 1.1f, 1}, dark, edge, true);
        break;
    case 3:
        carpart(r, v, (Vec3){0, roof + .18f, 0}, (Vec3){.7f, .2f, l * 1.1f}, fill, PAINTS[2], true);
        break;
    case 4:
        for (int i = 0; i < 4; i++) {
            float a = i * PI / 2, b = (i + 1) * PI / 2;
            Vec3 p = {cosf(a) * .7f, roof, sinf(a) * .7f}, q = {cosf(b) * .7f, roof, sinf(b) * .7f};
            carline(r, v, p, q, PAINTS[1], 1, true);
            carline(r, v, p, (Vec3){0, roof + 1.5f, 0}, PAINTS[1], 1, true);
        }
        break;
    case 5:
        carpart(r, v, (Vec3){0, roof + .2f, 0}, (Vec3){1.5f, .35f, 1.2f}, fill, PAINTS[2], true);
        for (int i = -1; i <= 1; i++)
            carpart(r, v, (Vec3){i * .55f, roof + .6f, -.4f}, (Vec3){.15f, .5f, .2f}, fill,
                    PAINTS[2], true);
        break;
    case 6:
        carpart(r, v, (Vec3){0, roof + .25f, 0}, (Vec3){2, .2f, 1.8f}, fill, edge, true);
        carpart(r, v, (Vec3){0, roof + .5f, 0}, (Vec3){.9f, .4f, .8f}, glass, PAINTS[3], true);
        break;
    case 7:
        carpart(r, v, (Vec3){0, roof + .55f, 0}, (Vec3){.8f, 1.1f, .8f},
                (Color){.12f, .13f, .17f, 1}, PAINTS[5], true);
        carpart(r, v, (Vec3){0, roof + 1.2f, 0}, (Vec3){.8f, .2f, .22f}, fill, PAINTS[6], true);
        break;
    }
}
static void helicopter(Render *r, Game *g, const Helicopter *h) {
    if (!visible(r, h->p, 18))
        return;
    Color edge = h == &g->helper ? PAINTS[3] : (Color){.7f, .85f, 1, 1},
          fill = {.045f, .065f, .09f, 1};
    Vec3 p = h->p;
    float sn = sinf(h->yaw), cs = cosf(h->yaw);
    r_box(r, p, (Vec3){2.7f, 2, 5.5f}, h->yaw, fill, edge, false);
    r_box(r, local((Vec3){0, .1f, 2}, p, sn, cs), (Vec3){2.35f, 1.4f, 1.7f}, h->yaw,
          (Color){.05f, .14f, .19f, 1}, edge, false);
    Vec3 tail = local((Vec3){0, .25f, -6}, p, sn, cs);
    r_line(r, local((Vec3){0, 0, -2}, p, sn, cs), tail, edge, 2, false);
    r_line(r, tail, (Vec3){tail.x, tail.y + 2, tail.z}, edge, 2, false);
    for (int i = 0; i < 2; i++) {
        float a = h->rotor + i * PI * .5f;
        r_line(r, (Vec3){p.x + sinf(a) * 7, p.y + 1.7f, p.z + cosf(a) * 7},
               (Vec3){p.x - sinf(a) * 7, p.y + 1.7f, p.z - cosf(a) * 7}, scale(edge, .65f), 1.5f,
               false);
    }
    for (int side = -1; side <= 1; side += 2) {
        Vec3 a = local((Vec3){side * 1.7f, -1.7f, -2.5f}, p, sn, cs),
             b = local((Vec3){side * 1.7f, -1.7f, 2.5f}, p, sn, cs);
        r_line(r, a, b, edge, 1.2f, false);
        r_line(r, a, local((Vec3){side * .9f, -.6f, -1}, p, sn, cs), edge, 1, false);
    }
    if (h->spot > 0) {
        Vec3 target = {g->player.x, 0.05f, g->player.z};
        for (int i = 0; i < 12; i++) {
            float a = i * PI / 6, b = (i + 1) * PI / 6;
            r_line(r, (Vec3){target.x + sinf(a) * 6, target.y, target.z + cosf(a) * 6},
                   (Vec3){target.x + sinf(b) * 6, target.y, target.z + cosf(b) * 6},
                   (Color){.6f, .57f, .3f, 1}, 1.1f, true);
        }
        if (r->settings->detail == 2)
            for (int i = -1; i <= 1; i += 2)
                r_line(r, (Vec3){p.x, p.y - 1, p.z}, (Vec3){target.x + i * 6, .1f, target.z},
                       (Color){.14f, .15f, .09f, 1}, .7f, false);
    }
}
#include "atmosphere.inc"
void render_top(Render *r, Game *g, float dt) {
    render_reset(r, 400, 240);
    r->visual = g->profile.settings;
    r->lod_hold = fmaxf(0, r->lod_hold - dt);
    if (!r->visual.auto_lod)
        r->lod_hold = 0;
    else if (g->screen == PLAY && g->measured_fps > 0 &&
             g->measured_fps < (g->profile.settings.fps == 60 ? 46 : 24))
        r->lod_hold = 3.5f;
    if (r->lod_hold > 0 && g->screen == PLAY) {
        r->visual.detail = 0;
        if (r->visual.view > 2)
            r->visual.view--;
        if (r->visual.bloom > 1)
            r->visual.bloom = 1;
    }
    r->settings = &r->visual;
    r->fog = THEMES[preview_theme(g)].sky;
    r->far_clip = CELL * (r->settings->view + .3f);
    bool garage = g->screen == SHOP || g->screen == GARAGE;
    bool attract = g->screen == TITLE || g->screen == HELP ||
                   (g->screen == SETTINGS && g->return_screen == TITLE);
    Vehicle v = g->player;
    if (garage || attract) {
        Vec3 center = world_center(&g->world, g->world.spawn);
        v.x = center.x;
        v.z = center.z;
        v.vx = v.vz = 0;
    }
    r->focus = (Vec3){v.x, 0, v.z};
    if (garage || attract) {
        float yaw = garage ? sinf(g->time * .18f) * .7f + .6f : sinf(g->time * .13f) * .3f + .25f;
        yaw += g->preview_yaw;
        float distance =
            garage
                ? fmaxf(6, CARS[preview_car(g)].length * 1.65f + CARS[preview_car(g)].height * .5f)
                : 16;
        v.yaw = .2f;
        r->eye = (Vec3){v.x + sinf(yaw) * distance,
                        garage ? fmaxf(3, CARS[preview_car(g)].height * 1.1f) : 6.5f,
                        v.z - cosf(yaw) * distance};
        r->forward = norm(
            sub((Vec3){v.x, garage ? .7f + CARS[preview_car(g)].height * .45f : 1, v.z}, r->eye));
    } else {
        float speed = vehicle_speed(&g->player), targetyaw = g->player.yaw;
        float factor = clampf(dt * 5, 0, 1);
        g->camera_yaw += angle_delta(targetyaw, g->camera_yaw) * factor;
        float distance = 11 + g->profile.settings.camera * 2 + speed * .045f;
        float height =
            4 + g->profile.settings.camera * 2 + (g->high_camera ? 9 : 0) + (g->giant > 0 ? 2 : 0);
        float tx = v.x - sinf(g->camera_yaw) * distance, tz = v.z - cosf(g->camera_yaw) * distance;
        g->camera_x += (tx - g->camera_x) * clampf(dt * 9, 0, 1);
        g->camera_z += (tz - g->camera_z) * clampf(dt * 9, 0, 1);
        g->camera_height += (height - g->camera_height) * factor;
        r->eye = (Vec3){g->camera_x, g->camera_height, g->camera_z};

        Vec3 target = {v.x + sinf(v.yaw) * 12, 1.3f, v.z + cosf(v.yaw) * 12};
        for (int i = 1; i <= 8; i++) {
            float t = i / 8.0f;
            float x = v.x + (r->eye.x - v.x) * t, z = v.z + (r->eye.z - v.z) * t;
            if (world_solid(&g->world, x, z, .4f)) {
                r->eye.x = v.x + (r->eye.x - v.x) * fmaxf(.15f, t - .13f);
                r->eye.z = v.z + (r->eye.z - v.z) * fmaxf(.15f, t - .13f);
                r->eye.y = fmaxf(r->eye.y, 10);
                break;
            }
        }
        if (g->profile.settings.shake && g->shake > 0) {
            r->eye.x += sinf(g->time * 73) * g->shake * .3f;
            r->eye.y += cosf(g->time * 67) * g->shake * .3f;
        }
        r->forward = norm(sub(target, r->eye));
    }
    r->right = norm(cross((Vec3){0, 1, 0}, r->forward));
    r->up = cross(r->forward, r->right);
    float fov =
        57 + (g->profile.settings.fov && !garage ? clampf(vehicle_speed(&v) * .15f, 0, 6) : 0);
    r->focal = 1 / tanf(fov * PI / 360);
    sky(r, g);

    if (!garage && !attract) {
        for (int i = 0; i < MAX_ACTORS; i++)
            if (g->actors[i].active &&
                visible(r, (Vec3){g->actors[i].v.x, 1, g->actors[i].v.z}, 5)) {
                Actor *a = &g->actors[i];
                r_car(r, g, a->v, a->model,
                      (CarStyle){.paint = a->kind ? a->variant : i % PAINT_COUNT}, a->kind);
                if (a->commit > 1.15f && ((int)(g->time * 8) & 1))
                    r_line(r, (Vec3){a->v.x, .08f, a->v.z}, (Vec3){a->charge_x, .08f, a->charge_z},
                           PAINTS[2], 1.5f, true);
            }
        for (int i = 0; i < 25; i++) {
            Pickup *p = &g->pickups[i];
            if (!p->active || !visible(r, (Vec3){p->px, 1, p->pz}, 2))
                continue;
            float y = 1.7f + sinf(g->time * 3 + i) * .3f;
            Color c = PAINTS[p->type == 1 ? 3 : p->type == 2 ? 0 : 2];
            r_box(r, (Vec3){p->px, y, p->pz}, (Vec3){1.2f, 1.2f, 1.2f}, g->time, scale(c, .18f), c,
                  true);
            r_line(r, (Vec3){p->px, .1f, p->pz}, (Vec3){p->px, y - .6f, p->pz}, scale(c, .5f), 1,
                   true);
        }
        for (int i = 0; i < MAX_PARTICLES; i++) {
            Particle *p = &g->particles[i];
            if (p->life <= 0)
                continue;
            r_line(
                r, p->p,
                (Vec3){p->p.x - p->v.x * .055f, p->p.y - p->v.y * .055f, p->p.z - p->v.z * .055f},
                scale(p->color, p->life / p->maxlife), 1.5f, true);
        }
    }
    r_car(r, g, v, preview_car(g), preview_style(g), 0);
    city(r, g);
    if (!garage && !attract)
        for (int i = 0; i < MAX_HELIS; i++)
            if (g->helis[i].active)
                helicopter(r, g, &g->helis[i]);
    if (!garage && !attract) {
        people_draw(r, g);
        supplies_draw(r, g);
        rain_draw(r, g);
        if (g->helper.active)
            helicopter(r, g, &g->helper);
    }
    render_hud(r, g);
}

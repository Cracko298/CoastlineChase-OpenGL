
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static float plane(Vertex v, int p) {
    switch (p) {
    case 0:
        return v.x + v.w;
    case 1:
        return v.w - v.x;
    case 2:
        return v.y + v.w;
    case 3:
        return v.w - v.y;
    case 4:
        return v.z + v.w;
    default:
        return -v.z;
    }
}
static Vertex lerp(Vertex a, Vertex b, float t) {
    return (Vertex){a.x + (b.x - a.x) * t,
                    a.y + (b.y - a.y) * t,
                    a.z + (b.z - a.z) * t,
                    a.w + (b.w - a.w) * t,
                    {a.c.r + (b.c.r - a.c.r) * t, a.c.g + (b.c.g - a.c.g) * t,
                     a.c.b + (b.c.b - a.c.b) * t, a.c.a + (b.c.a - a.c.a) * t}};
}
static float edge(float ax, float ay, float bx, float by, float x, float y) {
    return (x - ax) * (by - ay) - (y - ay) * (bx - ax);
}
static void raster(Vertex a, Vertex b, Vertex c, int pass, int w, int h, float *pixels,
                   float *depth) {
    if (a.w <= 0 || b.w <= 0 || c.w <= 0)
        return;
    float ax = (a.x / a.w + 1) * w * .5f, ay = (1 - a.y / a.w) * h * .5f,
          bx = (b.x / b.w + 1) * w * .5f, by = (1 - b.y / b.w) * h * .5f,
          cx = (c.x / c.w + 1) * w * .5f, cy = (1 - c.y / c.w) * h * .5f;
    float area = edge(ax, ay, bx, by, cx, cy);
    if (fabsf(area) < .00001f)
        return;
    int x0 = (int)clampf(floorf(fminf(ax, fminf(bx, cx))), 0, w - 1),
        x1 = (int)clampf(ceilf(fmaxf(ax, fmaxf(bx, cx))), 0, w - 1),
        y0 = (int)clampf(floorf(fminf(ay, fminf(by, cy))), 0, h - 1),
        y1 = (int)clampf(ceilf(fmaxf(ay, fmaxf(by, cy))), 0, h - 1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float u = edge(bx, by, cx, cy, x + .5f, y + .5f) / area,
                  v = edge(cx, cy, ax, ay, x + .5f, y + .5f) / area, t = 1 - u - v;
            if (u < 0 || v < 0 || t < 0)
                continue;
            float z = -(u * a.z / a.w + v * b.z / b.w + t * c.z / c.w);
            int i = y * w + x;
            if (pass != PASS_UI && z + 1e-6f < depth[i])
                continue;
            float inv = u / a.w + v / b.w + t / c.w;
            Color col = {(u * a.c.r / a.w + v * b.c.r / b.w + t * c.c.r / c.w) / inv,
                         (u * a.c.g / a.w + v * b.c.g / b.w + t * c.c.g / c.w) / inv,
                         (u * a.c.b / a.w + v * b.c.b / b.w + t * c.c.b / c.w) / inv,
                         (u * a.c.a / a.w + v * b.c.a / b.w + t * c.c.a / c.w) / inv};
            float rgb[] = {col.r, col.g, col.b};
            for (int k = 0; k < 3; k++)
                pixels[i * 3 + k] = pass == PASS_GLOW
                                        ? pixels[i * 3 + k] + rgb[k] * col.a
                                        : pixels[i * 3 + k] * (1 - col.a) + rgb[k] * col.a;
            if (pass == PASS_SOLID)
                depth[i] = z;
        }
}
bool render_ppm(const Render *r, const char *path, Color clear) {
    int w = r->width, h = r->height;
    float *pix = calloc(w * h * 3, sizeof(float)), *depth = calloc(w * h, sizeof(float));
    if (!pix || !depth) {
        free(pix);
        free(depth);
        return false;
    }
    for (int i = 0; i < w * h; i++) {
        pix[3 * i] = clear.r;
        pix[3 * i + 1] = clear.g;
        pix[3 * i + 2] = clear.b;
    }
    for (int p = 0; p < PASS_COUNT; p++) {
        const Mesh *m = &r->mesh[p];
        for (unsigned k = 0; k < m->count; k += 3) {
            Vertex poly[16], tmp[16];
            int n = 3;
            poly[0] = m->v[k];
            poly[1] = m->v[k + 1];
            poly[2] = m->v[k + 2];
            for (int planeid = 0; planeid < 6 && n; planeid++) {
                int nn = 0;
                for (int j = 0; j < n; j++) {
                    Vertex a = poly[j], b = poly[(j + 1) % n];
                    float da = plane(a, planeid), db = plane(b, planeid);
                    if (da >= 0)
                        tmp[nn++] = a;
                    if ((da >= 0) != (db >= 0))
                        tmp[nn++] = lerp(a, b, da / (da - db));
                }
                n = nn;
                for (int j = 0; j < n; j++)
                    poly[j] = tmp[j];
            }
            for (int j = 1; j < n - 1; j++)
                raster(poly[0], poly[j], poly[j + 1], p, w, h, pix, depth);
        }
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(pix);
        free(depth);
        return false;
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h * 3; i++)
        fputc((int)(clampf(pix[i], 0, 1) * 255 + .5f), f);
    bool ok = !ferror(f);
    fclose(f);
    free(pix);
    free(depth);
    return ok;
}

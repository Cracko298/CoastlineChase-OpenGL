/* Procedural landmark geometry; no external models or textures. */
static void light_pool(Render *r, Vec3 center, float radius, Color color) {
    if (!r->settings->lighting)
        return;
    center.y = .065f;
    Vertex c = project(r, center, color);
    c.c.a = .22f * (.6f + r->night * .4f);
    for (int i = 0; i < 8; i++) {
        float a = i * PI / 4, b = (i + 1) * PI / 4;
        Vertex p = project(
            r, (Vec3){center.x + sinf(a) * radius, center.y, center.z + cosf(a) * radius}, color);
        Vertex q = project(
            r, (Vec3){center.x + sinf(b) * radius, center.y, center.z + cosf(b) * radius}, color);
        p.c.a = q.c.a = 0;
        if (!reject(c, p, q))
            tri(r, PASS_GLOW, c, p, q);
    }
}
static void circle_line(Render *r, Vec3 c, float radius, Color col, bool vertical, bool glow) {
    for (int i = 0; i < 16; i++) {
        float a = i * PI / 8, b = (i + 1) * PI / 8;
        Vec3 p = {c.x + sinf(a) * radius, c.y + (vertical ? cosf(a) * radius : 0),
                  c.z + (vertical ? 0 : cosf(a) * radius)};
        Vec3 q = {c.x + sinf(b) * radius, c.y + (vertical ? cosf(b) * radius : 0),
                  c.z + (vertical ? 0 : cosf(b) * radius)};
        r_line(r, p, q, col, 1, glow);
    }
}
static void window_spill(Render *r, Vec3 center, bool alongx, float width, Color col) {
    Vertex c = project(r, center, col);
    c.c.a = .18f;
    for (int i = 0; i < 8; i++) {
        float a = i * PI / 4, b = (i + 1) * PI / 4;
        Vec3 p = center, q = center;
        p.y += sinf(a) * 1.8f;
        q.y += sinf(b) * 1.8f;
        if (alongx) {
            p.x += cosf(a) * (width + 1);
            q.x += cosf(b) * (width + 1);
        } else {
            p.z += cosf(a) * (width + 1);
            q.z += cosf(b) * (width + 1);
        }
        Vertex aa = project(r, p, col), bb = project(r, q, col);
        aa.c.a = bb.c.a = 0;
        if (!reject(c, aa, bb))
            tri(r, PASS_GLOW, c, aa, bb);
    }
}
static void neon_sign(Render *r, Vec3 p, float width, float height, const char *text, Color col) {
    r_box(r, p, (Vec3){width, height, .16f}, 0, (Color){.025f, .02f, .045f, 1}, col, true);
    if (hypotf(p.x - r->eye.x, p.z - r->eye.z) > 70)
        return;
    float unit = fminf((width - .7f) / (strlen(text) * 6), (height - .35f) / 7);
    float left = p.x - strlen(text) * 3 * unit, z = p.z + (r->eye.z > p.z ? .10f : -.10f);
    /* Original UI glyphs, horizontal pixel runs, solid emissive letters.
       One glowing frame avoids per-pixel bloom expansion. */
    for (int i = 0; text[i]; i++)
        for (int row = 0; row < 7; row++) {
            unsigned bits = coast_glyph(text[i], row);
            int run = -1;
            for (int x = 0; x <= 5; x++) {
                bool on = x < 5 && (bits & (1u << (4 - x)));
                if (on && run < 0)
                    run = x;
                if (!on && run >= 0) {
                    float a = left + (i * 6 + run) * unit, b = left + (i * 6 + x) * unit;
                    if (r->eye.z > p.z) {
                        a = 2 * p.x - a;
                        b = 2 * p.x - b;
                    }
                    float y = p.y + (3.5f - row) * unit;
                    r_quad(r, (Vec3){a, y, z}, (Vec3){b, y, z}, (Vec3){b, y - unit, z},
                           (Vec3){a, y - unit, z}, col);
                    run = -1;
                }
            }
        }
}
static void landmark(Render *r, Game *g, const Chunk *c, const Theme *t) {
    if (!c->landmark || !c->count)
        return;
    const Building *b = &c->b[0];
    float x = b->x + b->w * .5f, z = b->z + b->d * .5f, h = b->h;
    float front = b->z + b->d;
    if (!visible(r, (Vec3){x, h * .5f, z}, h * .5f + 12))
        return;
    Color neon = t->accent, cyan = t->outline, metal = {.10f, .12f, .15f, 1};
    if (c->landmark >= 13) {
        static const char *labels[] = {"NEON REPAIR", "PALM MOTEL", "VINYL 33",   "MARKET",
                                       "FIRE RESCUE", "HOSPITAL",   "VIDEO CLUB", "LAUNDRY"};
        neon_sign(r, (Vec3){x, h + 1.2f, front}, b->w, 2, labels[c->landmark - 13], neon);
        if (c->landmark == 13 || c->landmark == 17) {
            for (int i = -1; i <= 1; i += 2) {
                float xx = x + i * b->w * .25f;
                r_box(r, (Vec3){xx, 1.7f, front + .05f}, (Vec3){b->w * .38f, 3.4f, .15f}, 0,
                      (Color){.06f, .08f, .09f, 1}, cyan, true);
                for (int row = 1; row < 6; row++)
                    r_line(r, (Vec3){xx - b->w * .17f, row * .5f, front + .15f},
                           (Vec3){xx + b->w * .17f, row * .5f, front + .15f}, scale(cyan, .5f), .8f,
                           false);
            }
            r_box(r, (Vec3){x, 1.2f, front + 1.5f}, (Vec3){1.5f, 2.4f, .7f}, 0, metal, neon, false);
        } else if (c->landmark == 18) {
            r_box(r, (Vec3){x, h + 4, front}, (Vec3){4, .8f, .4f}, 0, PAINTS[6], PAINTS[6], true);
            r_box(r, (Vec3){x, h + 4, front}, (Vec3){.8f, 4, .4f}, 0, PAINTS[6], PAINTS[6], true);
        } else {
            r_box(r, (Vec3){x, 3, front + 1}, (Vec3){b->w, .3f, 2}, 0, scale(neon, .2f), neon,
                  true);
            for (int i = 0; i < 3; i++)
                r_box(r, (Vec3){b->x + 2 + i * (b->w - 4) / 2, 1.3f, front + .12f},
                      (Vec3){2, 2, .15f}, 0, (Color){.08f, .17f, .2f, 1}, cyan, true);
        }
        light_pool(r, (Vec3){x, 0, front + 2}, 7, neon);
    } else if (c->landmark == 1 || c->landmark == 2 || c->landmark == 3 || c->landmark == 10 ||
               c->landmark == 11) {
        Color awning = c->landmark == 10 ? (Color){.6f, .25f, .1f, 1} : neon;
        r_box(r, (Vec3){x, 3.3f, front + 1}, (Vec3){b->w + 1, .6f, 3}, 0, scale(awning, .25f),
              awning, true);
        for (int i = 0; i < 4; i++) {
            float xx = b->x + (i + .5f) * b->w / 4;
            r_line(r, (Vec3){xx, 3.7f, front + 2.6f}, (Vec3){xx, 3.7f, front - .4f}, cyan, .9f,
                   true);
        }
        if (c->landmark == 1) {
            r_line(r, (Vec3){b->x, .5f, front + .1f}, (Vec3){b->x + b->w, .5f, front + .1f}, cyan,
                   1.5f, true);
            r_box(r, (Vec3){x, h + 1, z}, (Vec3){8, 1.7f, 1}, 0, metal, neon, true);
        } else if (c->landmark == 2 || c->landmark == 3) {
            r_box(r, (Vec3){x, h + 2, front - .2f}, (Vec3){b->w * .8f, 4, .5f}, 0, metal, cyan,
                  true);
            for (int i = -1; i <= 1; i++) {
                Vec3 p = {x + i * 2, h + 2, front + .1f};
                r_line(r, (Vec3){p.x - 1, p.y, p.z}, (Vec3){p.x + 1, p.y, p.z}, neon, 1.6f, true);
                r_line(r, (Vec3){p.x, p.y - 1, p.z}, (Vec3){p.x, p.y + 1, p.z}, neon, 1.6f, true);
            }
        } else if (c->landmark == 10) {
            for (int i = 0; i < 5; i++)
                r_box(r, (Vec3){b->x + 1 + i * 2, 1, front + 1.5f}, (Vec3){1.3f, 2, .8f}, 0,
                      scale(PAINTS[i], .2f), PAINTS[i], false);
        } else {
            for (int i = -1; i <= 1; i += 2)
                r_box(r, (Vec3){x + i * 3, 1.2f, front + 1.5f}, (Vec3){1.3f, 2.4f, 1}, 0, metal,
                      cyan, true);
        }
        static const char *labels[] = {"", "DINER", "ARCADE", "CINEMA"};
        neon_sign(r, (Vec3){x, h + 1, front + .2f}, b->w, 1.8f,
                  c->landmark <= 3    ? labels[c->landmark]
                  : c->landmark == 11 ? "NEON FUEL"
                                      : "NIGHT MARKET",
                  neon);
        if (c->landmark == 11) {
            r_box(r, (Vec3){x, 4.4f, front + 2}, (Vec3){b->w + 1, .5f, 4}, 0, metal, neon, true);
            for (int i = -1; i <= 1; i += 2)
                r_box(r, (Vec3){x + i * (b->w * .45f), 2.2f, front + 2}, (Vec3){.2f, 4.4f, .2f}, 0,
                      metal, cyan, false);
        }
    } else if (c->landmark == 4) {
        Vec3 clock = {x, h - 3, front + .07f};
        circle_line(r, clock, 2.3f, cyan, true, true);
        float a = g->time * .02f;
        r_line(r, clock, (Vec3){x + sinf(a) * 1.9f, h - 3 + cosf(a) * 1.9f, clock.z}, neon, 1.6f,
               true);
        r_line(r, clock, (Vec3){x - 1.2f, h - 2.4f, clock.z}, neon, 1.6f, true);
        pyramid(r, (Vec3){x, h, z}, b->w * .65f, 5, metal, cyan);
    } else if (c->landmark == 5) {
        for (int i = 1; i < 5; i++)
            r_box(r, (Vec3){x, i * h / 5, z}, (Vec3){b->w + .1f, .7f, b->d + .1f}, 0, metal,
                  i & 1 ? neon : cyan, true);
        r_box(r, (Vec3){x, h + 1.3f, z}, (Vec3){6, 2.6f, 6}, 0, scale(cyan, .25f), cyan, true);
        pyramid(r, (Vec3){x, h + 2.7f, z}, 4, 3, metal, neon);
        float a = g->time * .3f;
        r_line(r, (Vec3){x, h + 1, z}, (Vec3){x + sinf(a) * 42, h - 2, z + cosf(a) * 42},
               scale(cyan, .5f), 2, true);
    } else if (c->landmark == 6 || c->landmark == 12) {
        pyramid(r, (Vec3){x, h, z}, b->w * .7f, c->landmark == 6 ? 7 : 5, scale(cyan, .12f), cyan);
        if (c->landmark == 6) {
            for (int i = -1; i <= 1; i += 2)
                tree(r, x + i * 4, front + 2, 5, 2, 1, cyan);
        } else {
            r_line(r, (Vec3){x, h + 3, z}, (Vec3){x + 7, h + 8, z + 3}, neon, 5, true);
            r_box(r, (Vec3){x + 7, h + 8, z + 3}, (Vec3){2, 2, 2}, .4f, metal, cyan, true);
        }
    } else if (c->landmark == 7) {
        Vec3 center = {x, h + 10, z};
        circle_line(r, center, 8.5f, neon, true, true);
        for (int i = 0; i < 8; i++) {
            float a = i * PI / 4 + g->time * .12f;
            Vec3 p = {x + sinf(a) * 8.5f, h + 10 + cosf(a) * 8.5f, z};
            r_line(r, center, p, scale(cyan, .65f), .8f, false);
            r_box(r, (Vec3){p.x, p.y - .7f, p.z}, (Vec3){1.8f, 1.4f, 1.8f}, 0,
                  scale(PAINTS[i], .2f), PAINTS[i], true);
        }
        r_line(r, (Vec3){x - 5, h, z + 2}, center, cyan, 2, false);
        r_line(r, (Vec3){x + 5, h, z + 2}, center, cyan, 2, false);
    } else if (c->landmark == 8) {
        r_box(r, (Vec3){x, h + 10, z}, (Vec3){1.5f, 20, 1.5f}, 0, metal, neon, false);
        r_box(r, (Vec3){x + 5, h + 20, z}, (Vec3){25, 1.2f, 1.2f}, 0, metal, neon, true);
        r_line(r, (Vec3){x + 15, h + 20, z}, (Vec3){x + 15, h + 8 + 2 * sinf(g->time * .3f), z},
               cyan, 1.1f, false);
    } else if (c->landmark == 9) {
        for (int i = 0; i < 3; i++)
            r_box(r, (Vec3){x, h + i * 3, z}, (Vec3){b->w - i * 3, 4, b->d - i * 3}, 0, metal, neon,
                  true);
        for (int i = 1; i < 8; i++)
            r_line(r, (Vec3){b->x - .05f, i * 5, front + .05f},
                   (Vec3){b->x + b->w + .05f, i * 5, front + .05f}, cyan, 1, true);
    }
    if (hypotf(x - r->eye.x, z - r->eye.z) < 85)
        light_pool(r, (Vec3){x, 0, front + 3}, 7, neon);
}

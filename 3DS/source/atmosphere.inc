static Color mix(Color a, Color b, float t) {
    return (Color){a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1};
}
static void sky(Render *r, Game *g) {
    float clock = g->run_time + (g->seed % 440), phase = fmodf(clock, 110),
          blend = clampf((phase - 85) / 25, 0, 1);
    int stage = (int)(clock / 110) % 4, next = (stage + 1) % 4;
    Color tops[] = {{.015f, .025f, .07f, 1},
                    {.025f, .012f, .055f, 1},
                    {.13f, .04f, .13f, 1},
                    {.028f, .044f, .062f, 1}};
    Color horizons[] = {{.13f, .25f, .36f, 1},
                        {.055f, .07f, .16f, 1},
                        {.58f, .22f, .17f, 1},
                        {.12f, .17f, .20f, 1}};
    float nights[] = {.55f, 1, .12f, .65f}, rains[] = {0, 0, 0, 1}, days[] = {.7f, .05f, .42f, .3f};
    r->night = nights[stage] + (nights[next] - nights[stage]) * blend;
    r->rain = rains[stage] + (rains[next] - rains[stage]) * blend;
    r->daylight = days[stage] + (days[next] - days[stage]) * blend;
    Color top = mix(mix(tops[stage], tops[next], blend), THEMES[preview_theme(g)].sky, .25f);
    Color bottom = mix(horizons[stage], horizons[next], blend);
    r->fog = top;
    Vertex a = {-1, 1, 0, 1, top}, b = {1, 1, 0, 1, top}, c = {1, -1, 0, 1, bottom},
           d = {-1, -1, 0, 1, bottom};
    tri(r, PASS_SOLID, a, b, c);
    tri(r, PASS_SOLID, a, c, d);
    float radius = r->far_clip * .83f;
    if (r->settings->weather && r->night > .3f && r->rain < .8f) {
        int count = r->settings->detail == 2 ? 84 : 42;
        for (int i = 0; i < count; i++) {
            uint32_t h = hash_cell(i, 49, g->seed);
            float az = (h % 6283) * .001f, el = .045f + ((h >> 13) % (i < 42 ? 300 : 850)) * .001f;
            Vec3 p = {r->eye.x + sinf(az) * cosf(el) * radius, r->eye.y + sinf(el) * radius,
                      r->eye.z + cosf(az) * cosf(el) * radius};
            float twinkle = .6f + .3f * sinf(g->time * 1.4f + i * 2);
            r_line(r, p, (Vec3){p.x + .35f, p.y, p.z},
                   scale((Color){.75f, .85f, 1, 1}, twinkle * r->night * (1 - r->rain)), 1.1f,
                   i % 9 == 0);
        }
        float shot = fmodf(g->run_time + (g->seed % 17), 27);
        if (shot < 1.7f) {
            float az = atan2f(r->forward.x, r->forward.z) +
                       ((int)(hash_cell((int)(g->run_time / 27), 3, g->seed) % 60) - 30) * .01f;
            Vec3 p = {r->eye.x + sinf(az) * radius, r->eye.y + radius * (.62f - shot * .13f),
                      r->eye.z + cosf(az) * radius * .65f};
            p.x += shot * radius * .18f;
            r_line(r, p, (Vec3){p.x - radius * .08f, p.y + radius * .06f, p.z},
                   (Color){.75f, .9f, 1, 1}, 1.3f, true);
        }
    }
    Vec3 sun = {r->eye.x - radius * .4f, r->eye.y + radius * .23f, r->eye.z + radius * .74f};
    Color suncol = mix((Color){1, .50f, .30f, 1}, (Color){.72f, .85f, 1, 1}, r->night);
    for (int i = 0; i < 10; i++) {
        float y = -12 + i * 2.7f, half = sqrtf(fmaxf(0, 15 * 15 - y * y));
        r_line(r, (Vec3){sun.x - half, sun.y + y, sun.z}, (Vec3){sun.x + half, sun.y + y, sun.z},
               scale(suncol, 1 - r->rain * .75f), 2.2f, true);
    }
    if (r->settings->weather && r->rain > .05f) {
        for (int i = 0; i < 6; i++) {
            float a1 = i * PI / 3 + g->time * .002f;
            Vec3 p = {r->eye.x + sinf(a1) * radius * .8f,
                      r->eye.y + radius * (.25f + (i % 3) * .10f),
                      r->eye.z + cosf(a1) * radius * .8f};
            r_line(r, (Vec3){p.x - radius * .18f, p.y, p.z}, (Vec3){p.x + radius * .18f, p.y, p.z},
                   (Color){.08f * r->rain, .10f * r->rain, .13f * r->rain, 1}, 14, false);
        }
    }
}
static void people_draw(Render *r, Game *g) {
    for (int i = 0; i < MAX_PEOPLE; i++) {
        Pedestrian *p = &g->people[i];
        if (!p->active || !visible(r, (Vec3){p->x, 1, p->z}, 2))
            continue;
        if (hypotf(p->x - r->eye.x, p->z - r->eye.z) > (r->settings->detail ? 85 : 45))
            continue;
        Color coat = scale(PAINTS[i % PAINT_COUNT], .4f), skin = {.42f, .29f, .21f, 1};
        r_box(r, (Vec3){p->x, 1.05f, p->z}, (Vec3){.5f, .9f, .34f}, p->yaw, coat, (Color){0},
              false);
        r_box(r, (Vec3){p->x, 1.7f, p->z}, (Vec3){.32f, .34f, .32f}, p->yaw, skin, (Color){0},
              false);
        float sn = sinf(p->yaw), cs = cosf(p->yaw);
        for (int side = -1; side <= 1; side += 2) {
            Vec3 hip = local((Vec3){side * .14f, .62f, 0}, (Vec3){p->x, 0, p->z}, sn, cs);
            Vec3 foot = local((Vec3){side * .14f, .08f, sinf(p->walk) * .24f * side},
                              (Vec3){p->x, 0, p->z}, sn, cs);
            r_line(r, hip, foot, (Color){.11f, .14f, .19f, 1}, 1.4f, false);
        }
    }
}
static void supplies_draw(Render *r, Game *g) {
    for (int i = 0; i < MAX_DROPS; i++) {
        SupplyDrop *d = &g->drops[i];
        if (!d->active || !visible(r, d->p, 6))
            continue;
        Color col = PAINTS[d->type == 0 ? 3 : d->type == 1 ? 0 : d->type == 2 ? 2 : 5];
        r_box(r, d->p, (Vec3){1.7f, 1.6f, 1.7f}, g->time * .2f, scale(col, .2f), col, true);
        circle_line(r, (Vec3){d->p.x, .07f, d->p.z}, 3.7f, col, false, true);
        if (d->p.y > 1) {
            pyramid(r, (Vec3){d->p.x, d->p.y + 3, d->p.z}, 3.2f, 1.2f, scale(col, .25f), col);
            for (int a = -1; a <= 1; a += 2)
                r_line(r, d->p, (Vec3){d->p.x + a * 2, d->p.y + 3, d->p.z}, col, .7f, false);
        }
    }
    if (g->giant > 0 || g->shield > 0) {
        Color col = g->giant > 0 ? PAINTS[3] : PAINTS[0];
        float radius = 3 * player_scale(g);
        circle_line(r, (Vec3){g->player.x, .08f, g->player.z}, radius, col, false, true);
        if (g->shield > 0)
            circle_line(r, (Vec3){g->player.x, 1.2f, g->player.z}, 3, col, true, true);
    }
    if (g->sprint_node >= 0) {
        Vec3 p = world_center(&g->world, g->sprint_node);
        if (visible(r, p, 15)) {
            circle_line(r, (Vec3){p.x, .08f, p.z}, 6, PAINTS[2], false, true);
            r_line(r, (Vec3){p.x, 1, p.z}, (Vec3){p.x, 15, p.z}, PAINTS[2], 1.5f, true);
        }
    }
}
static void rain_draw(Render *r, Game *g) {
    if (!r->settings->weather || r->rain < .02f)
        return;
    int count = (int)((r->settings->detail == 2 ? 65 : 32) * r->rain);
    for (int i = 0; i < count; i++) {
        uint32_t h = hash_cell(i, 61, g->seed);
        float side = (int)(h % 600) - 300, forward = 5 + (h >> 10) % 650;
        side *= .1f;
        forward *= .1f;
        float y = 18 - fmodf(g->time * (12 + (i % 5)) + (h % 200) * .1f, 21);
        Vec3 p = {r->eye.x + r->right.x * side + r->forward.x * forward, y,
                  r->eye.z + r->right.z * side + r->forward.z * forward};
        r_line(r, p, (Vec3){p.x - .18f, p.y - 1.2f, p.z + .2f}, (Color){.25f, .38f, .48f, 1}, .7f,
               false);
    }
    if (r->settings->lighting) {
        int n = world_nearest(&g->world, g->player.x, g->player.z);
        Vec3 p = world_center(&g->world, n);
        for (int i = -2; i <= 2; i++)
            light_pool(r, (Vec3){p.x + i * 2, 0, p.z + 12}, 2.2f,
                       scale(THEMES[preview_theme(g)].accent, r->rain * .5f));
    }
}

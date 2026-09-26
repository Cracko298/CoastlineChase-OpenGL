/* Distinct low-poly silhouettes for the twelve unconventional rides. */
static float unusual_car(Render *r, Vehicle v, int model, CarStyle style, Color fill, Color edge) {
    const CarSpec *s = &CARS[model];
    float w = s->width, l = s->length, h = s->height;
    Color dark = {.025f, .03f, .045f, 1}, glass = {.07f, .18f, .24f, 1};
    bool small = model == 23 || model == 24 || model == 31;
    if (model == 20) {
        carpart(r, v, (Vec3){0, .8f, 0}, (Vec3){w, .5f, l}, fill, edge, false);
        carpart(r, v, (Vec3){0, 1.8f, 1.1f}, (Vec3){w * .72f, 1.6f, l * .57f}, fill, edge, true);
        carpart(r, v, (Vec3){0, 2.3f, -l * .32f}, (Vec3){w, 2.2f, l * .26f}, glass, edge, false);
        carpart(r, v, (Vec3){0, 3.5f, -l * .32f}, (Vec3){w * 1.12f, .2f, l * .30f}, fill, edge,
                true);
        carpart(r, v, (Vec3){0, 3, 2.5f}, (Vec3){.65f, 1.6f, .65f}, dark, edge, false);
        for (int i = -1; i <= 1; i++)
            carline(r, v, (Vec3){i * .8f, .45f, l * .55f}, (Vec3){i * .55f, 1.2f, l * .46f}, edge,
                    2, false);
        for (int side = -1; side <= 1; side += 2) {
            for (int axle = 0; axle < 4; axle++)
                wheel(r, v, side * w * .47f, -3 + axle * 1.9f, .67f, style.wheel, edge, false);
            carline(r, v, (Vec3){side * w * .53f, .65f, -2.6f}, (Vec3){side * w * .53f, .65f, 2.6f},
                    PAINTS[5], 2, true);
        }
    } else if (model == 21) {
        carpart(r, v, (Vec3){0, .7f, 0}, (Vec3){w, .45f, l}, dark, edge, false);
        carpart(r, v, (Vec3){0, 1.8f, l * .35f}, (Vec3){w, 2.6f, l * .25f}, fill, edge, false);
        carpart(r, v, (Vec3){0, 2.5f, l * .478f}, (Vec3){w * .8f, .75f, .04f}, glass, edge, true);
        carpart(r, v, (Vec3){0, 2.0f, -l * .19f}, (Vec3){w, 2.8f, l * .57f}, fill, edge, false);
        for (int side = -1; side <= 1; side += 2) {
            carpart(r, v, (Vec3){side * w * .53f, 2.3f, l * .22f}, (Vec3){.2f, 3.3f, .2f}, dark,
                    PAINTS[5], false);
            for (int axle = 0; axle < 4; axle++)
                wheel(r, v, side * w * .48f, axle == 3 ? l * .38f : -l * .4f + axle * 1.3f, .57f,
                      style.wheel, edge, axle == 3);
        }
    } else if (model == 22) {
        carpart(r, v, (Vec3){0, h * .5f + .3f, 0}, (Vec3){w, h, l}, fill, edge, false);
        for (int floor = 0; floor < 2; floor++)
            for (int side = -1; side <= 1; side += 2) {
                float y = 1.5f + floor * 2;
                for (int pane = 0; pane < 6; pane++)
                    carpart(r, v, (Vec3){side * w * .502f, y, -l * .37f + pane * l * .145f},
                            (Vec3){.02f, 1.05f, l * .11f}, glass, edge, true);
            }
        for (int floor = 0; floor < 2; floor++)
            carpart(r, v, (Vec3){0, 1.5f + floor * 2, l * .503f}, (Vec3){w * .83f, 1.05f, .02f},
                    glass, edge, true);
        carpart(r, v, (Vec3){w * .504f, .85f, l * .27f}, (Vec3){.025f, 1.5f, 1.0f}, dark, edge,
                true);
        for (int i = 0; i < 4; i++)
            wheel(r, v, (i & 1 ? 1 : -1) * w * .49f, (i & 2 ? 1 : -1) * l * .33f, .58f, style.wheel,
                  edge, (i & 2) != 0);
    } else if (small) {
        if (model == 24) {
            carpart(r, v, (Vec3){0, .22f, 0}, (Vec3){w, .13f, l}, fill, edge, true);
            for (int i = 0; i < 4; i++)
                wheel(r, v, (i & 1 ? 1 : -1) * w * .43f, (i & 2 ? 1 : -1) * l * .36f, .12f,
                      style.wheel, edge, true);
        } else if (model == 23) {
            wheel(r, v, .04f, -l * .35f, .43f, style.wheel, edge, false);
            wheel(r, v, .04f, l * .35f, .43f, style.wheel, edge, true);
            carpart(r, v, (Vec3){0, .78f, 0}, (Vec3){.48f, .5f, 1.2f}, fill, edge, true);
            carline(r, v, (Vec3){0, .45f, l * .35f}, (Vec3){0, 1.25f, l * .23f}, edge, 2, false);
            carline(r, v, (Vec3){-.5f, 1.25f, l * .23f}, (Vec3){.5f, 1.25f, l * .23f}, edge, 2,
                    true);
        } else {
            wheel(r, v, .15f, 0, 1.0f, style.wheel, edge, false);
            carpart(r, v, (Vec3){0, .9f, 0}, (Vec3){.5f, .3f, .6f}, fill, edge, true);
        }
        float base = model == 24 ? .28f : .65f;
        carpart(r, v, (Vec3){0, base + 1.05f, -.1f}, (Vec3){.45f, .65f, .3f}, fill, edge, false);
        carpart(r, v, (Vec3){0, base + 1.56f, -.1f}, (Vec3){.36f, .36f, .36f}, dark, PAINTS[5],
                true);
        for (int side = -1; side <= 1; side += 2) {
            carline(r, v, (Vec3){side * .18f, base + 1.25f, 0},
                    (Vec3){side * .38f, base + .8f, .45f}, edge, 2, false);
            carline(r, v, (Vec3){side * .15f, base + .8f, 0},
                    (Vec3){side * .22f, base + .06f, side * .35f}, edge, 2, false);
        }
    } else if (model == 30) {
        /* Ground-contact saucer with a luminous rim, dome and four rollers. */
        for (int i = 0; i < 12; i++) {
            float a = i * PI / 6, b = (i + 1) * PI / 6;
            Vec3 p = {cosf(a) * w * .5f, .75f, sinf(a) * l * .5f},
                 q = {cosf(b) * w * .5f, .75f, sinf(b) * l * .5f};
            carquad(r, v, p, q, (Vec3){0, 1.4f, 0}, (Vec3){0, 1.4f, 0}, fill);
            carline(r, v, p, q, edge, 2, true);
        }
        carpart(r, v, (Vec3){0, 1.5f, 0}, (Vec3){w * .38f, .7f, l * .36f}, glass, edge, true);
        for (int i = 0; i < 4; i++)
            wheel(r, v, (i & 1 ? 1 : -1) * w * .32f, (i & 2 ? 1 : -1) * l * .25f, .23f, style.wheel,
                  edge, true);
    } else {
        carpart(r, v, (Vec3){0, .55f, 0}, (Vec3){w, .65f, l}, fill, edge, false);
        if (model == 25) {
            for (int side = -1; side <= 1; side += 2) {
                carpart(r, v, (Vec3){side * .6f, 1.5f, l * .4f}, (Vec3){.18f, 2.8f, .18f}, dark,
                        edge, false);
                carpart(r, v, (Vec3){side * .6f, .25f, l * .53f}, (Vec3){.25f, .15f, 1.2f}, dark,
                        edge, true);
            }
            for (int side = -1; side <= 1; side += 2)
                carline(r, v, (Vec3){side * w * .45f, .8f, -.9f},
                        (Vec3){side * w * .45f, 2.6f, -.9f}, edge, 1.4f, false);
            carpart(r, v, (Vec3){0, 2.6f, -.3f}, (Vec3){w, .15f, 1.4f}, fill, edge, false);
        } else if (model == 26 || model == 27) {
            carpart(r, v, (Vec3){0, .9f, -.4f}, (Vec3){w * .8f, .4f, .6f}, dark, edge, false);
            carpart(r, v, (Vec3){0, h, 0}, (Vec3){w * 1.08f, .2f, l * .7f}, fill, edge, true);
            for (int side = -1; side <= 1; side += 2)
                carline(r, v, (Vec3){side * w * .4f, .7f, l * .28f},
                        (Vec3){side * w * .4f, h, l * .28f}, edge, 1.4f, true);
            if (model == 27)
                carpart(r, v, (Vec3){0, 1.3f, -l * .3f}, (Vec3){w, 1.1f, l * .3f}, fill, edge,
                        false);
        } else if (model == 28) {
            carpart(r, v, (Vec3){0, 1.7f, 0}, (Vec3){w * .9f, 1.15f, l * .8f},
                    (Color){.3f, .14f, .03f, 1}, PAINTS[2], false);
            carpart(r, v, (Vec3){0, 2.3f, 0}, (Vec3){w * .5f, .6f, l * .86f},
                    (Color){.25f, .045f, .02f, 1}, PAINTS[6], false);
            for (int i = 0; i < 8; i++)
                carline(r, v, (Vec3){-.3f, 2.62f, -l * .35f + i * l * .09f},
                        (Vec3){.3f, 2.62f, -l * .305f + i * l * .09f}, PAINTS[2], 2, true);
        } else if (model == 29) {
            carpart(r, v, (Vec3){0, 1.25f, l * .25f}, (Vec3){1.2f, 1.25f, 1.2f}, fill, PAINTS[2],
                    true);
            carpart(r, v, (Vec3){0, 1.2f, l * .48f}, (Vec3){.85f, .24f, .7f}, fill, PAINTS[6],
                    false);
            for (int side = -1; side <= 1; side += 2)
                carpart(r, v, (Vec3){side * .61f, 1.6f, l * .32f}, (Vec3){.04f, .13f, .18f}, dark,
                        PAINTS[5], false);
        }
        for (int i = 0; i < (model == 27 ? 3 : 4); i++) {
            float x = (i & 1 ? 1 : -1) * w * .48f, z = (i & 2 ? 1 : -1) * l * .32f;
            if (model == 27 && i == 2)
                x = .02f;
            wheel(r, v, x, z, .32f, style.wheel, edge, (i & 2) != 0);
        }
    }
    float deck = model == 24 ? .29f : .9f;
    if (style.decal == 1 || style.decal == 2) {
        int count = style.decal;
        for (int i = 0; i < count; i++) {
            float x = count == 2 ? (i ? 1 : -1) * w * .18f : 0;
            carline(r, v, (Vec3){x, deck, -l * .43f}, (Vec3){x, deck, l * .43f}, PAINTS[5], 2,
                    true);
        }
    } else if (style.decal == 6) {
        for (int x = 0; x < 4; x++)
            for (int z = 0; z < 6; z++)
                if ((x + z) % 2 == 0) {
                    float xx = (x - 2) * w * .18f, zz = (z - 3) * l * .1f;
                    carquad(r, v, (Vec3){xx, deck, zz}, (Vec3){xx + w * .18f, deck, zz},
                            (Vec3){xx + w * .18f, deck, zz + l * .1f},
                            (Vec3){xx, deck, zz + l * .1f}, PAINTS[5]);
                }
    } else if (style.decal == 5) {
        for (int digit = -1; digit <= 1; digit += 2)
            for (int loop = 0; loop < (digit == 1 ? 2 : 1); loop++) {
                float x = digit * w * .19f, y = deck + .01f, z = (loop - .5f) * l * .15f;
                carline(r, v, (Vec3){x - w * .12f, y, z}, (Vec3){x + w * .12f, y, z}, PAINTS[5],
                        1.3f, false);
                carline(r, v, (Vec3){x - w * .12f, y, z + l * .14f},
                        (Vec3){x + w * .12f, y, z + l * .14f}, PAINTS[5], 1.3f, false);
                for (int side = -1; side <= 1; side += 2)
                    carline(r, v, (Vec3){x + side * w * .12f, y, z},
                            (Vec3){x + side * w * .12f, y, z + l * .14f}, PAINTS[5], 1.3f, false);
            }
    } else if (style.decal) {
        for (int i = 0; i < 12; i++) {
            float a = -l * .42f + i * l * .07f, b = a + l * .07f;
            float xa = style.decal == 7 ? sinf(i) * w * .25f : (i % 2 ? -.2f : .2f) * w;
            float xb = style.decal == 7 ? sinf(i + 1) * w * .25f : ((i + 1) % 2 ? -.2f : .2f) * w;
            carline(r, v, (Vec3){xa, deck, a}, (Vec3){xb, deck, b},
                    PAINTS[style.decal == 3   ? 2
                           : style.decal == 4 ? 3
                                              : 0],
                    1.6f, true);
        }
    }
    carline(r, v, (Vec3){-w * .25f, .7f, l * .51f}, (Vec3){w * .25f, .7f, l * .51f}, PAINTS[5], 2,
            true);
    return h + .25f;
}

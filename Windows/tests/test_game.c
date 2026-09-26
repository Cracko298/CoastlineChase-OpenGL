#include "game.h"
#include "render.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
static Game game;
static World wa, wb;
static void fresh(void) {
    game_init(&game, false, 0xC0A57);
    game_start(&game);
}
static void tick(unsigned held, float steer, int n) {
    for (int i = 0; i < n; i++)
        game_update(&game, (Input){.held = held, .steer = steer}, 1.0f / 60);
}
static void world_tests(void) {
    unsigned districts = 0, doors = 0, roofs = 0;
    float minh = 999, maxh = 0;
    unsigned differing = 0, outlines = 0, bends = 0, incomplete_junctions = 0;
    for (unsigned seed = 1; seed <= 40; seed++) {
        world_init(&wa, seed * 537);
        world_init(&wb, seed * 537);
        assert(memcmp(wa.tiles, wb.tiles, sizeof wa.tiles) == 0);
        assert(!strcmp(wa.name, wb.name) && strlen(wa.name) > 6);
        assert(!memcmp(wa.shore, wb.shore, sizeof wa.shore));
        assert(wa.land_count > 150 && wa.land_count <= MAP_TILES);
        assert(wa.bridge_count >= 1);
        int16_t field[MAP_TILES];
        world_distances(&wa, wa.spawn, field);
        for (int z = 0; z < 32; z++)
            for (int x = 0; x < 32; x++) {
                int n = z * 32 + x;
                const Tile *t = &wa.tiles[n];
                if (x == 0 || z == 0 || x == 31 || z == 31)
                    assert(!t->kind);
                if (!t->kind)
                    continue;
                assert(field[n] >= 0);
                districts |= 1u << t->kind;
                if (t->ox || t->oz)
                    bends++;
                if (t->roads != 15)
                    incomplete_junctions++;
                Chunk a = *world_chunk(&wa, x, z), b = *world_chunk(&wb, x, z);
                assert(memcmp(&a, &b, sizeof a) == 0);
                if (t->landmark)
                    assert(a.count > 0);
                for (int k = 0; k < a.count; k++) {
                    Building p = a.b[k];
                    assert(p.w > 0 && p.d > 0 && p.h > 0 && p.windows_x > 0 && p.windows_z > 0 &&
                           p.floors > 0);
                    assert(p.x >= x * CELL && p.z >= z * CELL && p.x + p.w <= (x + 1) * CELL &&
                           p.z + p.d <= (z + 1) * CELL);
                    assert(world_coast(&wa, p.x, p.z) >= 1 &&
                           world_coast(&wa, p.x + p.w, p.z + p.d) >= 1);
                    minh = fminf(minh, p.h);
                    maxh = fmaxf(maxh, p.h);
                    doors |= 1u << p.door;
                    roofs |= 1u << p.roof;
                    for (int j = 0; j < k; j++) {
                        Building q = a.b[j];
                        assert(p.x >= q.x + q.w || q.x >= p.x + p.w || p.z >= q.z + q.d ||
                               q.z >= p.z + p.d);
                    }
                }
                for (int d = 0; d < 4; d++) {
                    int other = world_neighbor(&wa, n, d);
                    if (other < 0)
                        continue;
                    assert(world_neighbor(&wa, other, (d + 2) % 4) == n);
                    if (seed <= 40) {
                        Vec3 p = world_center(&wa, n), q = world_center(&wa, other);
                        for (int i = 0; i <= 16; i++) {
                            float t = i / 16.0f;
                            assert(!world_solid(&wa, p.x + (q.x - p.x) * t, p.z + (q.z - p.z) * t,
                                                1.6f));
                        }
                    }
                }
            }
        world_init(&wb, seed * 537 + 1);
        if (memcmp(wa.tiles, wb.tiles, sizeof wa.tiles))
            differing++;
        unsigned changed = 0;
        for (int n = 0; n < MAP_TILES; n++)
            if (!!wa.tiles[n].kind != !!wb.tiles[n].kind)
                changed++;
        if (changed > 24)
            outlines++;
    }
    assert(outlines >= 38 && bends > 1000 && incomplete_junctions > 1000);
    assert(differing == 40 && doors == 15 && roofs == 15 && minh <= 5 && maxh >= 80);
    assert((districts & 0x1fe) == 0x1fe);
    Vec3 p = world_center(&wa, wa.spawn);
    world_stream(&wa, p.x, p.z, 3);
    unsigned n = wa.generated;
    world_stream(&wa, p.x, p.z, 3);
    assert(n == wa.generated);
    assert(!world_surface(&wa, -1, 100));
    assert(!world_surface(&wa, 10, 10));
    Vehicle v = {.x = -10, .z = -10, .vx = -4};
    float impact;
    world_resolve(&wa, &v, 1.2f, &impact);
    assert(world_surface(&wa, v.x, v.z));
    printf("PASS: 40 deterministic connected island maps, bridges, reciprocal road links, all "
           "districts; buildings %.0f-%.0f m, four doors/roofs\n",
           minh, maxh);
}
static uint32_t sum(const uint8_t *b, int n) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) {
        h ^= b[i];
        h *= 16777619u;
    }
    return h;
}
static void put(uint8_t *b, int *n, uint32_t v) {
    for (int k = 0; k < 4; k++)
        b[(*n)++] = (uint8_t)(v >> (k * 8));
}
static void legacy_save(const char *path) {
    uint8_t b[153] = {0};
    memcpy(b, "CC3D", 4);
    int n = 4;
    put(b, &n, 1);
    uint32_t vals[] = {1234, 3200, 14, 0x21, 3, 3, 3, 3};
    for (int i = 0; i < 8; i++)
        put(b, &n, vals[i]);
    b[n++] = 5;
    b[n++] = 1;
    b[n++] = 1;
    b[n++] = 1;
    b[n++] = 1;
    for (int c = 0; c < 8; c++)
        for (int k = 0; k < 4; k++)
            b[n++] = (c == 5 && k == 2) ? 4 : 0;
    int settings[] = {1, 3, 1, 1, 1, 1, 1, 1, 1, 30, 1, 0, 1, 1, 1, 1, 6, 0};
    for (int i = 0; i < 18; i++)
        put(b, &n, settings[i]);
    uint32_t h = sum(b, n);
    put(b, &n, h);
    assert(n == 153);
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fwrite(b, 1, 153, f) == 153);
    fclose(f);
}
static void save_tests(void) {
    char path[128], bak[140], tmp[140];
    snprintf(path, sizeof path, "/tmp/coast-v2-%ld.sav", (long)getpid());
    snprintf(bak, sizeof bak, "%s.bak", path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    CoastProfile a, b;
    CoastProfile migrated;
    assert(profile_load(&migrated, "tests/fixtures/v02.sav", false));
    assert(migrated.money == 54321 && migrated.selected == 19 && migrated.styles[19].engine == 5);
    assert(migrated.styles[19].wheel == 6 && migrated.styles[19].decal == 7 &&
           migrated.upgrades[19][3] == 4);
    assert(migrated.settings.lighting == 1 && migrated.settings.weather == 1);
    assert(profile_load(&migrated, "tests/fixtures/v03.sav", false));
    assert(migrated.money == 4321 && migrated.selected == 19 && migrated.upgrades[19][0] == 3);
    for (int car = 20; car < CAR_COUNT; car++)
        assert(migrated.styles[car].engine == 0 && migrated.upgrades[car][0] == 0);
    legacy_save(path);
    assert(profile_load(&a, path, false));
    assert(a.money == 1234 && a.selected == 5 && a.upgrades[5][2] == 4);
    assert(a.styles[5].paint == 1 && a.styles[19].hat == 1 && a.engines == 1);
    a.cars = UINT32_MAX;
    a.selected = 31;
    a.upgrades[31][3] = 5;
    a.wheels |= 1u << 6;
    a.decals |= 1u << 7;
    a.engines |= 1u << 5;
    a.styles[19].wheel = 6;
    a.styles[19].decal = 7;
    a.styles[19].engine = 5;
    assert(profile_save(&a, path));
    assert(profile_load(&b, path, false));
    assert(!memcmp(&a, &b, sizeof a));
    FILE *f = fopen(path, "wb");
    assert(f);
    fputs("BROKEN", f);
    fclose(f);
    assert(profile_load(&b, path, false));
    assert(b.selected == 5 && b.money == 1234);
    assert(profile_save(&b, path));
    b.styles[5].engine = 99;
    assert(!profile_save(&b, path));
    remove(path);
    remove(bak);
    remove(tmp);
    puts("PASS: v0.1/v0.2/v0.3 migration, 32 ownership bits/loadouts, v4 round trip, corrupt "
         "primary recovery, "
         "validation");
}
static void shop_tests(void) {
    fresh();
    game.screen = SHOP;
    game.tab = SHOP_CARS;
    game.cursor = 19;
    game.profile.money = 1;
    game_purchase(&game);
    assert(game.profile.selected == 0 && game.profile.money == 1);
    game.profile.money = 100000;
    game_purchase(&game);
    assert(game.profile.selected == 19);
    for (int t = 1; t < SHOP_TABS; t++) {
        game.tab = t;
        game.cursor = shop_count(t) - 1;
        CarStyle before = game.profile.styles[19];
        uint32_t money = game.profile.money;
        (void)preview_style(&game);
        assert(!memcmp(&before, &game.profile.styles[19], sizeof before) &&
               money == game.profile.money);
        game_purchase(&game);
        assert(t == SHOP_UPGRADES ? game.profile.upgrades[19][3] == 1
                                  : shop_owned(&game, t, game.cursor));
    }
    CarStyle style = game.profile.styles[19];
    game.tab = SHOP_CARS;
    game.cursor = 0;
    game_purchase(&game);
    assert(game.profile.styles[0].decal == 0 &&
           !memcmp(&style, &game.profile.styles[19], sizeof style));
    game.tab = SHOP_UPGRADES;
    game.cursor = 0;
    for (int i = 0; i < 7; i++)
        game_purchase(&game);
    assert(game.profile.upgrades[0][0] == 5);
    for (int i = 0; i < SETTING_COUNT; i++)
        for (int j = 0; j < 35; j++) {
            setting_change(&game, i, 1);
            char value[64];
            setting_value(&game.profile.settings, i, value, sizeof value);
            assert(*value);
        }
    fresh();
    game.screen = SHOP;
    game.profile.money = 1000;
    game.tab = SHOP_CARS;
    game.cursor = 1;
    uint32_t money = game.profile.money;
    menus_step(&game, (Input){.pressed = IN_A}, .016f);
    assert(game.confirm_purchase && game.profile.money == money);
    menus_step(&game, (Input){.pressed = IN_B}, .016f);
    assert(!game.confirm_purchase && game.profile.money == money);
    menus_step(&game, (Input){.pressed = IN_A}, .016f);
    menus_step(&game, (Input){.pressed = IN_A}, .016f);
    assert(game.profile.selected == 1 && game.profile.money == money - 350);
    menus_step(&game, (Input){.pressed = IN_B}, .016f);
    assert(game.screen == GARAGE);
    puts("PASS: preview without spending, purchase/cancel, per-car loadouts, upgrade caps, 17 "
         "settings");
}
static void driving_tests(void) {
    fresh();
    Vec3 p = {0};
    bool lane = false;
    for (int n = 0; n < MAP_TILES - 96 && !lane; n++) {
        p = world_center(&game.world, n);
        lane = true;
        for (int z = 0; z <= 130; z += 2)
            if (world_solid(&game.world, p.x, p.z + z, 1.5f)) {
                lane = false;
                break;
            }
    }
    assert(lane);
    game.player.x = p.x;
    game.player.z = p.z;
    for (int i = 0; i < 300; i++)
        drive_step(&game, &game.player, 1, 0, 0, false, false, 0, 1.0f / 60);
    float speed = vehicle_speed(&game.player);
    assert(speed > 16 && speed <= 19.01f);
    for (int i = 0; i < 120; i++)
        drive_step(&game, &game.player, 1, 0, 0, false, true, 0, 1.0f / 60);
    assert(vehicle_speed(&game.player) <= 23.01f);
    for (int i = 0; i < 120; i++)
        drive_step(&game, &game.player, 0, 1, 0, false, false, 0, 1.0f / 60);
    assert(vehicle_speed(&game.player) < 7);
    for (int c = 0; c < CAR_COUNT; c++) {
        for (int e = 0; e < ENGINE_COUNT; e++)
            for (int w = 0; w < WHEEL_COUNT; w++) {
                CarSpec s = car_tuned(&game.profile, c, e, w);
                assert(s.speed < 30 && s.accel < 12 && s.grip > 5);
            }
    }
    fresh();
    tick(IN_A, 0, 100);
    tick(IN_A | IN_R, .9f, 50);
    assert(fabsf(game.player.yaw) > .1f && game.drift_distance > 0);
    float t = game.run_time;
    game_update(&game, (Input){.pressed = IN_START}, 1.0f / 60);
    tick(IN_A, 0, 30);
    assert(game.run_time == t && game.screen == PAUSE);
    menus_step(&game, (Input){.pressed = IN_START}, .016f);
    assert(game.screen == PLAY);
    game.earned = 200;
    game_end(&game, 2);
    uint32_t money = game.profile.money;
    game_end(&game, 2);
    assert(game.profile.money == money && money >= 200);
    puts("PASS: starter 68 km/h / boost 83 km/h caps, braking, drivetrain bounds, drift, pause, "
         "payout");
}
static void police_tests(void) {
    int last = 0;
    for (int t = 0; t < 1200; t++) {
        int n = game_police_budget((float)t);
        assert(n >= last && n <= MAX_POLICE);
        last = n;
    }
    assert(last == MAX_POLICE && game_heli_budget(89) == 0 && game_heli_budget(90) == 1 &&
           game_heli_budget(360) == 3);
    fresh();
    game.run_time = 60;
    int node = -1;
    for (int n = 0; n < MAP_TILES; n++)
        if (game.world.tiles[n].roads && game.chase_field[n] > 10) {
            node = n;
            break;
        }
    assert(node >= 0);
    Vec3 p = world_center(&game.world, node);
    Actor *a = &game.actors[MAX_TRAFFIC];
    *a = (Actor){.active = 1,
                 .kind = PATROL,
                 .model = 5,
                 .node = node,
                 .target = node,
                 .tx = p.x,
                 .tz = p.z,
                 .v = {.x = p.x, .z = p.z, .hp = 100}};
    int before = game.chase_field[node];
    for (int i = 0; i < 60 * 25; i++) {
        police_step(&game, 1.0f / 60);
        game.time += 1.0f / 60;
        assert(a->active);
    }
    int after = game.chase_field[world_nearest(&game.world, a->v.x, a->v.z)];
    printf("Persistent unit route distance: %d -> %d tiles\n", before, after);
    assert(after < before);
    fresh();
    game.run_time = 600;
    int max_active = 0, types = 0;
    for (int i = 0; i < 60 * 150; i++) {
        if (game.screen == RESULT) {
            game_start(&game);
            game.run_time = 600;
        }
        game_update(
            &game,
            (Input){.held = IN_A | ((i / 180) % 2 ? IN_L : 0), .steer = sinf(i * .004f) * .32f},
            1.0f / 60);
        assert(isfinite(game.player.x) && isfinite(game.player.z) && isfinite(game.player.yaw));
        assert(world_surface(&game.world, game.player.x, game.player.z));
        assert(game.player.nitro >= 0 && game.player.nitro <= 100);
        assert(vehicle_speed(&game.player) < 35);
        int active = 0;
        for (int j = MAX_TRAFFIC; j < MAX_ACTORS; j++)
            if (game.actors[j].active) {
                active++;
                types |= 1 << game.actors[j].kind;
                assert(isfinite(game.actors[j].v.x));
            }
        if (active > max_active)
            max_active = active;
        for (int h = 0; h < 3; h++) {
            assert(game.helis[h].active);
            assert(isfinite(game.helis[h].p.y) && game.helis[h].p.y > 0);
        }
    }
    printf(
        "PASS: 150-second late-wave stress / peak %d ground cops / type mask %X / 3 helicopters\n",
        max_active, types);
    assert(max_active >= 10 && (types & (1 << SWAT)) && (types & (1 << POLICE_SUV)) &&
           (types & (1 << INTERCEPTOR)));
    fresh();
    game.run_time = 25;
    for (int i = 0; i < 60 * 6 && game.screen == PLAY; i++) {
        Actor *cop = &game.actors[MAX_TRAFFIC];
        *cop = (Actor){.active = 1,
                       .kind = PATROL,
                       .model = 5,
                       .target = game.world.spawn,
                       .v = {.x = game.player.x + 5, .z = game.player.z, .hp = 100},
                       .tx = game.player.x + 5,
                       .tz = game.player.z + 64};
        police_step(&game, 1.0f / 60);
    }
    assert(game.screen == RESULT && game.reason == 1);
    puts("PASS: sustained arrest / no escape timer or distance despawn");
}
static void contact_tests(void) {
    fresh();
    Vec3 p = world_center(&game.world, game.world.spawn);
    const float speeds[][2] = {{0, 12}, {8, 16}, {12, 12}, {18, 4}};
    for (int i = 0; i < 4; i++) {
        game.player = (Vehicle){.x = p.x, .z = p.z, .vx = speeds[i][0], .hp = 100};
        game.hit_cool = 0;
        Actor a = {.active = 1,
                   .kind = CIVILIAN,
                   .model = 5,
                   .v = {.x = p.x + 2, .z = p.z, .vx = -speeds[i][1], .hp = 100}};
        vehicle_contact(&game, &a);
        if (i < 3)
            assert(game.player.hp == 100 && a.v.hp == 100);
        else
            assert(game.player.hp < 100 && a.v.hp < 100);
    }
    supply_collect(&game, POWER_GIANT);
    assert(player_scale(&game) > 1.8f);
    game.player = (Vehicle){.x = p.x, .z = p.z, .vx = 18, .hp = 100};
    game.hit_cool = 0;
    Actor a = {.active = 1, .kind = PATROL, .model = 5, .v = {.x = p.x + 2, .z = p.z, .hp = 100}};
    vehicle_contact(&game, &a);
    assert(game.player.hp == 100 && !a.active);
    supply_collect(&game, POWER_SHIELD);
    supply_collect(&game, POWER_MAGNET);
    assert(game.shield == 18 && game.magnet == 22);
    game.player.hp = 30;
    game.player.nitro = 5;
    supply_collect(&game, POWER_REPAIR);
    assert(game.player.hp == 70 && game.player.nitro == 100);
    float t = game.giant;
    game.screen = PAUSE;
    game_update(&game, (Input){0}, .05f);
    assert(game.giant == t);
    puts("PASS: faster-player contact damage rule, civilian equal/slower immunity, giant collision scale, "
         "all four supplies, paused timers");
}
static void release_four_tests(void) {
    fresh();
    float baseline = game_pressure(&game);
    Vec3 c = world_center(&game.world, game.world.spawn);
    for (int kind = CIVILIAN; kind <= INTERCEPTOR; kind++) {
        game.property_damage = 0;
        game.hit_cool = 0;
        game.player = (Vehicle){.x = c.x, .z = c.z, .hp = 100};
        Actor a = {.active = 1,
                   .kind = kind,
                   .model = 5,
                   .v = {.x = c.x + 2, .z = c.z, .vx = -15, .hp = 100}};
        vehicle_contact(&game, &a);
        assert(a.v.hp == 100 && game.property_damage == 0);
        assert(kind==CIVILIAN ? game.player.hp==100 : game.player.hp<100 && game.player.hp>=93);
        game.player = (Vehicle){.x = c.x, .z = c.z, .vx = 18, .hp = 100};
        game.hit_cool = 0;
        a = (Actor){
            .active = 1, .kind = kind, .model = 5, .v = {.x = c.x + 2, .z = c.z, .hp = 100}};
        vehicle_contact(&game, &a);
        assert(game.property_damage > 0 && fabsf(game.property_damage - (100 - a.v.hp)) < .001f);
        assert(game_pressure(&game) > baseline && game_police_budget(game_pressure(&game)) > 0);
        float damage = game.property_damage;
        game.player = (Vehicle){.x = c.x, .z = c.z, .vx = 18, .hp = 100};
        a.v.x = c.x + 2;
        a.v.z = c.z;
        vehicle_contact(&game, &a);
        assert(game.property_damage == damage);
    }
    game.property_damage = 10000;
    game.run_time = 500;
    assert(game_police_budget(game_pressure(&game)) == MAX_POLICE);
    assert(game_heli_budget(game_pressure(&game)) == MAX_HELIS);
    fresh();
    game.player = (Vehicle){.x = c.x, .z = c.z, .vx = 2.5f, .hp = 100};
    Actor *cop = &game.actors[MAX_TRAFFIC];
    *cop =
        (Actor){.active = 1, .kind = PATROL, .model = 5, .v = {.x = c.x + 5, .z = c.z, .hp = 100}};
    for (int i = 0; i < 120; i++)
        game_arrest_step(&game, .05f);
    assert(game.bust == 0 && game.screen == PLAY);
    game.player.vx = 0;
    for (int i = 0; i < 50; i++)
        game_arrest_step(&game, .05f);
    assert(game.bust > 2.4f && game.screen == PLAY);
    cop->v.x = c.x + 9;
    for (int i = 0; i < 50; i++)
        game_arrest_step(&game, .05f);
    assert(game.bust == 0);
    const Chunk *chunk = NULL;
    for (int n = 0; n < MAP_TILES; n++)
        if (game.world.chunks[n].count) {
            chunk = &game.world.chunks[n];
            break;
        }
    assert(chunk);
    const Building *b = &chunk->b[0];
    game.player.x = b->x - 1.5f;
    game.player.z = b->z + 2.5f;
    cop->v.x = b->x + 2.5f;
    cop->v.z = b->z - 1.5f;
    for (int i = 0; i < 120; i++)
        game_arrest_step(&game, .05f);
    assert(game.bust == 0 && game.screen == PLAY);

    game.player =
        (Vehicle){.x = b->x - 1.7f, .z = b->z + b->d * .5f, .vx = 19, .yaw = PI / 2, .hp = 100};
    for (int i = 0; i < 30; i++)
        drive_step(&game, &game.player, 1, 0, 0, false, false, 0, 1.f / 60);
    assert(game.player.hp == 100 && game.property_damage == 0);
    fresh();
    game.drifting = true;
    game.player.vz = 15;
    game.player.yaw = 0;
    for (int i = 0; i < 120; i++)
        game_drift_score(&game, .25f, 1.f / 60);
    assert(game.drift_score == 0);
    game.player.vx = 5;
    for (int i = 0; i < 360; i++)
        game_drift_score(&game, .25f, 1.f / 60);
    assert(game.drift_score > 600 && game.drift_distance > 89 && game.drift_chain > 5.9f);
    int cash = (int)(game.drift_score / 10);
    uint32_t bank = game.profile.money;
    game_end(&game, 1);
    assert(game.reason == 1 && game.last_payout == cash &&
           game.profile.money == bank + (unsigned)cash);
    game_end(&game, 1);
    assert(game.profile.money == bank + (unsigned)cash);
    fresh();
    game.profile.selected = 20;
    game.player = (Vehicle){.x = c.x, .z = c.z, .vz = 17, .hp = 100};
    Actor front = {
        .active = 1, .kind = CIVILIAN, .model = 0, .v = {.x = c.x, .z = c.z + 6, .hp = 100}};
    vehicle_contact(&game, &front);
    assert(front.v.hp < 100 && game.property_damage > 0);
    for (int model = 20; model < CAR_COUNT; model++) {
        fresh();
        game.profile.selected = model;
        for (int i = 0; i < 240; i++)
            game_update(&game, (Input){.held = IN_A, .steer = .15f}, 1.f / 60);
        assert(world_surface(&game.world, game.player.x, game.player.z));
        assert(isfinite(game.player.yaw) && vehicle_speed(&game.player) <= CARS[model].speed + .1f);
    }

    int rounded = 0, extensions = 0, dense = 0, landmarks = 0;
    for (int n = 0; n < MAP_TILES; n++) {
        const Chunk *ch = &game.world.chunks[n];
        if (ch->count > 4)
            dense++;
        if (ch->landmark)
            landmarks |= 1 << ch->landmark;
        for (int z = 1; z < 64; z += 8)
            for (int x = 1; x < 64; x += 8) {
                float px = (n % 32) * CELL + x, pz = (n / 32) * CELL + z;
                bool surface = world_surface(&game.world, px, pz);
                if (!ch->kind && surface)
                    extensions++;
                if (ch->kind > 0 && ch->kind < BRIDGE_EW && !surface)
                    rounded++;
            }
    }
    fprintf(stderr, "Coast samples: rounded=%d extensions=%d dense=%d\n", rounded, extensions,
            dense);
    assert(rounded > 0 && extensions > 100 && dense > 10);
    assert((landmarks & ((1 << (LANDMARK_COUNT + 1)) - 2)) == ((1 << (LANDMARK_COUNT + 1)) - 2));
    puts("PASS: player-only damage dispatch, all CPU types, cooldown, capped escalation, arrest "
         "range/speed/LOS, scenery immunity, real-slip drift payout, organic shore and dense lots");
}
static void police_damage_tests(void) {
    fresh();Vec3 p=world_center(&game.world,game.world.spawn);
    float last=0;
    const float speeds[]={2,6,12,20,35,60};
    for(unsigned i=0;i<sizeof speeds/sizeof speeds[0];i++) {
        game.player=(Vehicle){.x=p.x,.z=p.z,.hp=100};game.hit_cool=0;
        Actor a={.active=1,.kind=PATROL,.model=5,.v={.x=p.x+2,.z=p.z,.vx=-speeds[i],.hp=100}};
        vehicle_contact(&game,&a);float loss=100-game.player.hp;
        assert(loss>=last && loss<=7 && a.v.hp==100 && game.property_damage==0);
        if(i==0)assert(loss==0);else assert(loss>0);
        last=loss;
    }
    assert(last==7);

    game.player.x=p.x;game.player.z=p.z;game.player.vx=game.player.vz=0;
    Actor second={.active=1,.kind=SWAT,.model=4,.v={.x=p.x+2,.z=p.z,.vx=-25,.hp=200}};
    vehicle_contact(&game,&second);assert(game.player.hp==93 && second.v.hp==200);
    game.hit_cool=0;game.profile.upgrades[0][2]=5;
    game.player=(Vehicle){.x=p.x,.z=p.z,.hp=100};
    second=(Actor){.active=1,.kind=SWAT,.model=4,.v={.x=p.x+2,.z=p.z,.vx=-20,.hp=200}};
    vehicle_contact(&game,&second);assert(100-game.player.hp<3);
    for(int power=0;power<2;power++) {
        game.hit_cool=0;game.player=(Vehicle){.x=p.x,.z=p.z,.hp=100};
        game.shield=power?10:0;game.giant=power?0:10;
        second=(Actor){.active=1,.kind=SWAT,.model=4,.v={.x=p.x+2,.z=p.z,.vx=-25,.hp=200}};
        vehicle_contact(&game,&second);assert(game.player.hp==100);
    }
    game.giant=game.shield=0;game.hit_cool=0;
    game.player=(Vehicle){.x=p.x,.z=p.z,.hp=100};
    second=(Actor){.active=1,.kind=PATROL,.model=5,.v={.x=p.x+2,.z=p.z,.vx=25,.hp=100}};
    vehicle_contact(&game,&second);assert(game.player.hp==100);
    puts("PASS: police speed-proportional damage, 7-hull cap, armor, shared cooldown, power protection and attribution");
}
static void visual_independence_tests(void) {
    static Game a, b;
    game_init(&a, 0, 913);
    game_start(&a);
    b = a;
    settings_preset(&a.profile.settings, 0);
    settings_preset(&b.profile.settings, 2);
    a.profile.settings.traffic = 0;
    b.profile.settings.traffic = 2;
    a.profile.settings.steering = 0;
    b.profile.settings.steering = 2;
    for (int i = 0; i < 60 * 90; i++) {
        if (a.screen == RESULT) {
            game_start(&a);
            game_start(&b);
        }
        Input input = {.held = IN_A | ((i / 120) % 2 ? IN_R : IN_L), .steer = sinf(i * .02f) * .7f};
        game_update(&a, input, 1.f / 60);
        game_update(&b, input, 1.f / 60);
        assert(!memcmp(&a.player, &b.player, sizeof a.player));
        assert(!memcmp(a.actors, b.actors, sizeof a.actors));
        assert(a.rng == b.rng && a.earned == b.earned && a.screen == b.screen);
    }
    for (int i = 0; i < SETTING_COUNT; i++) {
        assert(!strstr(setting_name(i), "TRAFFIC") && !strstr(setting_name(i), "STEERING"));
        assert(!strstr(setting_name(i), "MUSIC") && !strstr(setting_name(i), "SOUND"));
    }
    puts("PASS: 90-second identical simulation across graphics presets and legacy traffic/steering "
         "values");
}
static void ambience_tests(void) {
    fresh();
    int maxsteps = 0, supply_types = 0, spawned = 0;
    for (int i = 0; i < 60 * 210; i++) {
        game.run_time += 1.f / 60;
        ambience_step(&game, 1.f / 60);
        for (int j = 0; j < MAX_PEOPLE; j++)
            if (game.people[j].active) {
                spawned++;
                assert(isfinite(game.people[j].x) &&
                       world_surface(&game.world, game.people[j].x, game.people[j].z));
                if (game.people[j].steps > maxsteps)
                    maxsteps = game.people[j].steps;
            }
        for (int j = 0; j < MAX_DROPS; j++)
            if (game.drops[j].active) {
                supply_types |= 1 << game.drops[j].type;
                assert(!world_solid(&game.world, game.drops[j].p.x, game.drops[j].p.z, 1.5f));
            }
    }
    printf("Pedestrian traversals: %d; supply type mask: %X\n", maxsteps, supply_types);
    assert(spawned && maxsteps >= 2 && supply_types == 15);
    unsigned generated = game.world.generated;
    for (int n = 0; n < MAP_TILES; n++)
        world_chunk(&game.world, n % 32, n / 32);
    assert(generated == game.world.generated && generated == MAP_TILES);
    puts("PASS: roaming pedestrian block transitions, all four safe supply types, immutable city "
         "cache");
}
static void map_tests(void) {
    fresh();
    int target = -1;
    for (int n = 0; n < MAP_TILES; n++)
        if (game.chase_field[n] > 6) {
            target = n;
            break;
        }
    assert(target >= 0);
    game_set_waypoint(&game, target);
    int node = game.world.spawn, steps = 0;
    while (node != target && steps++ < MAP_TILES) {
        int next = -1;
        for (int d = 0; d < 4; d++) {
            int n = world_neighbor(&game.world, node, d);
            if (n >= 0 && game.gps_field[n] >= 0 && game.gps_field[n] < game.gps_field[node]) {
                next = n;
                break;
            }
        }
        assert(next >= 0);
        node = next;
    }
    assert(node == target);
    game_update(&game, (Input){.touch = true, .touch_x = 30, .touch_y = 80}, 1.0f / 60);
    assert(game.screen == CITYMAP);
    menus_step(&game, (Input){.pressed = IN_A}, .016f);
    assert(game.screen == PLAY);
    puts("PASS: radar/map navigation, connected GPS route, return to run");
}
static void position(int node, float yaw) {
    Vec3 p = world_center(&game.world, node);
    game.player = (Vehicle){.x = p.x,
                            .z = p.z,
                            .yaw = yaw,
                            .hp = 82,
                            .nitro = 77,
                            .vx = sinf(yaw) * 13,
                            .vz = cosf(yaw) * 13};
    game.camera_yaw = yaw;
    game.camera_x = p.x - sinf(yaw) * 14;
    game.camera_z = p.z - cosf(yaw) * 14;
    game.camera_height = 6;
    game.notice_time = 0;
}
static void preview(const char *prefix) {
    Render r;
    assert(render_init(&r));
    fresh();
    game.profile.settings.view = 3;
    game.profile.settings.bloom = 1;
    game.profile.settings.detail = 2;
    game.profile.settings.showfps = 0;
    char path[512];
    for (int scene = 0; scene < 25; scene++) {
        game.screen = PLAY;
        game.run_time = 240;
        game.time = 15;
        game.wave = 5;
        game.heat = 3;
        game.earned = 315;
        game.notice_time = 0;
        game.giant = game.shield = game.magnet = 0;
        game.sprint_node = -1;
        game.high_camera = false;
        game.profile.settings.camera = 1;
        memset(&game.helper, 0, sizeof game.helper);
        memset(game.drops, 0, sizeof game.drops);
        memset(game.actors, 0, sizeof game.actors);
        memset(game.helis, 0, sizeof game.helis);
        position(game.world.spawn, 0);
        if (scene == 0) {
            game.screen = TITLE;
            game.time = 2;
        }
        if (scene == 1) {
            int node = game.world.spawn;
            for (int z = 4; z < 28; z++)
                for (int x = 2; x < 25; x++) {
                    const Tile *t = world_tile(&game.world, x, z);
                    if (t->kind && t->kind < BRIDGE_EW && (t->roads & 1) &&
                        !world_tile(&game.world, x - 1, z)->kind) {
                        node = z * 32 + x;
                        break;
                    }
                }
            position(node, 0);
            game.actors[12] =
                (Actor){.active = 1,
                        .kind = POLICE_SUV,
                        .model = 7,
                        .v = {.x = game.player.x - 3, .z = game.player.z + 20, .hp = 150}};
            game.helis[0] = (Helicopter){.active = true,
                                         .p = {game.player.x + 12, 22, game.player.z + 53},
                                         .rotor = .3f,
                                         .spot = 1};
        }
        if (scene == 2) {
            game.screen = GARAGE;
            game.cursor = 3;
            game.profile.selected = 12;
            game.profile.money = 2600;
        }
        if (scene == 3) {
            game.screen = SHOP;
            game.tab = SHOP_DECALS;
            game.cursor = 3;
            game.profile.selected = 12;
            game.profile.styles[12].wheel = 2;
            game.profile.styles[12].engine = 5;
            game.preview_yaw = .4f;
        }
        if (scene == 4) {
            int node = game.world.spawn;
            for (int i = 0; i < MAP_TILES; i++)
                if (game.world.tiles[i].kind == BRIDGE_EW) {
                    node = i;
                    break;
                }
            position(node, PI / 2);
            game.player.x -= 16;
            game.camera_x -= 16;
            game.actors[13] = (Actor){
                .active = 1,
                .kind = SWAT,
                .model = 4,
                .v = {.x = game.player.x + 22, .z = game.player.z - 3, .yaw = PI / 2, .hp = 200}};
        }
        if (scene == 5) {
            game.screen = CITYMAP;
            game.map_return = PLAY;
            int target = -1;
            for (int n = 0; n < MAP_TILES; n++)
                if (game.chase_field[n] > 12) {
                    target = n;
                    break;
                }
            game_set_waypoint(&game, target);
        }
        if (scene >= 6 && scene <= 9) {
            int family = scene == 6 ? 3 : scene == 7 ? 7 : scene == 9 ? 6 : 5;
            int node = game.world.spawn;
            for (int n = 0; n < MAP_TILES; n++)
                if (game.world.tiles[n].landmark == family) {
                    node = n;
                    break;
                }
            position(node, -PI * .75f);
            game.time = 4;
            game.camera_height = 6;
            game.high_camera = false;
            if (scene == 7) {
                game.player.z += 18;
                game.player.yaw = atan2f(-24, -42);
                game.camera_yaw = game.player.yaw;
                game.camera_x = game.player.x - sinf(game.player.yaw) * 14;
                game.camera_z = game.player.z - cosf(game.player.yaw) * 14;
                game.camera_height = 4;
                game.profile.settings.camera = 0;
            }
            game.run_time = (scene == 8 ? 165 : 260) + 440 - (game.seed % 440);
            if (scene == 8) {
                game.run_time = 165 + 440 - (game.seed % 440);
                for (int n = 0; n < MAP_TILES - 32; n++)
                    if (game.world.tiles[n].kind && game.world.tiles[n].kind < BRIDGE_EW &&
                        !game.world.tiles[n + 32].kind) {
                        position(n, 0);
                        break;
                    }
            }
            if (scene == 9) {
                position(node, 0);
                game.giant = 14;
                game.helper = (Helicopter){
                    .active = true, .p = {game.player.x + 8, 20, game.player.z + 45}, .rotor = .3f};
                game.drops[0] = (SupplyDrop){.active = true,
                                             .p = {game.player.x + 2, 7, game.player.z + 24},
                                             .type = POWER_GIANT,
                                             .life = 25};
            }
        }
        if (scene == 10) {
            game.screen = SETTINGS;
            game.return_screen = TITLE;
            game.settings_cursor = 15;
        }
        if (scene >= 11 && scene <= 22) {
            game.screen = SHOP;
            game.tab = SHOP_CARS;
            game.cursor = 20 + scene - 11;
            game.preview_yaw = .2f;
            game.profile.money = 8000;
        }
        if (scene == 23) {
            int node = game.world.spawn;
            for (int n = 0; n < MAP_TILES; n++)
                if (game.world.tiles[n].landmark == 13) {
                    node = n;
                    break;
                }
            const Building *shop = &game.world.chunks[node].b[0];
            Vec3 center = world_center(&game.world, node);
            position(node,
                     atan2f(shop->x + shop->w * .5f - center.x, shop->z + shop->d + 2 - center.z));
            game.time = 20;
            game.notice_time = 3;
            snprintf(game.notice, sizeof game.notice, "NEON MECHANICS / %s", game.world.name);
        }
        if (scene == 24) {
            game.screen = RESULT;
            game.reason = 1;
            game.drift_score = 3270;
            game.last_payout = 692;
        }
        render_top(&r, &game, 1.0f / 30);
        unsigned total = 0, dropped = 0;
        for (int p = 0; p < PASS_COUNT; p++) {
            total += r.mesh[p].count;
            dropped += r.mesh[p].dropped;
            for (unsigned i = 0; i < r.mesh[p].count; i++) {
                Vertex v = r.mesh[p].v[i];
                assert(isfinite(v.x) && isfinite(v.y) && isfinite(v.z) && isfinite(v.w));
            }
        }
        printf("SCENE %d: %u vertices / %u dropped\n", scene, total, dropped);
        assert(!dropped);
        snprintf(path, sizeof path, "%s_%d_top.ppm", prefix, scene);
        assert(render_ppm(&r, path, THEMES[preview_theme(&game)].sky));
        render_bottom(&r, &game);
        assert(!r.mesh[PASS_UI].dropped);
        snprintf(path, sizeof path, "%s_%d_bottom.ppm", prefix, scene);
        assert(render_ppm(&r, path, (Color){0, 0, 0, 1}));
    }
    game.screen = SHOP;
    for (int tab = 0; tab < SHOP_TABS; tab++)
        for (int item = 0; item < shop_count(tab); item++) {
            game.tab = tab;
            game.cursor = item;
            render_top(&r, &game, 1.0f / 30);
            render_bottom(&r, &game);
            assert(!r.mesh[PASS_UI].dropped);
        }
    game.screen = PLAY;
    for (int n = 0; n < MAP_TILES; n++)
        if (game.world.tiles[n].kind == DOWNTOWN) {
            position(n, PI * .2f);
            break;
        }
    game.profile.settings.view = 5;
    game.profile.settings.bloom = 3;
    render_top(&r, &game, .033f);
    unsigned dropped = 0;
    for (int i = 0; i < PASS_COUNT; i++) {
        assert(r.mesh[i].count <= r.mesh[i].capacity);
        dropped += r.mesh[i].dropped;
    }
    printf("MAX DETAIL: %u dropped vertices (safe bounded degradation)\n", dropped);
    render_free(&r);
}
int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--preview")) {
        preview(argc > 2 ? argv[2] : "preview/coast");
        return 0;
    }
    world_tests();
    save_tests();
    shop_tests();
    driving_tests();
    police_tests();
    contact_tests();
    release_four_tests();
    police_damage_tests();
    visual_independence_tests();
    ambience_tests();
    map_tests();
    puts("ALL V0.5 TESTS PASSED");
    return 0;
}

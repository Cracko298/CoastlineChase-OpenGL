#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static float frand(Game *g) {
    g->fx_rng ^= g->fx_rng << 13;
    g->fx_rng ^= g->fx_rng >> 17;
    g->fx_rng ^= g->fx_rng << 5;
    return (g->fx_rng & 65535) / 65535.0f;
}
void game_notice(Game *g, const char *s) {
    snprintf(g->notice, sizeof g->notice, "%s", s);
    g->notice_time = 3.0f;
}
void game_particle(Game *g, float x, float y, float z, Color c, int count) {
    if (!g->profile.settings.particles)
        return;
    for (int k = 0; k < count; k++) {
        int slot = -1;
        for (int i = 0; i < MAX_PARTICLES; i++)
            if (g->particles[i].life <= 0) {
                slot = i;
                break;
            }
        if (slot < 0)
            return;
        Particle *p = &g->particles[slot];
        p->p = (Vec3){x, y, z};
        p->v = (Vec3){(frand(g) - .5f) * 6, 1 + frand(g) * 4, (frand(g) - .5f) * 6};
        p->life = p->maxlife = .35f + frand(g) * .5f;
        p->color = c;
    }
}
static void reset_player(Game *g) {
    Vec3 p = world_center(&g->world, g->world.spawn);
    g->player = (Vehicle){.x = p.x + 2.8f, .z = p.z, .hp = 100, .nitro = 100};
    g->camera_x = g->player.x;
    g->camera_z = g->player.z - 14;
    g->camera_yaw = 0;
    g->camera_height = 6;
}
void game_init(Game *g, bool n, uint32_t seed) {
    memset(g, 0, sizeof *g);
    g->new3ds = n;
    g->rng = seed ? seed : 12345;
    g->seed = seed;
    g->fx_rng = seed ^ 0xC045123u;
    profile_defaults(&g->profile, n);
    world_init(&g->world, seed);
    reset_player(g);
    g->screen = TITLE;
    g->waypoint = -1;
    g->sprint_node = -1;
    g->last_tile = -1;
    world_stream(&g->world, g->player.x, g->player.z, g->profile.settings.view);
}
void game_start(Game *g) {
    g->seed = random_u32(g);
    g->profile.run_count++;
    g->dirty = true;
    world_init(&g->world, g->seed);
    reset_player(g);
    memset(g->actors, 0, sizeof g->actors);
    memset(g->helis, 0, sizeof g->helis);
    memset(g->people, 0, sizeof g->people);
    memset(g->drops, 0, sizeof g->drops);
    memset(&g->helper, 0, sizeof g->helper);
    g->giant = g->shield = g->magnet = g->people_clock = g->sprint_time = g->combo_time = 0;
    g->property_damage = g->drift_score = g->drift_chain = g->drift_grace = 0;
    g->supply_clock = 28;
    g->supply_serial = g->landmark_bits = g->landmark_count = g->sprint_count = g->combo = 0;
    g->sprint_node = -1;
    memset(g->particles, 0, sizeof g->particles);
    memset(g->pickups, 0, sizeof g->pickups);
    memset(g->collected, 0, sizeof g->collected);
    memset(g->visited, 0, sizeof g->visited);
    g->time = g->run_time = g->distance = g->drift_distance = g->heat = g->bust = g->hit_cool =
        g->spawn_clock = g->recover_time = g->nav_clock = 0;
    g->earned = g->escapes = g->near_misses = g->peak_heat = g->mission_bits = g->last_payout =
        g->wave = g->takedowns = g->explored = 0;
    g->waypoint = g->last_tile = -1;
    g->shake = g->flash = 0;
    g->boosting = g->drifting = g->rear = g->confirm_purchase = false;
    g->screen = PLAY;
    g->cursor = 0;
    world_distances(&g->world, g->world.spawn, g->chase_field);
    game_notice(g, g->world.name);
}
void game_end(Game *g, int reason) {
    if (g->screen == RESULT)
        return;
    g->reason = reason;
    g->last_payout =
        g->earned + (int)(g->drift_score / 10) + (int)(g->distance * .045f) + (int)g->run_time / 2;
    if (g->last_payout < 0)
        g->last_payout = 0;
    uint32_t cash = (uint32_t)g->last_payout;
    g->profile.money =
        g->profile.money > 1000000000u - cash ? 1000000000u : g->profile.money + cash;
    if ((uint32_t)g->distance > g->profile.best)
        g->profile.best = (uint32_t)g->distance;
    g->dirty = true;
    g->screen = RESULT;
    g->cursor = 0;
    g->boosting = false;
}
void game_set_waypoint(Game *g, int n) {
    if (n < 0 || n >= MAP_TILES || !g->world.tiles[n].kind)
        return;
    g->waypoint = n;
    world_distances(&g->world, n, g->gps_field);
    game_notice(g, "ROUTE SET / FOLLOW THE RADAR LINE");
}
static void pickups_step(Game *g) {
    int cx = (int)floorf(g->player.x / CELL), cz = (int)floorf(g->player.z / CELL), index = 0;
    for (int j = -2; j <= 2; j++)
        for (int i = -2; i <= 2; i++) {
            Pickup *p = &g->pickups[index++];
            p->active = false;
            p->x = cx + i;
            p->z = cz + j;
            const Tile *t = world_tile(&g->world, p->x, p->z);
            if (!t->kind || !t->roads)
                continue;
            p->node = p->z * MAP_SIDE + p->x;
            if (g->collected[p->node])
                continue;
            uint32_t h = hash_cell(p->x, p->z, g->seed + 803);
            p->key = h;
            int d = h % 4;
            for (int k = 0; k < 4; k++)
                if (t->roads & (1 << ((d + k) % 4))) {
                    d = (d + k) % 4;
                    break;
                }
            Vec3 center = world_center(&g->world, p->node);
            p->px = center.x + ROAD_DX[d] * 17 + ROAD_DZ[d] * 2.8f;
            p->pz = center.z + ROAD_DZ[d] * 17 - ROAD_DX[d] * 2.8f;
            p->type = h % 9 == 0 ? 1 : h % 7 == 0 ? 2 : 0;
            p->active = true;
            if (hypotf(g->player.x - p->px, g->player.z - p->pz) <
                (g->magnet > 0 ? 20 : 3.8f * player_scale(g))) {
                g->collected[p->node] = 1;
                p->active = false;
                g->audio_events |= 2;
                if (p->type == 1) {
                    g->player.hp = fminf(100, g->player.hp + 30);
                    game_notice(g, "REPAIR / +30 HULL");
                } else if (p->type == 2) {
                    g->player.nitro = 100;
                    game_notice(g, "NITRO REFILLED");
                } else {
                    g->earned += 25 + (int)g->heat * 5;
                    game_notice(g, "CASH STASH COLLECTED");
                }
                game_particle(g, p->px, 1, p->pz, PAINTS[p->type == 1 ? 3 : 2], 10);
            }
        }
}
static void contracts(Game *g) {
    bool done[] = {g->distance >= 1500, g->drift_distance >= 120, g->run_time >= 180,
                   g->explored >= 12};
    const char *msg[] = {"ROAD TRIP CONTRACT +180", "DRIFT CONTRACT +200", "SURVIVOR CONTRACT +250",
                         "EXPLORER CONTRACT +160"};
    int reward[] = {180, 200, 250, 160};
    for (int i = 0; i < 4; i++)
        if (done[i] && !(g->mission_bits & (1 << i))) {
            g->mission_bits |= 1 << i;
            g->earned += reward[i];
            g->audio_events |= 2;
            game_notice(g, msg[i]);
        }
}
void game_drift_score(Game *g, float traveled, float dt) {
    float slip = fabsf(g->player.vx * cosf(g->player.yaw) - g->player.vz * sinf(g->player.yaw));
    if (g->drifting && slip > 1.5f && traveled > .001f) {
        g->drift_chain += dt;
        g->drift_grace = 1.2f;
        g->drift_distance += traveled;
        float mult = 1 + fminf(3, floorf(g->drift_chain / 2));
        g->drift_score += traveled * 5 * mult;
    } else {
        g->drift_grace = fmaxf(0, g->drift_grace - dt);
        if (g->drift_grace <= 0)
            g->drift_chain = 0;
    }
}
void game_update(Game *g, Input in, float dt) {
    dt = clampf(dt, 0, .05f);
    g->time += dt;
    g->notice_time = fmaxf(0, g->notice_time - dt);
    if (g->screen != PLAY) {
        menus_step(g, in, dt);
        return;
    }
    if (in.pressed & IN_START) {
        g->screen = PAUSE;
        g->cursor = 0;
        return;
    }
    if (in.pressed & IN_SELECT) {
        g->return_screen = PAUSE;
        g->old_cursor = 2;
        g->screen = SETTINGS;
        return;
    }
    if ((in.pressed & IN_MAP) || (in.touch && in.touch_x < 162 && in.touch_y >= 45 && in.touch_y < 198)) {
        g->map_return = PLAY;
        g->screen = CITYMAP;
        return;
    }
    g->run_time += dt;
    ambience_step(g, dt);
    g->hit_cool = fmaxf(0, g->hit_cool - dt);
    g->shake = fmaxf(0, g->shake - dt * 1.6f);
    g->flash = fmaxf(0, g->flash - dt);
    float steer = in.steer;
    if (in.held & IN_LEFT)
        steer = -1;
    if (in.held & IN_RIGHT)
        steer = 1;
    g->drifting = (in.held & IN_R) && vehicle_speed(&g->player) > 7;
    g->boosting = (in.held & IN_L) && (in.held & IN_A) && g->player.nitro > 0;
    int engine = g->profile.styles[g->profile.selected].engine;
    g->player.nitro =
        clampf(g->player.nitro +
                   dt * (g->boosting ? -(23 - g->profile.upgrades[g->profile.selected][3] * 1.8f)
                                     : (engine == 1 ? 13 : 10)),
               0, 100);
    if (in.pressed & IN_X)
        g->high_camera = !g->high_camera;
    if (in.held & IN_Y)
        g->recover_time += dt;
    else
        g->recover_time = 0;
    if (g->recover_time > 1.5f) {
        Vec3 p = world_center(&g->world, world_nearest(&g->world, g->player.x, g->player.z));
        g->player.x = p.x + 2.8f;
        g->player.z = p.z;
        g->player.vx = g->player.vz = g->player.yaw = 0;
        g->recover_time = 0;
        game_notice(g, "RECOVERED / POLICE STILL PURSUING");
    }
    float x = g->player.x, z = g->player.z;
    drive_step(g, &g->player, (in.held & IN_A) ? 1 : 0, (in.held & IN_B) ? 1 : 0, steer,
               g->drifting, g->boosting, g->profile.selected, dt);
    float traveled = hypotf(g->player.x - x, g->player.z - z);
    g->distance += traveled;
    game_drift_score(g, traveled, dt);
    if (g->drifting && fabsf(steer) > .2f) {
        if ((int)(g->run_time * 16) != (int)((g->run_time - dt) * 16))
            game_particle(g, g->player.x, .1f, g->player.z, (Color){.4f, .6f, .8f, 1}, 1);
    }
    if (g->boosting && (int)(g->run_time * 20) != (int)((g->run_time - dt) * 20))
        game_particle(g, g->player.x - sinf(g->player.yaw) * 2, .4f,
                      g->player.z - cosf(g->player.yaw) * 2, PAINTS[0], 1);
    int wave = g->run_time < 20 ? 0 : 1 + (int)((g->run_time - 20) / 45);
    if (wave > g->wave) {
        g->wave = wave;
        g->earned += wave > 1 ? 35 : 0;
        char text[64];
        snprintf(text, sizeof text, "DISPATCH WAVE %d / %d GROUND UNITS", wave,
                 game_police_budget(game_pressure(g)));
        game_notice(g, text);
    }
    g->heat = fminf(5, game_police_budget(game_pressure(g)) / 4.f);
    g->peak_heat = (int)g->heat;
    police_step(g, dt);
    if (g->screen != PLAY)
        return;
    float impact;
    vehicle_resolve(g, &g->player, g->profile.selected, &impact);
    pickups_step(g);
    int node = world_node(g->player.x, g->player.z);
    if (node >= 0 && !g->visited[node]) {
        g->visited[node] = 1;
        g->explored++;
        g->earned += 3;
    }
    if (node != g->last_tile) {
        g->last_tile = node;
        if (node >= 0 && g->notice_time <= 0)
            game_notice(g, g->world.tiles[node].landmark
                               ? landmark_name(g->world.tiles[node].landmark)
                               : district_name(g->world.tiles[node].kind));
    }
    if (node == g->waypoint) {
        g->waypoint = -1;
        game_notice(g, "DESTINATION REACHED");
    }
    if (node >= 0) {
        int landmark = g->world.tiles[node].landmark;
        if (landmark && !(g->landmark_bits & (1 << landmark))) {
            g->landmark_bits |= 1 << landmark;
            g->landmark_count++;
            g->earned += 45;
            game_notice(g, "LANDMARK DISCOVERED / +45");
        }
    }
    contracts(g);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (p->life <= 0)
            continue;
        p->life -= dt;
        p->p.x += p->v.x * dt;
        p->p.y += p->v.y * dt;
        p->p.z += p->v.z * dt;
        p->v.y -= 12 * dt;
    }
    world_stream(&g->world, g->player.x, g->player.z, g->profile.settings.view);
    if (g->player.hp <= 0)
        game_end(g, 0);
}

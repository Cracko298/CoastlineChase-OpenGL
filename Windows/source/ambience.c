#include "game.h"
#include <math.h>
#include <stdio.h>
float player_scale(const Game *g) {
    return g->giant > 0 ? 1.85f : 1;
}
float player_radius(const Game *g) {
    return CARS[g->profile.selected].width * .52f * player_scale(g);
}
void supply_collect(Game *g, int type) {
    if (type == POWER_GIANT) {
        g->giant = 14;
        game_notice(g, "MEGA RIDE / BIG CAR FOR 14 SECONDS");
    } else if (type == POWER_SHIELD) {
        g->shield = 18;
        game_notice(g, "BUBBLE SHIELD / 18 SECONDS");
    } else if (type == POWER_MAGNET) {
        g->magnet = 22;
        game_notice(g, "CASH MAGNET / 22 SECONDS");
    } else {
        g->player.hp = fminf(100, g->player.hp + 40);
        g->player.nitro = 100;
        game_notice(g, "PIT CREW / REPAIR AND NITRO");
    }
    g->audio_events |= 2;
}
static uint32_t prand(Pedestrian *p) {
    p->rng ^= p->rng << 13;
    p->rng ^= p->rng >> 17;
    p->rng ^= p->rng << 5;
    return p->rng;
}
static void pedestrian_goal(Game *g, Pedestrian *p) {
    int choices[4], count = 0, previous = p->target;
    for (int d = 0; d < 4; d++) {
        int n = world_neighbor(&g->world, p->node, d);
        if (n >= 0 && n != previous)
            choices[count++] = n;
    }
    if (!count && previous >= 0)
        choices[count++] = previous;
    if (!count) {
        p->active = false;
        return;
    }
    p->target = choices[prand(p) % count];
    Vec3 a = world_center(&g->world, p->node), b = world_center(&g->world, p->target);
    float dx = b.x - a.x, dz = b.z - a.z, len = hypotf(dx, dz),
          side = p->side * (g->world.tiles[p->node].kind >= BRIDGE_EW ||
                                    g->world.tiles[p->target].kind >= BRIDGE_EW
                                ? 7.0f
                                : 10.5f);
    p->tx = a.x + dz / len * side;
    p->tz = a.z - dx / len * side;
    p->stage = 0;
    p->yaw = atan2f(dx, dz);
}
static void pedestrians(Game *g, float dt) {
    for (int i = 0; i < MAX_PEOPLE; i++) {
        Pedestrian *p = &g->people[i];
        if (p->active && hypotf(p->x - g->player.x, p->z - g->player.z) > 420)
            p->active = false;
        if (!p->active) {
            if (!p->rng)
                p->rng = hash_cell(i, 12, g->seed) | 1u;
            for (int k = 0; k < 8; k++) {
                int n = prand(p) % MAP_TILES;
                if (!g->world.tiles[n].roads || g->world.tiles[n].kind >= BRIDGE_EW)
                    continue;
                Vec3 c = world_center(&g->world, n);
                if (hypotf(c.x - g->player.x, c.z - g->player.z) > 185)
                    continue;
                p->node = n;
                p->target = -1;
                p->side = (i & 1) ? 1 : -1;
                p->active = true;
                pedestrian_goal(g, p);
                p->x = p->tx;
                p->z = p->tz;
                break;
            }
            continue;
        }
        if (p->pause > 0) {
            p->pause -= dt;
            continue;
        }
        float dx = p->tx - p->x, dz = p->tz - p->z, len = hypotf(dx, dz);
        if (len < .3f) {
            if (p->stage == 0) {
                Vec3 a = world_center(&g->world, p->node), b = world_center(&g->world, p->target);
                p->tx += b.x - a.x;
                p->tz += b.z - a.z;
                p->stage = 1;
            } else {
                int old = p->node;
                p->node = p->target;
                p->target = old;
                p->steps++;
                pedestrian_goal(g, p);
                if (prand(p) % 4 == 0)
                    p->pause = .5f + (prand(p) % 20) * .1f;
            }
            continue;
        }

        bool yield = false;
        if (hypotf(p->x - g->player.x, p->z - g->player.z) < 6 && vehicle_speed(&g->player) > 2)
            yield = true;
        for (int a = 0; a < MAX_ACTORS && !yield; a++)
            if (g->actors[a].active && hypotf(p->x - g->actors[a].v.x, p->z - g->actors[a].v.z) < 4)
                yield = true;
        if (yield)
            continue;
        float move = fminf(len, dt * (1.15f + (i % 5) * .13f));
        float nx = p->x + dx / len * move, nz = p->z + dz / len * move;
        if (world_solid(&g->world, nx, nz, .25f)) {
            Vec3 c = world_center(&g->world, p->node);
            p->tx = c.x;
            p->tz = c.z;
            p->stage = 0;
            continue;
        }
        p->x = nx;
        p->z = nz;
        p->yaw = atan2f(dx, dz);
        p->walk += move * 5;
    }
}
void ambience_step(Game *g, float dt) {
    g->giant = fmaxf(0, g->giant - dt);
    g->shield = fmaxf(0, g->shield - dt);
    g->magnet = fmaxf(0, g->magnet - dt);
    g->combo_time = fmaxf(0, g->combo_time - dt);
    if (g->combo_time == 0)
        g->combo = 0;
    g->people_clock += dt;
    if (g->people_clock >= .05f) {
        pedestrians(g, g->people_clock);
        g->people_clock = 0;
    }
    g->supply_clock -= dt;
    int node = world_nearest(&g->world, g->player.x, g->player.z), ahead = node;
    float best = -1e9f;
    for (int d = 0; d < 4; d++) {
        int next = world_neighbor(&g->world, node, d);
        if (next < 0)
            continue;
        float score = ROAD_DX[d] * sinf(g->player.yaw) + ROAD_DZ[d] * cosf(g->player.yaw);
        if (score > best) {
            best = score;
            ahead = next;
        }
    }
    Vec3 goal = world_center(&g->world, ahead);
    Helicopter *h = &g->helper;
    h->active = g->supply_clock < 12;
    for (int i = 0; i < MAX_DROPS; i++)
        if (g->drops[i].active && g->drops[i].p.y > 15)
            h->active = true;
    if (h->active) {
        if (h->p.y == 0)
            h->p = (Vec3){goal.x + 75, 38, goal.z + 75};
        float dx = goal.x - h->p.x, dz = goal.z - h->p.z, len = hypotf(dx, dz),
              m = fminf(len, dt * 32);
        if (len > .01f) {
            h->p.x += dx / len * m;
            h->p.z += dz / len * m;
            h->yaw = atan2f(dx, dz);
        }
        h->rotor = fmodf(h->rotor + dt * 32, 2 * PI);
        h->spot = 0;
    }
    if (g->supply_clock <= 0) {
        for (int i = 0; i < MAX_DROPS; i++)
            if (!g->drops[i].active) {
                SupplyDrop *d = &g->drops[i];
                d->active = true;
                d->p = goal;
                d->p.y = 22;
                d->life = 38;
                d->type = g->supply_serial++ % 4;
                h->p = (Vec3){goal.x, 28, goal.z};
                h->active = true;
                game_notice(g, "FRIENDLY AIRDROP / FOLLOW THE GREEN CRATE");
                break;
            }
        g->supply_clock = 42 + (hash_cell(g->supply_serial, 23, g->seed) % 12);
    }
    for (int i = 0; i < MAX_DROPS; i++) {
        SupplyDrop *d = &g->drops[i];
        if (!d->active)
            continue;
        d->p.y = fmaxf(.8f, d->p.y - dt * 4.5f);
        d->life -= dt;
        if (d->life <= 0)
            d->active = false;
        else if (d->p.y < 2 &&
                 hypotf(d->p.x - g->player.x, d->p.z - g->player.z) < 4.5f * player_scale(g)) {
            supply_collect(g, d->type);
            d->active = false;
        }
    }

    if (g->run_time > 40 && g->sprint_node < 0) {
        for (int k = 0; k < MAP_TILES; k++) {
            int n = (k * 37 + (int)(g->seed % MAP_TILES) + g->sprint_count * 17) % MAP_TILES;
            if (g->chase_field[n] >= 4 && g->chase_field[n] <= 7) {
                g->sprint_node = n;
                g->sprint_time = 65;
                break;
            }
        }
    }
    if (g->sprint_node >= 0) {
        g->sprint_time -= dt;
        if (node == g->sprint_node) {
            g->earned += 150;
            g->sprint_count++;
            g->sprint_node = -1;
            game_notice(g, "CITY SPRINT COMPLETE / +150");
        } else if (g->sprint_time <= 0) {
            g->sprint_count++;
            g->sprint_node = -1;
        }
    }
}

#include "game.h"
#include <math.h>
#include <string.h>
static float d2(float x, float z, float a, float b) {
    float dx = x - a, dz = z - b;
    return dx * dx + dz * dz;
}
float game_pressure(const Game *g) {

    return g->run_time + fminf(600, g->property_damage * .35f);
}
int game_police_budget(float t) {
    return t < 20 ? 0 : (int)clampf(2 + floorf((t - 20) / 25), 2, MAX_POLICE);
}
int game_heli_budget(float t) {
    return t < 90 ? 0 : t < 210 ? 1 : t < 360 ? 2 : 3;
}
static bool clear(Game *g, float x, float z, float xx, float zz) {
    float length = hypotf(xx - x, zz - z);
    int steps = (int)(length / 2) + 1;
    if (steps > 80)
        steps = 80;
    for (int i = 1; i <= steps; i++) {
        float t = i / (float)steps;
        if (world_solid(&g->world, x + (xx - x) * t, z + (zz - z) * t, 1.3f))
            return false;
    }
    return true;
}
static int next_dir(Game *g, Actor *a, int node) {
    int best = -1;
    float score = 1e30f;
    for (int d = 0; d < 4; d++) {
        int next = world_neighbor(&g->world, node, d);
        if (next < 0)
            continue;
        float s;
        if (a->kind) {
            int n = g->chase_field[next];
            if (n < 0)
                continue;
            s = n * 100 + (d == (a->dir + 2) % 4 ? 4 : 0) +
                (a->kind == INTERCEPTOR ? (float)(hash_cell(next, a->model, g->seed) % 9) : 0);
        } else {
            s = random_u32(g) % 100;
            if (d == (a->dir + 2) % 4)
                s += 200;
            if (d == a->dir)
                s -= 25;
        }
        if (s < score) {
            score = s;
            best = d;
        }
    }
    return best;
}
static void waypoint(Game *g, Actor *a, int node) {
    int d = next_dir(g, a, node);
    a->node = node;
    a->dir = d < 0 ? a->dir : d;
    a->target = d < 0 ? node : world_neighbor(&g->world, node, d);
    Vec3 p = world_center(&g->world, a->target);
    a->tx = p.x + ROAD_DZ[a->dir] * 2.8f;
    a->tz = p.z - ROAD_DX[a->dir] * 2.8f;
}
static bool spawn(Game *g, int index, int kind) {
    Actor *a = &g->actors[index];
    for (int tries = 0; tries < 180; tries++) {
        int node = (int)(random_u32(g) % MAP_TILES);
        if (!g->world.tiles[node].roads)
            continue;
        Vec3 p = world_center(&g->world, node);
        float distance = d2(p.x, p.z, g->player.x, g->player.z);
        if (distance < 95 * 95 || distance > 260 * 260)
            continue;
        float ahead =
            (p.x - g->player.x) * sinf(g->player.yaw) + (p.z - g->player.z) * cosf(g->player.yaw);
        if (ahead > 0 && distance < 180 * 180)
            continue;
        bool occupied = false;
        for (int j = 0; j < MAX_ACTORS; j++)
            if (g->actors[j].active && d2(p.x, p.z, g->actors[j].v.x, g->actors[j].v.z) < 12 * 12)
                occupied = true;
        if (occupied)
            continue;
        memset(a, 0, sizeof *a);
        a->active = 1;
        a->kind = kind;
        static const int patrol[] = {5, 10, 11, 16};
        static const int suv[] = {7, 13, 4};
        static const int swat[] = {4, 18, 7};
        static const int pursuit[] = {6, 19, 2, 3};
        a->variant = random_u32(g) % 4;
        a->model = kind == PATROL        ? patrol[a->variant]
                   : kind == POLICE_SUV  ? suv[a->variant % 3]
                   : kind == SWAT        ? swat[a->variant % 3]
                   : kind == INTERCEPTOR ? pursuit[a->variant]
                                         : (int)(random_u32(g) % 20);
        a->v = (Vehicle){.x = p.x,
                         .z = p.z,
                         .hp = kind == SWAT         ? 200
                               : kind == POLICE_SUV ? 150
                                                    : 100};
        waypoint(g, a, node);
        a->v.yaw = atan2f(ROAD_DX[a->dir], ROAD_DZ[a->dir]);
        a->v.x += ROAD_DZ[a->dir] * 2.8f;
        a->v.z -= ROAD_DX[a->dir] * 2.8f;
        a->v.vx = ROAD_DX[a->dir] * 8;
        a->v.vz = ROAD_DZ[a->dir] * 8;
        return true;
    }
    return false;
}
static int unit_kind(float time, int slot) {
    if (time > 240 && slot % 5 == 4)
        return INTERCEPTOR;
    if (time > 150 && slot % 4 == 3)
        return SWAT;
    if (time > 65 && slot % 3 == 2)
        return POLICE_SUV;
    return PATROL;
}
static void helicopter_step(Game *g, float dt) {
    int count = game_heli_budget(game_pressure(g));
    for (int i = 0; i < count; i++) {
        Helicopter *h = &g->helis[i];
        float a = g->player.yaw + .3f + sinf(g->run_time * .1f + i * 2) * .65f;
        float tx = g->player.x + sinf(a) * (55 + i * 8), tz = g->player.z + cosf(a) * (55 + i * 8);
        if (!h->active) {
            h->active = true;
            h->p = (Vec3){tx + 90, 55, tz + 90};
            game_notice(g, "AIR SUPPORT INBOUND");
        }
        float dx = tx - h->p.x, dz = tz - h->p.z, len = hypotf(dx, dz), move = fminf(len, dt * 27);
        if (len > .001f) {
            h->p.x += dx / len * move;
            h->p.z += dz / len * move;
            h->yaw = atan2f(dx, dz);
        }
        h->think -= dt;
        if (h->think <= 0) {
            h->think = .25f;
            float altitude = 27;
            int cx = (int)floorf(h->p.x / CELL), cz = (int)floorf(h->p.z / CELL);
            for (int z = cz - 1; z <= cz + 1; z++)
                for (int x = cx - 1; x <= cx + 1; x++) {
                    const Chunk *c = world_chunk(&g->world, x, z);
                    for (int b = 0; b < c->count; b++)
                        if (d2(h->p.x, h->p.z, c->b[b].x + c->b[b].w / 2,
                               c->b[b].z + c->b[b].d / 2) < 28 * 28)
                            altitude = fmaxf(altitude, c->b[b].h + 12);
                }
            h->altitude = altitude;
        }
        h->p.y += (h->altitude - h->p.y) * clampf(dt * 1.5f, 0, 1);
        h->rotor = fmodf(h->rotor + dt * 32, 2 * PI);
        h->spot = d2(h->p.x, h->p.z, g->player.x, g->player.z) < 90 * 90 ? 1 : 0;
    }
}
void police_step(Game *g, float dt) {
    g->nav_clock -= dt;
    if (g->nav_clock <= 0) {
        g->nav_clock = .45f;
        int target = world_nearest(&g->world, g->player.x, g->player.z);
        if (g->chase_field[target] != 0)
            world_distances(&g->world, target, g->chase_field);
    }
    int traffic = 9, cops = game_police_budget(game_pressure(g));
    g->spawn_clock -= dt;
    if (g->spawn_clock <= 0) {
        g->spawn_clock = game_heli_budget(game_pressure(g)) ? .75f : 1.05f;
        bool added = false;
        for (int i = 0; i < cops; i++)
            if (!g->actors[MAX_TRAFFIC + i].active) {
                added = spawn(g, MAX_TRAFFIC + i, unit_kind(game_pressure(g), i));
                break;
            }
        if (!added)
            for (int i = 0; i < traffic; i++)
                if (!g->actors[i].active) {
                    spawn(g, i, CIVILIAN);
                    break;
                }
    }
    helicopter_step(g, dt);
    for (int i = 0; i < MAX_ACTORS; i++) {
        Actor *a = &g->actors[i];
        if (!a->active)
            continue;
        if (!a->kind &&
            (i >= traffic || d2(a->v.x, a->v.z, g->player.x, g->player.z) > 420 * 420)) {
            a->active = 0;
            continue;
        }
        a->age += dt;
        a->hit_cool = fmaxf(0, a->hit_cool - dt);
        float distance = d2(a->v.x, a->v.z, g->player.x, g->player.z),
              to_goal = hypotf(a->tx - a->v.x, a->tz - a->v.z);
        if (to_goal < 6) {
            waypoint(g, a, a->target);
            to_goal = hypotf(a->tx - a->v.x, a->tz - a->v.z);
        }
        float speed = vehicle_speed(&a->v);
        a->think -= dt;
        a->commit = fmaxf(0, a->commit - dt);
        bool waiting_signal = false;
        if (a->think <= 0) {
            a->think = .10f + (i % 3) * .012f;
            float tx = a->tx, tz = a->tz;
            bool intercept =
                a->kind && distance < 66 * 66 && clear(g, a->v.x, a->v.z, g->player.x, g->player.z);
            if (intercept) {
                tx = g->player.x + g->player.vx * (a->kind == INTERCEPTOR ? .8f : .5f);
                tz = g->player.z + g->player.vz * (a->kind == INTERCEPTOR ? .8f : .5f);
            }
            if (intercept && a->commit <= 0 && distance > 12 * 12 && distance < 55 * 55 &&
                fmodf(a->age + i * 1.7f, 8) < .15f) {
                int charging = 0;
                for (int j = MAX_TRAFFIC; j < MAX_ACTORS; j++)
                    if (g->actors[j].commit > 0)
                        charging++;
                if (charging < 2) {
                    a->commit = 1.9f;
                    a->charge_x = tx;
                    a->charge_z = tz;
                }
            }
            if (a->commit > 0) {
                tx = a->charge_x;
                tz = a->charge_z;
            }
            float turn = angle_delta(atan2f(tx - a->v.x, tz - a->v.z), a->v.yaw),
                  target_speed = a->kind ? (a->kind == SWAT          ? 17
                                            : a->kind == POLICE_SUV  ? 20
                                            : a->kind == INTERCEPTOR ? 24
                                                                     : 23)
                                         : (9 + i % 4);
            target_speed *= 1 - clampf(fabsf(turn) * .65f, 0, .77f);
            if (!intercept && to_goal < 22)
                target_speed = fminf(target_speed, 7 + to_goal * .25f);
            int phase =
                ((int)(g->run_time / 8) + (a->target % MAP_SIDE + a->target / MAP_SIDE) % 3) & 1;
            waiting_signal =
                !a->kind && to_goal < 22 && to_goal > 7 && phase == ((a->dir & 1) ? 0 : 1);
            if (waiting_signal)
                target_speed = 0;
            float steer = clampf(turn * 2.8f, -1, 1);
            float look = fmaxf(4, speed * .55f);
            if (world_solid(&g->world, a->v.x + sinf(a->v.yaw) * look,
                            a->v.z + cosf(a->v.yaw) * look, 1.5f)) {
                float left = a->v.yaw - .7f, right = a->v.yaw + .7f;
                bool l =
                    !world_solid(&g->world, a->v.x + sinf(left) * 5, a->v.z + cosf(left) * 5, 1.3f);
                bool r = !world_solid(&g->world, a->v.x + sinf(right) * 5, a->v.z + cosf(right) * 5,
                                      1.3f);
                if (l != r)
                    steer = l ? -1 : 1;
                target_speed = fminf(target_speed, 5);
            }
            if (a->commit > 1.15f)
                target_speed = fminf(target_speed, 9);
            else if (a->commit > 0)
                target_speed += 3;
            a->control = steer;
            a->desired = target_speed;
            a->direct = intercept;
        }
        waiting_signal = !a->kind && a->desired == 0;
        float steer = a->control, target_speed = a->desired;
        if (speed < 1.2f && !waiting_signal)
            a->stuck += dt;
        else
            a->stuck = fmaxf(0, a->stuck - dt);
        if (a->stuck > 2 && a->recovery <= 0) {
            a->recovery = 1.7f;
            a->stuck = 0;
            int node = world_nearest(&g->world, a->v.x, a->v.z);
            Vec3 center = world_center(&g->world, node);
            a->target = node;
            a->tx = center.x;
            a->tz = center.z;
        }
        if (a->recovery > 0) {
            a->recovery -= dt;
            drive_step(g, &a->v, 0, 1, -steer, false, false, a->model, dt);
        } else
            drive_step(g, &a->v, speed < target_speed ? 1 : 0, speed > target_speed + 1 ? .65f : 0,
                       steer, false, false, a->model, dt);
        float dx = g->player.x - a->v.x, dz = g->player.z - a->v.z;
        distance = dx * dx + dz * dz;
        float radius =
            (CARS[g->profile.selected].length * player_scale(g) + CARS[a->model].length) * .55f;
        if (distance < radius * radius) {
            vehicle_contact(g, a);
        } else if (distance < 6 * 6 && !a->passed && vehicle_speed(&g->player) > 12) {
            a->passed = 1;
            g->near_misses++;
            g->earned += 10;
            game_notice(g, "CLOSE CALL +10");
            g->audio_events |= 2;
        }
    }

    for (int i = 0; i < MAX_ACTORS; i++)
        if (g->actors[i].active)
            for (int j = i + 1; j < MAX_ACTORS; j++)
                if (g->actors[j].active) {
                    Vehicle *a = &g->actors[i].v, *b = &g->actors[j].v;
                    float dx = a->x - b->x, dz = a->z - b->z, dist = dx * dx + dz * dz;
                    if (dist > 8 || dist < .00001f)
                        continue;
                    float length = sqrtf(dist), push = (2.85f - length) * .5f;
                    dx /= length;
                    dz /= length;
                    a->x += dx * push;
                    a->z += dz * push;
                    b->x -= dx * push;
                    b->z -= dz * push;
                }
    for (int i = 0; i < MAX_ACTORS; i++)
        if (g->actors[i].active) {
            float impact;
            vehicle_resolve(g, &g->actors[i].v, g->actors[i].model, &impact);
        }
    game_arrest_step(g, dt);
}
void game_arrest_step(Game *g, float dt) {
    bool nearby = false;
    if (vehicle_speed(&g->player) < 2.4f && g->shield <= 0 && g->giant <= 0)
        for (int i = 0; i < MAX_ACTORS; i++) {
            const Actor *a = &g->actors[i];
            if (a->active && a->kind && d2(a->v.x, a->v.z, g->player.x, g->player.z) < 7 * 7 &&
                clear(g, a->v.x, a->v.z, g->player.x, g->player.z)) {
                nearby = true;
                break;
            }
        }
    g->bust = nearby ? g->bust + dt : fmaxf(0, g->bust - dt * 1.3f);
    if (g->bust >= 5)
        game_end(g, 1);
}

static void hull_delta(const Game *g, const Actor *actor, float *dx, float *dz) {
    const Vehicle *a = &g->player, *b = &actor->v;
    float ah =
        fmaxf(0, CARS[g->profile.selected].length * .5f - CARS[g->profile.selected].width * .55f) *
        player_scale(g);
    float bh = fmaxf(0, CARS[actor->model].length * .5f - CARS[actor->model].width * .55f);
    float ax = sinf(a->yaw) * ah, az = cosf(a->yaw) * ah, bx = sinf(b->yaw) * bh,
          bz = cosf(b->yaw) * bh;
    float ux = ax * 2, uz = az * 2, vx = bx * 2, vz = bz * 2;
    float rx = a->x - ax - (b->x - bx), rz = a->z - az - (b->z - bz);
    float aa = ux * ux + uz * uz, ee = vx * vx + vz * vz, ff = vx * rx + vz * rz, ss = 0, tt = 0;
    if (aa < .00001f)
        tt = ee > .00001f ? clampf(ff / ee, 0, 1) : 0;
    else {
        float cc = ux * rx + uz * rz;
        if (ee < .00001f)
            ss = clampf(-cc / aa, 0, 1);
        else {
            float bb = ux * vx + uz * vz, den = aa * ee - bb * bb;
            ss = den > .00001f ? clampf((bb * ff - cc * ee) / den, 0, 1) : 0;
            tt = (bb * ss + ff) / ee;
            if (tt < 0) {
                tt = 0;
                ss = clampf(-cc / aa, 0, 1);
            } else if (tt > 1) {
                tt = 1;
                ss = clampf((bb - cc) / aa, 0, 1);
            }
        }
    }
    *dx = rx + ux * ss - vx * tt;
    *dz = rz + uz * ss - vz * tt;
}

void vehicle_contact(Game *g, Actor *a) {
    float dx, dz;
    hull_delta(g, a, &dx, &dz);
    float len = hypotf(dx, dz), radius = player_radius(g) + CARS[a->model].width * .55f;
    if (len >= radius)
        return;
    float overlap = radius - len;
    if (len < .001f) {
        dx = 1;
        dz = 0;
    } else {
        dx /= len;
        dz /= len;
    }
    float player_speed = vehicle_speed(&g->player), cpu_speed = vehicle_speed(&a->v);
    float player_approach = -(g->player.vx * dx + g->player.vz * dz);
    float cpu_approach = a->v.vx * dx + a->v.vz * dz;
    bool player_hit = player_speed > cpu_speed + .05f && player_approach > 2 &&
                      player_approach >= cpu_approach;
    bool police_hit = a->kind != CIVILIAN && cpu_approach > 2 &&
                      cpu_approach > player_approach;
    float rel = (g->player.vx - a->v.vx) * dx + (g->player.vz - a->v.vz) * dz;
    float share = g->giant > 0 ? .17f : a->kind == SWAT ? .60f : .45f;
    g->player.x += dx * overlap * share;
    g->player.z += dz * overlap * share;
    a->v.x -= dx * overlap * (1 - share);
    a->v.z -= dz * overlap * (1 - share);
    if (rel < 0) {
        float impulse = fminf(18, -rel * .65f);
        g->player.vx += dx * impulse * share * 1.6f;
        g->player.vz += dz * impulse * share * 1.6f;
        a->v.vx -= dx * impulse * (1 - share) * 1.6f;
        a->v.vz -= dz * impulse * (1 - share) * 1.6f;
        if (a->kind && a->commit > 0) {
            a->recovery = .7f;
            a->commit = 0;
        }
        if ((player_hit || police_hit) && -rel > 3) {
            if (g->hit_cool <= 0 && g->giant <= 0 && g->shield <= 0) {
                CarSpec c = car_tuned(&g->profile, g->profile.selected, -1, -1);

                float damage = police_hit ? clampf((fminf(cpu_speed, -rel) - 3) * .32f * 100 / c.armor, 0, 7)
                                          : clampf((-rel - 3) * 70 / c.armor, 0, 12);
                g->player.hp -= damage;
                g->hit_cool = 1.0f;
                g->flash = .08f;
            }
            if (player_hit && a->hit_cool <= 0) {
                float loss = fminf(fmaxf(0, a->v.hp), (-rel) * (g->giant > 0 ? 12 : 5));
                a->v.hp -= loss;
                g->property_damage += loss;
                a->hit_cool = .7f;
            }
            g->shake = .22f;
            g->audio_events |= 1;
            game_particle(g, g->player.x, .7f, g->player.z, PAINTS[2], 8);
        }
    }
    if (a->v.hp <= 0 && a->active) {
        a->active = 0;
        if (a->kind) {
            g->takedowns++;
            g->combo = g->combo < 5 ? g->combo + 1 : 5;
            g->combo_time = 9;
            g->earned += 45 + g->combo * 20;
            game_notice(g, g->combo > 1 ? "CHAIN TAKEDOWN / BONUS CASH" : "UNIT SPUN OUT / +65");
        } else {
            g->earned += 5;
            game_notice(g, "TRAFFIC SPINOUT");
        }
        game_particle(g, a->v.x, 1, a->v.z, PAINTS[2], 14);
    }
}

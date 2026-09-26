#include "game.h"
#include <math.h>
void vehicle_resolve(Game *g, Vehicle *v, int model, float *impact) {
    float scale = v == &g->player ? player_scale(g) : 1;
    float radius = fmaxf(.35f, CARS[model].width * .52f) * scale;
    float half = fmaxf(0, CARS[model].length * .5f * scale - radius);
    int samples = (int)ceilf(half * 2 / fmaxf(radius, 1)) + 1;
    if (samples < 2)
        samples = 2;
    if (samples > 12)
        samples = 12;
    *impact = 0;
    float sn = sinf(v->yaw), cs = cosf(v->yaw);
    for (int j = 0; j < samples; j++) {
        float off = -half + j * (half * 2) / (samples - 1);
        Vehicle q = *v;
        q.x += sn * off;
        q.z += cs * off;
        float hit = 0;
        world_resolve(&g->world, &q, radius, &hit);
        v->x = q.x - sn * off;
        v->z = q.z - cs * off;
        v->vx = q.vx;
        v->vz = q.vz;
        *impact = fmaxf(*impact, hit);
    }
}
void drive_step(Game *g, Vehicle *v, float throttle, float brake, float steering, bool drift,
                bool boost, int model, float dt) {
    bool player = v == &g->player;
    CarSpec c = player ? car_tuned(&g->profile, model, -1, -1) : CARS[model];
    float sn = sinf(v->yaw), cs = cosf(v->yaw), forward = v->vx * sn + v->vz * cs,
          speed = vehicle_speed(v);
    steering = clampf(steering, -1, 1);
    if (player) {
        float sensitivity = .94f;
        steering = copysignf(powf(fabsf(steering), 1.3f), steering) * sensitivity;
    }
    v->steer += (steering - v->steer) * clampf(dt * (player ? 4.5f : 6.5f), 0, 1);
    float wheelbase = c.length * .62f, angle = v->steer * (.58f / (1 + speed * .035f));
    float yaw_rate = forward / wheelbase * tanf(angle) * (drift ? 1.18f : 1);
    yaw_rate = clampf(yaw_rate, -1.65f, 1.65f);
    v->yaw = angle_delta(v->yaw + yaw_rate * dt, 0);
    sn = sinf(v->yaw);
    cs = cosf(v->yaw);
    float side = v->vx * cs - v->vz * sn;
    float grip = c.grip * (drift ? .35f : 1);
    v->vx -= cs * side * clampf(grip * dt, 0, 1);
    v->vz += sn * side * clampf(grip * dt, 0, 1);
    float a = throttle * c.accel;
    if (brake > 0)
        a -= forward > .6f ? brake * 17 : brake * 4.5f;
    if (boost)
        a += 5.5f;
    v->vx += sn * a * dt;
    v->vz += cs * a * dt;
    float resistance = 1 / (1 + dt * (throttle > 0 ? .16f : .34f));
    v->vx *= resistance;
    v->vz *= resistance;
    float maximum = fminf(34, c.speed + (boost ? 4.0f : 0));
    speed = vehicle_speed(v);
    if (speed > maximum) {
        float wanted = fmaxf(maximum, speed - 12 * dt);
        v->vx *= wanted / speed;
        v->vz *= wanted / speed;
    }
    forward = v->vx * sn + v->vz * cs;
    if (forward < -5) {
        v->vx *= .92f;
        v->vz *= .92f;
    }
    float px = v->x, pz = v->z;
    v->x += v->vx * dt;
    v->z += v->vz * dt;
    float impact = 0;
    vehicle_resolve(g, v, model, &impact);
    float traveled = hypotf(v->x - px, v->z - pz);
    v->wheel_spin = fmodf(
        v->wheel_spin + traveled / (model == 7 ? .64f : .38f) * (forward < 0 ? -1 : 1), 2 * PI);
    v->lean += (-v->steer * speed * .0025f - v->lean) * clampf(dt * 5, 0, 1);
    v->pitch += (-a * .002f - v->pitch) * clampf(dt * 4, 0, 1);
    if (player && impact > 3.5f && g->hit_cool <= 0 && g->shield <= 0 && g->giant <= 0) {

        g->hit_cool = .65f;
        g->shake = .35f;
        g->flash = .15f;
        g->audio_events |= 1;
        game_particle(g, v->x, .6f, v->z, (Color){1, .6f, .15f, 1}, 12);
    }
}

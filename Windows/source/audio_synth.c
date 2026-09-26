#include "audio.h"
#include <math.h>
#define RATE 22050
static uint32_t sample_clock;
static float sine_table[1024];
static float engine_phase, siren_phase, bass_phase, arp_phase, kick_phase, effect_phase;
static float effect_time;
static int effect_kind;
static uint32_t noise = 321;
void audio_synth_reset(void) {
    sample_clock = 0;
    engine_phase = siren_phase = bass_phase = arp_phase = kick_phase = effect_phase = effect_time =
        0;
    effect_kind = 0;
    noise = 321;
    for (int i = 0; i < 1024; i++)
        sine_table[i] = sinf(2 * PI * i / 1024.f);
}
static float osc(float *phase, float freq) {
    *phase += freq / RATE;
    if (*phase >= 1)
        *phase -= floorf(*phase);
    return *phase < .5f ? 1 : -1;
}
void audio_mix(Game *g, int16_t *buf, int count) {
    if (g->audio_events) {
        effect_kind = g->audio_events & 1 ? 1 : 2;
        effect_time = .18f;
        g->audio_events = 0;
    }
    const Settings *s = &g->profile.settings;
    float speed = vehicle_speed(&g->player);
    bool driving = g->screen == PLAY;
    static const float notes[] = {55,        82.4069f, 110,       82.4069f, 65.4064f,  97.9989f,
                                  130.8128f, 97.9989f, 73.4162f,  110,      146.8324f, 110,
                                  65.4064f,  97.9989f, 130.8128f, 97.9989f};
    const float siren_frequency = 620 + sinf(g->time * 4) * 220;
    for (int i = 0; i < count; i++) {
        float sample = 0;
        uint32_t position = sample_clock++ % (3675u * 16u);
        if (s->music) {
            int step = position / 3675u;
            float beat = (position % 3675u) / 3675.0f;
            float base = notes[step];
            float bass = osc(&bass_phase, base) * .055f;
            float arp = osc(&arp_phase, base * 4) * .032f * (1 - beat);
            float envelope = fmaxf(0, 1 - beat * 5);
            osc(&kick_phase, 48 + 100 * envelope * envelope);
            float kick = sine_table[(int)(kick_phase * 1024) & 1023] * envelope * envelope * .12f;
            sample += bass + arp + kick;
        }
        if (s->sfx && driving) {
            sample += osc(&engine_phase, 35 + speed * 2.7f) * (g->boosting ? .085f : .05f);
            if (g->heat >= 1)
                sample += osc(&siren_phase, siren_frequency) * .025f;
        }
        if (effect_time > 0) {
            effect_time -= 1.0f / RATE;
            if (s->sfx) {
                if (effect_kind == 1) {
                    noise = noise * 1664525u + 1013904223u;
                    sample += ((noise >> 16) / 32768.0f - 1) * effect_time * 1.7f;
                } else {
                    osc(&effect_phase, 900 + effect_time * 4000);
                    sample += sine_table[(int)(effect_phase * 1024) & 1023] * effect_time * .7f;
                }
            }
        }
        buf[i] = (int16_t)(clampf(sample * s->volume / 10.0f, -1, 1) * 24000);
    }
}

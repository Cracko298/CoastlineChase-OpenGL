#ifndef COAST_AUDIO_H
#define COAST_AUDIO_H
#include "game.h"
void audio_synth_reset(void);
void audio_mix(Game *g, int16_t *samples, int count);
bool audio_init(void);
void audio_update(Game *g);
void audio_free(void);
#endif

#ifdef __3DS__
#include "audio.h"
#include <3ds.h>
#include <string.h>
#define SAMPLES 768
static ndspWaveBuf wave[3];
static int16_t *samples;
static bool ready;
bool audio_init(void) {
    if (R_FAILED(ndspInit()))
        return false;
    samples = linearAlloc(3 * SAMPLES * sizeof(int16_t));
    if (!samples) {
        ndspExit();
        return false;
    }
    memset(samples, 0, 3 * SAMPLES * sizeof(int16_t));
    memset(wave, 0, sizeof wave);
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, 22050);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ndspChnSetMix(0, mix);
    for (int i = 0; i < 3; i++) {
        wave[i].data_vaddr = samples + i * SAMPLES;
        wave[i].nsamples = SAMPLES;
    }
    audio_synth_reset();
    ready = true;
    return true;
}
void audio_update(Game *g) {
    if (!ready) {
        g->audio_events = 0;
        return;
    }
    for (int b = 0; b < 3; b++) {
        if (wave[b].status != NDSP_WBUF_DONE && wave[b].status != NDSP_WBUF_FREE)
            continue;
        audio_mix(g, samples + b * SAMPLES, SAMPLES);
        DSP_FlushDataCache(samples + b * SAMPLES, SAMPLES * sizeof(int16_t));
        ndspChnWaveBufAdd(0, &wave[b]);
    }
}
void audio_free(void) {
    if (!ready)
        return;
    ndspChnWaveBufClear(0);
    ndspExit();
    linearFree(samples);
    samples = NULL;
    ready = false;
}
#endif

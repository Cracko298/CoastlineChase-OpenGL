#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "audio.h"
#include <string.h>
#define SAMPLES 768
static HWAVEOUT output;
static WAVEHDR headers[3];
static int16_t samples[3][SAMPLES];
static bool ready, muted;
bool audio_init(void) {
    WAVEFORMATEX format = {WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
    if (waveOutOpen(&output, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        return false;
    for (int i = 0; i < 3; i++) {
        headers[i].lpData = (LPSTR)samples[i];
        headers[i].dwBufferLength = sizeof samples[i];
        if (waveOutPrepareHeader(output, &headers[i], sizeof headers[i]) != MMSYSERR_NOERROR) {
            for (int j = 0; j < i; j++)
                waveOutUnprepareHeader(output, &headers[j], sizeof headers[j]);
            waveOutClose(output);
            output = NULL;
            return false;
        }
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
    for (int i = 0; i < 3; i++) {
        if (headers[i].dwFlags & WHDR_INQUEUE)
            continue;
        audio_mix(g, samples[i], SAMPLES);
        if (muted)
            memset(samples[i], 0, sizeof samples[i]);
        waveOutWrite(output, &headers[i], sizeof headers[i]);
    }
}
void desktop_audio_pause(bool pause) {
    if (ready) {
        if (pause)
            waveOutPause(output);
        else
            waveOutRestart(output);
    }
}
void desktop_audio_mute(bool mute) {
    muted = mute;
}
void audio_free(void) {
    if (!ready)
        return;
    waveOutReset(output);
    for (int i = 0; i < 3; i++)
        waveOutUnprepareHeader(output, &headers[i], sizeof headers[i]);
    waveOutClose(output);
    output = NULL;
    ready = false;
}

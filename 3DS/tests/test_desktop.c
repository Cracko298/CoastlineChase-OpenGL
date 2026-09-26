#include "desktop.h"
#include "audio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
static Game g;
int main(void) {
    bool keys[256] = {0};
    keys['W'] = keys['A'] = keys[0x20] = keys[0x10] = true;
    unsigned k = desktop_keyboard(keys, true);
    assert((k & (IN_A | IN_LEFT | IN_R | IN_L)) == (IN_A | IN_LEFT | IN_R | IN_L));
    assert(!(k & IN_UP));
    assert(desktop_keyboard(keys, false) & IN_UP);
    memset(keys, 0, sizeof keys);
    keys[0x1B] = true;
    assert(desktop_keyboard(keys, true) == IN_START && desktop_keyboard(keys, false) == IN_B);
    memset(keys, 0, sizeof keys);
    keys['M'] = true;
    assert(desktop_keyboard(keys, true) == IN_MAP);
    const int sizes[][2] = {{800, 520}, {1280, 800}, {1920, 1080}, {3840, 2160}};
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++)
        for (int play = 0; play < 2; play++) {
            DesktopLayout l = desktop_layout(sizes[i][0], sizes[i][1], play, true);
            assert(l.scene.x >= 0 && l.scene.y >= 0 && l.scene.x + l.scene.w <= sizes[i][0] &&
                   l.scene.y + l.scene.h <= sizes[i][1]);
            assert(l.panel.x >= 0 && l.panel.y >= 0 && l.panel.x + l.panel.w <= sizes[i][0] &&
                   l.panel.y + l.panel.h <= sizes[i][1]);
            int x, y;
            assert(desktop_pointer(l.panel, l.panel.x + l.panel.w / 2, l.panel.y + l.panel.h / 2,
                                   &x, &y));
            assert(abs(x - 160) <= 1 && abs(y - 120) <= 1);
            assert(!desktop_pointer(l.panel, l.panel.x - 1, l.panel.y, &x, &y));
        }
    game_init(&g, true, 991);
    game_start(&g);
    game_update(&g, (Input){.pressed = IN_MAP}, 1.f / 60);
    assert(g.screen == CITYMAP);
    game_update(&g, (Input){.pressed = IN_SELECT}, 1.f / 60);
    assert(g.screen == SETTINGS && g.return_screen == PLAY);
    game_update(&g, (Input){.pressed = IN_B}, 1.f / 60);
    assert(g.screen == PLAY);
    game_update(&g, (Input){.pressed = IN_MAP}, 1.f / 60);
    game_update(&g, (Input){.pressed = IN_MAP}, 1.f / 60);
    assert(g.screen == PLAY);
    game_update(&g, (Input){.pressed = IN_START}, 1.f / 60);
    assert(g.screen == PAUSE);
    game_update(&g, (Input){.pressed = IN_SELECT}, 1.f / 60);
    assert(g.screen == SETTINGS);
    game_update(&g, (Input){.pressed = IN_B}, 1.f / 60);
    assert(g.screen == PAUSE);
    game_update(&g, (Input){.pressed = IN_MAP}, 1.f / 60);
    assert(g.screen == CITYMAP);
    int16_t pcm[768];
    audio_synth_reset();
    g.profile.settings.music = 1;
    g.profile.settings.sfx = 1;
    g.screen = PLAY;
    g.player.vz = 20;
    g.heat = 3;
    g.audio_events = 1;
    audio_mix(&g, pcm, 768);
    int peak = 0;
    for (int i = 0; i < 768; i++) {
        int a = abs(pcm[i]);
        if (a > peak)
            peak = a;
    }
    assert(peak > 100 && peak <= 24000 && !g.audio_events);
    g.profile.settings.music = g.profile.settings.sfx = 0;
    audio_mix(&g, pcm, 768);
    for (int i = 0; i < 768; i++)
        assert(pcm[i] == 0);
    puts("PASS: desktop driving/menu mappings, click coordinates at 800p through 4K, "
         "map/pause/settings flow and shared PCM audio");
}

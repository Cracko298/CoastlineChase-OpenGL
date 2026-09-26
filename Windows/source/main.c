#ifdef __3DS__
#include "game.h"
#include "render.h"
#include "audio.h"
#include <3ds.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
static Game game;
static unsigned keys(u32 k) {
    unsigned o = 0;
    if (k & KEY_A)
        o |= IN_A;
    if (k & KEY_B)
        o |= IN_B;
    if (k & KEY_X)
        o |= IN_X;
    if (k & KEY_Y)
        o |= IN_Y;
    if (k & KEY_L)
        o |= IN_L;
    if (k & KEY_R)
        o |= IN_R;
    if (k & KEY_START)
        o |= IN_START;
    if (k & KEY_SELECT)
        o |= IN_SELECT;
    if (k & KEY_DUP)
        o |= IN_UP;
    if (k & KEY_DDOWN)
        o |= IN_DOWN;
    if (k & KEY_DLEFT)
        o |= IN_LEFT;
    if (k & KEY_DRIGHT)
        o |= IN_RIGHT;
    return o;
}
int main(void) {
    gfxInitDefault();
    gfxSet3D(false);
    bool new3ds = false;
    APT_CheckNew3DS(&new3ds);
    if (new3ds)
        osSetSpeedupEnable(true);
    const char *savepath = "sdmc:/3ds/CoastlineChase/coastline.sav";
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/CoastlineChase", 0777);
    game_init(&game, new3ds, (uint32_t)osGetTime());
    profile_load(&game.profile, savepath, new3ds);
    Render top = {0}, bottom = {0};
    bool ok = backend_init();
    if (ok)
        ok = render_init(&top);
    if (ok) {
        bottom.mesh[PASS_UI].capacity = 42000;
        bottom.mesh[PASS_UI].v = linearAlloc(42000 * sizeof(Vertex));
        ok = bottom.mesh[PASS_UI].v != NULL;
    }
    if (!ok) {
        consoleInit(GFX_BOTTOM, NULL);
        printf("Coastline Chase\nGraphics initialization failed.\nPress START to exit.\n");
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_START)
                break;
            gspWaitForVBlank();
        }
        backend_free();
        render_free(&top);
        render_free(&bottom);
        gfxExit();
        return 1;
    }
    bool sound = audio_init();
    if (!sound)
        game_notice(&game, "AUDIO UNAVAILABLE - DSP NOT READY");
    u64 previous = svcGetSystemTick(), fpsstart = osGetTime(), bottom_tick = 0;
    Screen previous_screen = RESULT;
    unsigned frames = 0;
    float accumulator = 0, save_clock = 0;
    unsigned pending = 0, previous_menu_direction = 0;
    float menu_repeat = 0;
    bool pending_touch = false;
    int touchx = 0, touchy = 0;
    while (aptMainLoop() && !game.quit) {
        u64 frame_start = svcGetSystemTick();
        float dt = (float)(frame_start - previous) / SYSCLOCK_ARM11;
        previous = frame_start;
        dt = clampf(dt, 0, .1f);
        accumulator += dt;
        save_clock += dt;
        hidScanInput();
        circlePosition cp;
        hidCircleRead(&cp);
        pending |= keys(hidKeysDown());
        unsigned menu_direction = 0;
        if (game.screen != PLAY) {
            menu_direction = keys(hidKeysHeld()) & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT);
            if (cp.dx > 55)
                menu_direction |= IN_RIGHT;
            if (cp.dx < -55)
                menu_direction |= IN_LEFT;
            if (cp.dy > 55)
                menu_direction |= IN_UP;
            if (cp.dy < -55)
                menu_direction |= IN_DOWN;
            if (menu_direction != previous_menu_direction) {
                pending |= menu_direction;
                menu_repeat = .35f;
            } else if (menu_direction) {
                menu_repeat -= dt;
                if (menu_repeat <= 0) {
                    pending |= menu_direction;
                    menu_repeat = .13f;
                }
            }
        }
        previous_menu_direction = menu_direction;
        if (hidKeysDown() & KEY_TOUCH) {
            touchPosition t;
            hidTouchRead(&t);
            pending_touch = true;
            touchx = t.px;
            touchy = t.py;
        }
        Input in = {.held = keys(hidKeysHeld()) | menu_direction,
                    .pressed = pending,
                    .steer = abs(cp.dx) < 14 ? 0 : clampf(cp.dx / 140.0f, -1, 1),
                    .touch = pending_touch,
                    .touch_x = touchx,
                    .touch_y = touchy};
        int steps = 0;
        while (accumulator >= 1.0f / 60 && steps < 6) {
            game_update(&game, in, 1.0f / 60);
            accumulator -= 1.0f / 60;
            in.pressed = 0;
            in.touch = false;
            pending = 0;
            pending_touch = false;
            steps++;
        }
        if (steps == 6)
            accumulator = 0;
        audio_update(&game);
        if (game.dirty && save_clock > .7f) {
            bool saved = profile_save(&game.profile, savepath);
            game.save_failed = !saved;
            if (saved)
                game.dirty = false;
            save_clock = 0;
        }

        backend_begin();
        render_top(&top, &game, dt);
        backend_draw(&top, false, THEMES[preview_theme(&game)].sky);
        if (game.screen != PLAY || previous_screen != game.screen ||
            frame_start - bottom_tick > SYSCLOCK_ARM11 / 15) {
            render_bottom(&bottom, &game);
            bottom_tick = frame_start;
        }
        previous_screen = game.screen;
        backend_draw(&bottom, true, (Color){.01f, .02f, .04f, 1});
        backend_end();
        frames++;
        u64 now = osGetTime();
        if (now - fpsstart >= 500) {
            game.measured_fps = frames * 1000.0f / (now - fpsstart);
            frames = 0;
            fpsstart = now;
        }
        u64 target_ticks = SYSCLOCK_ARM11 / (game.profile.settings.fps == 30 ? 30 : 60);
        u64 elapsed = svcGetSystemTick() - frame_start;
        if (elapsed < target_ticks)
            svcSleepThread((s64)((target_ticks - elapsed) * 1000000000ULL / SYSCLOCK_ARM11));
    }
    if (game.screen == PLAY || game.screen == PAUSE ||
        (game.screen == SETTINGS && game.return_screen == PAUSE) ||
        (game.screen == HELP && game.return_screen == PAUSE) ||
        (game.screen == CITYMAP && (game.map_return == PLAY || game.map_return == PAUSE)))
        game_end(&game, 2);
    if (game.dirty) {
        profile_save(&game.profile, savepath);
    }
    audio_free();
    backend_free();
    render_free(&top);
    render_free(&bottom);
    gfxExit();
    return 0;
}
#endif

#include "render.h"
#include "desktop.h"
void desktop_chrome(Render *r, const Game *g, bool muted, bool pad) {
    render_reset(r, 1280, 800);
    Color cyan = {.23f, .93f, 1, 1}, mutedcolor = {.48f, .6f, .7f, 1};
    if (g->screen != PLAY) {
        r_text(r, 24, 18, 2, cyan, "COASTLINE CHASE / PC");
        r_text(r, 24, 44, 1, mutedcolor, "32 RIDES / PROCEDURAL ISLANDS / NEON PURSUITS");
    }
    r_rect(r, 0, 770, 1280, 30, (Color){.015f, .025f, .045f, .94f});
    r_text(r, 16, 780, 1.2f, cyan,
           pad ? "PAD: RT GAS / LT BRAKE / LB NITRO / RB DRIFT / START PAUSE / BACK MAP"
               : "WASD DRIVE / SPACE DRIFT / SHIFT NITRO / M MAP / TAB DASHBOARD / ESC PAUSE");
    r_text(r, 1000, 780, 1.1f, mutedcolor,
           muted ? "F10 SOUND OFF / F11 FULL" : "F1 HELP / F11 FULLSCREEN");
    if (g->save_failed)
        r_text(r, 24, 750, 1.5f, (Color){1, .4f, .3f, 1},
               "SAVE FAILED / CHECK DISK SPACE AND FOLDER ACCESS");
}

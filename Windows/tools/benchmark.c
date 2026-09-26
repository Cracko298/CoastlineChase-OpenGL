#include "game.h"
#include "render.h"
#include <stdio.h>
#include <time.h>
static Game g;
static double ms(clock_t t) {
    return 1000.0 * (clock() - t) / CLOCKS_PER_SEC;
}
int main(void) {
    game_init(&g, 0, 12345);
    game_start(&g);
    Render r;
    render_init(&r);
    g.profile.settings.view = 3;
    g.profile.settings.detail = 1;
    g.profile.settings.bloom = 1;
    g.run_time = 600;
    clock_t t = clock();
    for (int i = 0; i < 3600; i++) {
        g.player.hp = 100;
        g.bust = 0;
        g.screen = PLAY;
        game_update(&g, (Input){.held = IN_A, .steer = (i / 180) % 2 ? .35f : -.35f}, 1.f / 60);
    }
    double sim = ms(t);
    unsigned generated = g.world.generated;
    t = clock();
    for (int i = 0; i < 240; i++)
        render_top(&r, &g, 1.f / 30);
    printf("simulation_ms_per_tick %.4f render_ms_per_frame %.4f generated_chunks %u\n", sim / 3600,
           ms(t) / 240, generated);
    render_free(&r);
}

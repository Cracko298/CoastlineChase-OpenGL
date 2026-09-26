#include "game.h"
#include "render.h"
#include <assert.h>
#include <stdio.h>
static Game game;
int main(void) {
    Render r;
    assert(render_init(&r));
    for (int i = 0; i < 8; i++) {
        game_init(&game, false, (i + 1) * 537);
        game.screen = CITYMAP;
        render_bottom(&r, &game);
        char path[128];
        snprintf(path, sizeof path, "preview/city_%d.ppm", i);
        assert(render_ppm(&r, path, (Color){0, 0, 0, 1}));
    }
    render_free(&r);
    return 0;
}

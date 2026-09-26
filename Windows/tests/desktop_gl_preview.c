
#include <GL/osmesa.h>
#include "desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
static Game g;
static unsigned char pixels[1280 * 800 * 4];
static void save(const char *name) {
    glFinish();
    FILE *f = fopen(name, "wb");
    assert(f);
    fprintf(f, "P6\n1280 800\n255\n");
    for (int y = 799; y >= 0; y--)
        for (int x = 0; x < 1280; x++)
            fwrite(pixels + (y * 1280 + x) * 4, 1, 3, f);
    fclose(f);
}
int main(void) {
    OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGBA, 24, 8, 0, NULL);
    assert(ctx);
    assert(OSMesaMakeCurrent(ctx, pixels, GL_UNSIGNED_BYTE, 1280, 800));
    desktop_gl_init();
    printf("Desktop GL: %s / %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    Render scene, panel, overlay;
    assert(render_init(&scene) && render_init(&panel) && render_init(&overlay));

    render_reset(&scene, 1280, 800);
    Vertex vertices[6] = {{-.8f, -.8f, -.8f, 1, {1, 0, 0, 1}}, {.8f, -.8f, -.8f, 1, {1, 0, 0, 1}},
                          {0, .8f, -.8f, 1, {1, 0, 0, 1}},     {-.8f, -.8f, -.2f, 1, {0, 0, 1, 1}},
                          {.8f, -.8f, -.2f, 1, {0, 0, 1, 1}},  {0, .8f, -.2f, 1, {0, 0, 1, 1}}};
    memcpy(scene.mesh[PASS_SOLID].v, vertices, sizeof vertices);
    scene.mesh[PASS_SOLID].count = 6;
    desktop_draw_gl(&scene, (DesktopRect){0, 0, 1280, 800}, 800, (Color){0, 0, 0, 1}, true);
    glFinish();
    unsigned char *center = pixels + (400 * 1280 + 640) * 4;
    assert(center[0] > 240 && center[2] < 10);
    game_init(&g, true, 0xC0A57);
    game_start(&g);
    g.profile.settings.auto_lod = 0;
    g.profile.settings.showfps = 0;
    for (int n = 0; n < 6; n++) {
        g.notice_time = 0;
        g.screen = PLAY;
        g.run_time = 140;
        g.time = 12;
        g.heat = 3;
        if (n == 0)
            g.screen = TITLE;
        if (n == 1) {
            g.actors[12] = (Actor){.active = 1,
                                   .kind = PATROL,
                                   .variant = 1,
                                   .model = 5,
                                   .v = {.x = g.player.x - 3, .z = g.player.z + 22, .hp = 100}};
            g.helis[0] = (Helicopter){
                .active = true, .p = {g.player.x + 12, 22, g.player.z + 50}, .spot = 1};
        }
        if (n == 2 || n == 3) {
            g.screen = SHOP;
            g.tab = SHOP_CARS;
            g.cursor = n == 2 ? 20 : 24;
            g.profile.money = 3000;
        }
        if (n == 4) {
            g.screen = SETTINGS;
            g.return_screen = TITLE;
            g.settings_cursor = 0;
        }
        if (n == 5) {
            g.screen = CITYMAP;
            g.map_return = PLAY;
        }
        glScissor(0, 0, 1280, 800);
        glDepthMask(GL_TRUE);
        glClearColor(.009f, .016f, .03f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        DesktopLayout layout = desktop_layout(1280, 800, g.screen == PLAY, n == 1);
        render_top(&scene, &g, 1.f / 60);
        desktop_draw_gl(&scene, layout.scene, 800, THEMES[preview_theme(&g)].sky, true);
        if (layout.panel_visible) {
            render_bottom(&panel, &g);
            desktop_draw_gl(&panel, layout.panel, 800, (Color){.01f, .02f, .04f, 1}, true);
        }
        desktop_chrome(&overlay, &g, false, false);
        desktop_draw_gl(&overlay, (DesktopRect){0, 0, 1280, 800}, 800, (Color){0}, false);
        assert(glGetError() == GL_NO_ERROR);
        for (int pass = 0; pass < PASS_COUNT; pass++)
            assert(!scene.mesh[pass].dropped && !panel.mesh[pass].dropped &&
                   !overlay.mesh[pass].dropped);
        char path[128];
        snprintf(path, sizeof path, "preview/windows_%d.ppm", n);
        save(path);
    }
    render_free(&scene);
    render_free(&panel);
    render_free(&overlay);
    OSMesaDestroyContext(ctx);
    puts("PASS: shared Windows OpenGL path, reverse depth, six desktop layouts and no GL "
         "errors/buffer drops");
}

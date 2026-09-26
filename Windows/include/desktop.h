#ifndef COAST_DESKTOP_H
#define COAST_DESKTOP_H
#include "game.h"
#include "render.h"
typedef struct {
    int x, y, w, h;
} DesktopRect;
typedef struct {
    DesktopRect scene, panel;
    bool panel_visible;
} DesktopLayout;
DesktopLayout desktop_layout(int width, int height, bool playing, bool dashboard);
unsigned desktop_keyboard(const bool keys[256], bool playing);
bool desktop_pointer(DesktopRect rect, int x, int y, int *tx, int *ty);
const char *desktop_text(const char *text);
void desktop_gl_init(void);
void desktop_draw_gl(Render *r, DesktopRect rect, int height, Color clear, bool erase);
void desktop_chrome(Render *r, const Game *g, bool muted, bool pad);
#endif

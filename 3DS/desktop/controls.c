#include "desktop.h"
#include <math.h>
static DesktopRect fit(int x, int y, int w, int h, float aspect) {
    int ww = w, hh = (int)(w / aspect);
    if (hh > h) {
        hh = h;
        ww = (int)(h * aspect);
    }
    if (ww < 1)
        ww = 1;
    if (hh < 1)
        hh = 1;
    return (DesktopRect){x + (w - ww) / 2, y + (h - hh) / 2, ww, hh};
}
DesktopLayout desktop_layout(int w, int h, bool playing, bool dashboard) {
    DesktopLayout l = {0};
    l.panel_visible = !playing || dashboard;
    if (playing) {
        l.scene = fit(0, 0, w, h, 400.f / 240);
        int pw = (int)clampf(w * .30f, 260, 480), ph = pw * 3 / 4;
        l.panel = (DesktopRect){w - pw - 16, h - ph - 42, pw, ph};
    } else {
        int side = (int)(w * .31f);
        l.scene = fit(12, 48, w - side - 36, h - 100, 400.f / 240);
        l.panel = fit(w - side - 12, 90, side, h - 180, 320.f / 240);
    }
    return l;
}
bool desktop_pointer(DesktopRect r, int x, int y, int *tx, int *ty) {
    if (r.w <= 0 || r.h <= 0 || x < r.x || y < r.y || x >= r.x + r.w || y >= r.y + r.h)
        return false;
    *tx = (x - r.x) * 320 / r.w;
    *ty = (y - r.y) * 240 / r.h;
    return true;
}
unsigned desktop_keyboard(const bool k[256], bool play) {
    unsigned out = 0;
    if (k['A'] || k[0x25])
        out |= IN_LEFT;
    if (k['D'] || k[0x27])
        out |= IN_RIGHT;
    if (k['W'] || k[0x26])
        out |= play ? IN_A : IN_UP;
    if (k['S'] || k[0x28])
        out |= play ? IN_B : IN_DOWN;
    if (k[0x0D])
        out |= IN_A;
    if (k[0x1B])
        out |= play ? IN_START : IN_B;
    if (k[0x20])
        out |= play ? IN_R : IN_A;
    if (k[0x10] || k['Q'])
        out |= IN_L;
    if (k['E'])
        out |= IN_R;
    if (k['X'] || k['C'])
        out |= IN_X;
    if (k['R'])
        out |= IN_Y;
    if (k['M'])
        out |= IN_MAP;
    if (k[0x71])
        out |= IN_SELECT;
    return out;
}

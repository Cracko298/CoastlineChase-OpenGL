#ifndef COAST_RENDER_H
#define COAST_RENDER_H
#include "game.h"
uint8_t coast_glyph(char c, int row);
typedef struct {
    float x, y, z, w;
    Color c;
} Vertex;
_Static_assert(sizeof(Vertex) == 32, "GPU vertex layout must remain 32 bytes");
enum { PASS_SOLID, PASS_EDGE, PASS_GLOW, PASS_UI, PASS_COUNT };
typedef struct {
    Vertex *v;
    unsigned count, capacity, dropped;
} Mesh;
typedef struct {
    Mesh mesh[PASS_COUNT];
    Vec3 eye, right, up, forward, focus;
    float focal, far_clip;
    Color fog;
    int width, height;
    const Settings *settings;
    Settings visual;
    float night, rain, daylight;
    float car_sn, car_cs, car_scale;
    bool dirty;
    float lod_hold;
} Render;
bool render_init(Render *r);
void render_free(Render *r);
void render_reset(Render *r, int width, int height);
void render_top(Render *r, Game *g, float dt);
void render_hud(Render *r, const Game *g);
void render_bottom(Render *r, const Game *g);
void r_rect(Render *r, float x, float y, float w, float h, Color c);
void r_text(Render *r, float x, float y, float scale, Color c, const char *text);
void r_line2(Render *r, float x, float y, float xx, float yy, float width, Color c);
void r_box(Render *r, Vec3 center, Vec3 size, float yaw, Color fill, Color edge, bool glow);
void r_line(Render *r, Vec3 a, Vec3 b, Color c, float width, bool glow);
void r_quad(Render *r, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Color col);
void r_car(Render *r, Game *g, Vehicle v, int model, CarStyle style, int cop);
#ifdef __3DS__
bool backend_init(void);
void backend_free(void);
void backend_begin(void);
void backend_draw(Render *r, bool bottom, Color clear);
void backend_end(void);
#else
bool render_ppm(const Render *r, const char *path, Color clear);
#endif
#endif

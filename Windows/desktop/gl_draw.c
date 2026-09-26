#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include <stddef.h>
#include "render.h"
#include "desktop.h"
void desktop_gl_init(void) {
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glClearDepth(0);
    glDepthFunc(GL_GEQUAL);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glScalef(1, 1, -2);
    glTranslatef(0, 0, .5f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}
void desktop_draw_gl(Render *r, DesktopRect rect, int height, Color clear, bool erase) {
    glViewport(rect.x, height - rect.y - rect.h, rect.w, rect.h);
    glScissor(rect.x, height - rect.y - rect.h, rect.w, rect.h);
    if (erase) {
        glDepthMask(GL_TRUE);
        glClearColor(clear.r, clear.g, clear.b, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    for (int pass = 0; pass < PASS_COUNT; pass++) {
        const Mesh *m = &r->mesh[pass];
        if (!m->count)
            continue;
        if (pass == PASS_UI)
            glDisable(GL_DEPTH_TEST);
        else
            glEnable(GL_DEPTH_TEST);
        glDepthMask(pass == PASS_SOLID ? GL_TRUE : GL_FALSE);
        glBlendFunc(GL_SRC_ALPHA, pass == PASS_GLOW ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
        glVertexPointer(4, GL_FLOAT, sizeof(Vertex), m->v);
        glColorPointer(4, GL_FLOAT, sizeof(Vertex), (const char *)m->v + offsetof(Vertex, c));
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m->count);
    }
}

#ifdef __3DS__
#include "render.h"
#include <3ds.h>
#include <citro3d.h>
#include "coast_shbin.h"
static C3D_RenderTarget *targets[2];
static DVLB_s *shader;
static shaderProgram_s program;
static bool initialized, program_ready;
#define TRANSFER_FLAGS                                                                             \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |               \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
bool backend_init(void) {
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 2))
        return false;
    initialized = true;
    shader = DVLB_ParseFile((u32 *)coast_shbin, coast_shbin_size);
    if (!shader)
        return false;
    shaderProgramInit(&program);
    program_ready = true;
    if (R_FAILED(shaderProgramSetVsh(&program, &shader->DVLE[0])))
        return false;
    C3D_BindProgram(&program);
    targets[0] = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    targets[1] = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if (!targets[0] || !targets[1])
        return false;
    C3D_RenderTargetSetOutput(targets[0], GFX_TOP, GFX_LEFT, TRANSFER_FLAGS);
    C3D_RenderTargetSetOutput(targets[1], GFX_BOTTOM, GFX_LEFT, TRANSFER_FLAGS);
    C3D_AttrInfo *a = C3D_GetAttrInfo();
    AttrInfo_Init(a);
    AttrInfo_AddLoader(a, 0, GPU_FLOAT, 4);
    AttrInfo_AddLoader(a, 1, GPU_FLOAT, 4);
    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthMap(true, -1, 0);
    return true;
}
void backend_begin(void) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
}
void backend_draw(Render *r, bool bottom, Color clear) {
    uint32_t col = ((uint32_t)(clear.r * 255) << 24) | ((uint32_t)(clear.g * 255) << 16) |
                   ((uint32_t)(clear.b * 255) << 8) | 255;
    C3D_RenderTarget *target = targets[bottom ? 1 : 0];
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, col, 0);
    C3D_FrameDrawOn(target);
    for (int p = 0; p < PASS_COUNT; p++) {
        Mesh *m = &r->mesh[p];
        if (!m->count)
            continue;
        C3D_DepthTest(p != PASS_UI, GPU_GEQUAL, p == PASS_SOLID ? GPU_WRITE_ALL : GPU_WRITE_COLOR);
        if (p == PASS_GLOW)
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE, GPU_ONE, GPU_ZERO);
        else
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                           GPU_ONE, GPU_ZERO);
        if (r->dirty)
            GSPGPU_FlushDataCache(m->v, m->count * sizeof(Vertex));
        C3D_BufInfo *b = C3D_GetBufInfo();
        BufInfo_Init(b);
        BufInfo_Add(b, m->v, sizeof(Vertex), 2, 0x10);
        C3D_DrawArrays(GPU_TRIANGLES, 0, m->count);
    }
    r->dirty = false;
}
void backend_end(void) {
    C3D_FrameEnd(0);
}
void backend_free(void) {
    if (!initialized)
        return;
    C3D_FrameSync();
    for (int i = 0; i < 2; i++)
        if (targets[i])
            C3D_RenderTargetDelete(targets[i]);
    if (program_ready)
        shaderProgramFree(&program);
    if (shader)
        DVLB_Free(shader);
    C3D_Fini();
    initialized = false;
}
#endif

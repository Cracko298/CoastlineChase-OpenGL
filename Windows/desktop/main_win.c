#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <xinput.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
#include "game.h"
#include "render.h"
#include "audio.h"
#include "desktop.h"
static Game game;
static HWND window;
static HDC device;
static HGLRC context;
static int width = 1280, height = 800;
static bool active = true, closing, fullscreen, dashboard, muted;
static bool keys[256], click;
static unsigned shortcuts;
static int mousex, mousey;
static RECT windowed;
static char savepath[512];
static wchar_t savedir[MAX_PATH];
static HMODULE xinput;
static DWORD(WINAPI *pad_state)(DWORD, XINPUT_STATE *);
void desktop_audio_pause(bool pause);
void desktop_audio_mute(bool mute);
static bool run_active(void) {
    return game.screen == PLAY || game.screen == PAUSE ||
           ((game.screen == SETTINGS || game.screen == HELP) &&
            (game.return_screen == PLAY || game.return_screen == PAUSE)) ||
           (game.screen == CITYMAP && (game.map_return == PLAY || game.map_return == PAUSE));
}
static LRESULT CALLBACK procedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE:
        closing = true;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        width = LOWORD(lp);
        height = HIWORD(lp);
        return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lp)->ptMinTrackSize = (POINT){800, 520};
        return 0;
    case WM_ACTIVATEAPP:
        active = wp != 0;
        if (!active) {
            memset(keys, 0, sizeof keys);
            click = false;
            shortcuts = 0;
            if (game.screen == PLAY) {
                game.screen = PAUSE;
                game.cursor = 0;
            }
        }
        desktop_audio_pause(!active);
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp < 256)
            keys[wp] = true;
        if (!(lp & (1L << 30))) {
            if (wp == VK_F11 || (wp == VK_RETURN && (lp & (1L << 29))))
                shortcuts |= 1;
            if (wp == VK_TAB)
                shortcuts |= 2;
            if (wp == VK_F1)
                shortcuts |= 4;
            if (wp == VK_F10)
                shortcuts |= 8;
            if (wp == VK_F9)
                shortcuts |= 16;
        }
        if (msg == WM_SYSKEYDOWN && wp == VK_F4)
            break;
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wp < 256)
            keys[wp] = false;
        return 0;
    case WM_LBUTTONDOWN:
        mousex = GET_X_LPARAM(lp);
        mousey = GET_Y_LPARAM(lp);
        click = true;
        return 0;
    case WM_DPICHANGED: {
        RECT *r = (RECT *)lp;
        SetWindowPos(hwnd, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
static void toggle_fullscreen(void) {
    fullscreen = !fullscreen;
    if (fullscreen) {
        GetWindowRect(window, &windowed);
        MONITORINFO info = {.cbSize = sizeof info};
        GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info);
        SetWindowLongPtrW(window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(window, HWND_TOP, info.rcMonitor.left, info.rcMonitor.top,
                     info.rcMonitor.right - info.rcMonitor.left,
                     info.rcMonitor.bottom - info.rcMonitor.top, SWP_FRAMECHANGED);
    } else {
        SetWindowLongPtrW(window, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPos(window, NULL, windowed.left, windowed.top, windowed.right - windowed.left,
                     windowed.bottom - windowed.top, SWP_FRAMECHANGED | SWP_NOZORDER);
    }
}
static bool graphics_init(HINSTANCE instance) {
    SetProcessDPIAware();
    WNDCLASSW wc = {.style = CS_OWNDC,
                    .lpfnWndProc = procedure,
                    .hInstance = instance,
                    .hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1)),
                    .hCursor = LoadCursorW(NULL, IDC_ARROW),
                    .lpszClassName = L"CoastlineChaseWindow"};
    if (!RegisterClassW(&wc))
        return false;
    RECT bounds = {0, 0, width, height};
    AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
    window = CreateWindowW(wc.lpszClassName, L"Coastline Chase - Windows v0.5", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left,
                           bounds.bottom - bounds.top, NULL, NULL, instance, NULL);
    if (!window)
        return false;
    device = GetDC(window);
    PIXELFORMATDESCRIPTOR p = {.nSize = sizeof p,
                               .nVersion = 1,
                               .dwFlags =
                                   PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                               .iPixelType = PFD_TYPE_RGBA,
                               .cColorBits = 32,
                               .cDepthBits = 24,
                               .cStencilBits = 8,
                               .iLayerType = PFD_MAIN_PLANE};
    int format = ChoosePixelFormat(device, &p);
    if (!format || !SetPixelFormat(device, format, &p))
        return false;
    context = wglCreateContext(device);
    if (!context || !wglMakeCurrent(device, context))
        return false;
    desktop_gl_init();
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    return true;
}
static bool ui_init(Render *r, unsigned capacity) {
    memset(r, 0, sizeof *r);
    r->mesh[PASS_UI].capacity = capacity;
    r->mesh[PASS_UI].v = malloc(capacity * sizeof(Vertex));
    return r->mesh[PASS_UI].v != NULL;
}
static bool save_location(bool portable) {
    if (portable) {
        if (!GetModuleFileNameW(NULL, savedir, MAX_PATH))
            return false;
        wchar_t *slash = wcsrchr(savedir, L'\\');
        if (!slash)
            return false;
        *slash = 0;
    } else {
        if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL,
                                    SHGFP_TYPE_CURRENT, savedir)))
            return false;
        if (wcslen(savedir) + 20 >= MAX_PATH)
            return false;
        wcscat(savedir, L"\\CoastlineChase");
        if (!CreateDirectoryW(savedir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
            return false;
    }
    wchar_t full[MAX_PATH];
    if (wcslen(savedir) + 16 >= MAX_PATH)
        return false;
    wcscpy(full, savedir);
    wcscat(full, L"\\coastline.sav");
    return WideCharToMultiByte(CP_UTF8, 0, full, -1, savepath, sizeof savepath - 5, NULL, NULL) > 0;
}
static unsigned controller(float *steer, bool *connected) {
    XINPUT_STATE state = {0};
    *steer = 0;
    *connected = false;
    if (!pad_state)
        return 0;
    for (DWORD i = 0; i < 4; i++)
        if (pad_state(i, &state) == ERROR_SUCCESS) {
            *connected = true;
            break;
        }
    if (!*connected)
        return 0;
    WORD b = state.Gamepad.wButtons;
    unsigned out = 0;
    float axis = state.Gamepad.sThumbLX / 32767.f;
    *steer = fabsf(axis) < .18f ? 0 : copysignf(clampf((fabsf(axis) - .18f) / .82f, 0, 1), axis);
    if (b & XINPUT_GAMEPAD_A)
        out |= IN_A;
    if (b & XINPUT_GAMEPAD_B)
        out |= IN_B;
    if (b & XINPUT_GAMEPAD_X)
        out |= IN_X;
    if (b & XINPUT_GAMEPAD_Y)
        out |= IN_Y;
    if (b & XINPUT_GAMEPAD_LEFT_SHOULDER)
        out |= IN_L;
    if (b & XINPUT_GAMEPAD_RIGHT_SHOULDER)
        out |= IN_R;
    if (b & XINPUT_GAMEPAD_START)
        out |= IN_START;
    if (b & XINPUT_GAMEPAD_BACK)
        out |= IN_MAP;
    if (b & XINPUT_GAMEPAD_DPAD_LEFT)
        out |= IN_LEFT;
    if (b & XINPUT_GAMEPAD_DPAD_RIGHT)
        out |= IN_RIGHT;
    if (game.screen == PLAY) {
        if (state.Gamepad.bRightTrigger > 30 || (b & XINPUT_GAMEPAD_DPAD_UP))
            out |= IN_A;
        if (state.Gamepad.bLeftTrigger > 30 || (b & XINPUT_GAMEPAD_DPAD_DOWN))
            out |= IN_B;
    } else {
        if (state.Gamepad.sThumbLY > 16000 || (b & XINPUT_GAMEPAD_DPAD_UP))
            out |= IN_UP;
        if (state.Gamepad.sThumbLY < -16000 || (b & XINPUT_GAMEPAD_DPAD_DOWN))
            out |= IN_DOWN;
        if (*steer > .6f)
            out |= IN_RIGHT;
        if (*steer < -.6f)
            out |= IN_LEFT;
    }
    return out;
}
static void screenshot(const char *name) {
    size_t stride = ((size_t)width * 3 + 3) & ~(size_t)3, bytes = stride * height;
    unsigned char *pixels = malloc(bytes);
    if (!pixels)
        return;
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            unsigned char *p = pixels + y * stride + x * 3;
            unsigned char b = p[0];
            p[0] = p[2];
            p[2] = b;
        }
    BITMAPFILEHEADER file = {.bfType = 0x4d42, .bfSize = (DWORD)(54 + bytes), .bfOffBits = 54};
    BITMAPINFOHEADER info = {.biSize = 40,
                             .biWidth = width,
                             .biHeight = height,
                             .biPlanes = 1,
                             .biBitCount = 24,
                             .biSizeImage = (DWORD)bytes};
    FILE *f = fopen(name, "wb");
    if (f) {
        fwrite(&file, sizeof file, 1, f);
        fwrite(&info, sizeof info, 1, f);
        fwrite(pixels, 1, bytes, f);
        fclose(f);
    }
    free(pixels);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prior, LPWSTR command, int show) {
    (void)prior;
    (void)show;
    bool smoke = wcsstr(command, L"--smoke-test") != NULL,
         portable = wcsstr(command, L"--portable") != NULL;
    if (!save_location(portable)) {
        MessageBoxW(NULL, L"Cannot open your save folder.", L"Coastline Chase",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    game_init(&game, true, smoke ? 0xC0A57u : (uint32_t)GetTickCount64());
    if (!smoke)
        profile_load(&game.profile, savepath, true);
    Render scene = {0}, panel = {0}, overlay = {0};
    if (!graphics_init(instance) || !render_init(&scene) || !ui_init(&panel, 42000) ||
        !ui_init(&overlay, 14000)) {
        MessageBoxW(window, L"Cannot initialize OpenGL graphics. Check your display driver.",
                    L"Coastline Chase", MB_OK | MB_ICONERROR);
        return 1;
    }
    const wchar_t *dlls[] = {L"xinput1_4.dll", L"xinput9_1_0.dll"};
    for (int i = 0; i < 2 && !xinput; i++)
        xinput = LoadLibraryExW(dlls[i], NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (xinput) {
        FARPROC f = GetProcAddress(xinput, "XInputGetState");
        memcpy(&pad_state, &f, sizeof pad_state);
    }
    bool sound = audio_init();
    if (!sound)
        game_notice(&game, "AUDIO UNAVAILABLE / GAME STILL PLAYABLE");
    LARGE_INTEGER frequency, previous;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previous);
    double fpsclock = 0;
    unsigned frames = 0, frame = 0, previous_buttons = 0, pending = 0, previous_direction = 0;
    float accumulator = 0, save_clock = 0, repeat = 0;
    bool pending_click = false;
    int touchx = 0, touchy = 0;
    unsigned errors = 0;
    FILE *report = smoke ? fopen("windows-smoke.log", "w") : NULL;
    if (report)
        fprintf(report, "GL vendor: %s\nGL renderer: %s\nGL version: %s\n", glGetString(GL_VENDOR),
                glGetString(GL_RENDERER), glGetString(GL_VERSION));
    while (!closing && !game.quit) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                closing = true;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        LARGE_INTEGER start;
        QueryPerformanceCounter(&start);
        float dt = (float)((double)(start.QuadPart - previous.QuadPart) / frequency.QuadPart);
        previous = start;
        if (!smoke && (!active || IsIconic(window) || width < 1 || height < 1)) {
            pending = previous_buttons = previous_direction = 0;
            pending_click = false;
            accumulator = 0;
            Sleep(20);
            continue;
        }
        dt = smoke ? 1.f / 60 : clampf(dt, 0, .1f);
        accumulator += dt;
        save_clock += dt;
        if (shortcuts & 1) {
            toggle_fullscreen();
            keys[VK_RETURN] = false;
        }
        if (shortcuts & 2)
            dashboard = !dashboard;
        if (shortcuts & 4) {
            if (game.screen == HELP) {
                game.screen = game.return_screen;
                game.cursor = game.old_cursor;
            } else {
                if (game.screen == SETTINGS) {
                    game.screen = game.return_screen;
                    game.cursor = game.old_cursor;
                }
                if (game.screen == CITYMAP)
                    game.screen = game.map_return;
                game.return_screen = game.screen;
                game.old_cursor = game.cursor;
                game.screen = HELP;
            }
        }
        if (shortcuts & 8) {
            muted = !muted;
            desktop_audio_mute(muted);
        }
        if (shortcuts & 16)
            ShellExecuteW(window, L"open", savedir, NULL, NULL, SW_SHOWNORMAL);
        shortcuts = 0;
        bool pad = false;
        float steer = 0;
        unsigned held = desktop_keyboard(keys, game.screen == PLAY) | controller(&steer, &pad);
        pending |= held & ~previous_buttons;
        previous_buttons = held;
        unsigned direction =
            game.screen == PLAY ? 0 : held & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT);
        if (direction != previous_direction) {
            pending |= direction;
            repeat = .35f;
        } else if (direction) {
            repeat -= dt;
            if (repeat <= 0) {
                pending |= direction;
                repeat = .13f;
            }
        }
        previous_direction = direction;
        DesktopLayout layout = desktop_layout(width, height, game.screen == PLAY, dashboard);
        if (click && layout.panel_visible &&
            desktop_pointer(layout.panel, mousex, mousey, &touchx, &touchy))
            pending_click = true;
        click = false;
        Input in = {.held = held,
                    .pressed = pending,
                    .steer = steer,
                    .touch = pending_click,
                    .touch_x = touchx,
                    .touch_y = touchy};
        if (smoke) {
            if (frame == 30)
                in.pressed = IN_A;
            if (frame > 30 && frame < 180) {
                in.held = IN_A | IN_L;
                in.steer = sinf(frame * .025f) * .2f;
            }
            if (frame == 180) {
                game.screen = SHOP;
                game.tab = SHOP_CARS;
                game.profile.money = 9000;
            }
            if (frame >= 180 && frame < 300)
                game.cursor = 20 + (frame - 180) / 10;
            if (frame == 300) {
                game.screen = SETTINGS;
                game.return_screen = TITLE;
                game.settings_cursor = 14;
            }
            if (frame == 330) {
                game.screen = CITYMAP;
                game.map_return = PLAY;
            }
            if (frame == 360)
                game.screen = PLAY;
            if (frame >= 360) {
                in.held = IN_A;
                in.steer = .15f;
                dashboard = true;
            }
        }
        int steps = 0;
        while (accumulator >= 1.f / 60 && steps < 6) {
            game_update(&game, in, 1.f / 60);
            accumulator -= 1.f / 60;
            in.pressed = 0;
            in.touch = false;
            pending = 0;
            pending_click = false;
            steps++;
        }
        if (steps == 6)
            accumulator = 0;
        if (!smoke && game.dirty && save_clock > .7f) {
            game.save_failed = !profile_save(&game.profile, savepath);
            if (!game.save_failed)
                game.dirty = false;
            save_clock = 0;
        }
        audio_update(&game);
        glViewport(0, 0, width, height);
        glScissor(0, 0, width, height);
        glDepthMask(GL_TRUE);
        glClearColor(.009f, .016f, .03f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        layout = desktop_layout(width, height, game.screen == PLAY, dashboard);
        render_top(&scene, &game, dt);
        desktop_draw_gl(&scene, layout.scene, height, THEMES[preview_theme(&game)].sky, true);
        if (layout.panel_visible) {
            render_bottom(&panel, &game);
            desktop_draw_gl(&panel, layout.panel, height, (Color){.01f, .02f, .04f, 1}, true);
        }
        desktop_chrome(&overlay, &game, muted, pad);
        desktop_draw_gl(&overlay, (DesktopRect){0, 0, width, height}, height, (Color){0}, false);
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            errors++;
            if (report)
                fprintf(report, "GL error frame %u: %u\n", frame, error);
        }
        if (smoke && (frame == 20 || frame == 120 || frame == 205 || frame == 305 || frame == 335 ||
                      frame == 400)) {
            char path[80];
            snprintf(path, sizeof path, "windows-frame-%u.bmp", frame);
            screenshot(path);
        }
        SwapBuffers(device);
        frame++;
        frames++;
        fpsclock += dt;
        if (fpsclock >= .5) {
            game.measured_fps = (float)(frames / fpsclock);
            frames = 0;
            fpsclock = 0;
        }
        if (smoke && frame >= 420)
            break;
        if (!smoke) {
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            double elapsed = (double)(now.QuadPart - start.QuadPart) / frequency.QuadPart;
            double delay = 1.0 / game.profile.settings.fps - elapsed;
            if (delay > .001)
                Sleep((DWORD)(delay * 1000));
        }
    }
    if (!smoke) {
        if (run_active())
            game_end(&game, 2);
        if (game.dirty && !profile_save(&game.profile, savepath))
            MessageBoxW(window,
                        L"Save failed. Please check disk space and folder permissions. Your "
                        L"previous backup is retained.",
                        L"Coastline Chase", MB_OK | MB_ICONWARNING);
    }
    if (report) {
        fprintf(report, "Frames: %u\nGL errors: %u\nState finite: %d\n", frame, errors,
                isfinite(game.player.x) && isfinite(game.player.z));
        fclose(report);
    }
    audio_free();
    render_free(&scene);
    render_free(&panel);
    render_free(&overlay);
    if (xinput)
        FreeLibrary(xinput);
    wglMakeCurrent(NULL, NULL);
    if (context)
        wglDeleteContext(context);
    if (device)
        ReleaseDC(window, device);
    if (window)
        DestroyWindow(window);
    return errors ? 2 : 0;
}

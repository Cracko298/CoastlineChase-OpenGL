#include "game.h"
static void settings(Game *g) {
    g->return_screen = g->screen;
    g->old_cursor = g->cursor;
    g->screen = SETTINGS;
}
static void garage(Game *g) {
    g->screen = GARAGE;
    g->cursor = g->tab;
    g->confirm_purchase = false;
    g->preview_yaw = 0;
}
static void buy(Game *g) {
    if (shop_owned(g, g->tab, g->cursor)) {
        game_purchase(g);
        return;
    }
    if (g->profile.money < (uint32_t)shop_price(g, g->tab, g->cursor)) {
        game_notice(g, "NOT ENOUGH CASH - KEEP DRIVING");
        return;
    }
    g->confirm_purchase = true;
}
void menus_step(Game *g, Input in, float dt) {
    if ((in.pressed & IN_SELECT) && g->screen != SETTINGS) {

        if (g->screen == HELP) {
            g->screen = g->return_screen;
            g->cursor = g->old_cursor;
        }
        if (g->screen == CITYMAP)
            g->screen = g->map_return;
        settings(g);
        return;
    }
    if ((in.pressed & IN_MAP) && g->screen == PAUSE) {
        g->map_return = PAUSE;
        g->screen = CITYMAP;
        return;
    }
    if (g->screen == CITYMAP) {
        if (in.pressed & (IN_B | IN_A | IN_START | IN_MAP) || (in.touch && in.touch_y >= 215)) {
            g->screen = g->map_return;
            return;
        }
        if ((in.pressed & IN_Y) && g->sprint_node >= 0)
            game_set_waypoint(g, g->sprint_node);
        if (in.pressed & IN_X) {
            g->waypoint = -1;
            game_notice(g, "ROUTE CLEARED");
        }
        if (in.touch && in.touch_x >= 12 && in.touch_x < 172 && in.touch_y >= 45 &&
            in.touch_y < 205) {
            int x = (in.touch_x - 12) / 5, z = 31 - (in.touch_y - 45) / 5;
            game_set_waypoint(g, z * MAP_SIDE + x);
        }
        return;
    }
    if (g->screen == SETTINGS) {
        if (in.pressed & (IN_B | IN_SELECT) || (in.touch && in.touch_y >= 215)) {
            g->screen = g->return_screen;
            g->cursor = g->old_cursor;
            return;
        }
        if (in.pressed & IN_UP)
            g->settings_cursor = (g->settings_cursor + SETTING_COUNT - 1) % SETTING_COUNT;
        if (in.pressed & IN_DOWN)
            g->settings_cursor = (g->settings_cursor + 1) % SETTING_COUNT;
        if (in.pressed & IN_L)
            g->settings_cursor = (g->settings_cursor + SETTING_COUNT - 5) % SETTING_COUNT;
        if (in.pressed & IN_R)
            g->settings_cursor = (g->settings_cursor + 5) % SETTING_COUNT;
        if (in.pressed & IN_LEFT)
            setting_change(g, g->settings_cursor, -1);
        if (in.pressed & (IN_RIGHT | IN_A))
            setting_change(g, g->settings_cursor, 1);
        if (in.touch && in.touch_y >= 46 && in.touch_y < 196) {
            int i = (g->settings_cursor / 5) * 5 + (in.touch_y - 46) / 30;
            if (i < SETTING_COUNT) {
                g->settings_cursor = i;
                setting_change(g, i, in.touch_x < 160 ? -1 : 1);
            }
        }
        return;
    }
    if (g->screen == HELP) {
        if (in.pressed & (IN_A | IN_B | IN_START) || in.touch) {
            g->screen = g->return_screen;
            g->cursor = g->old_cursor;
        }
        return;
    }
    if (g->screen == GARAGE) {
        if (in.pressed & IN_B || (in.touch && in.touch_y >= 215)) {
            g->screen = TITLE;
            g->cursor = 1;
            return;
        }
        int d = 0;
        if (in.pressed & IN_LEFT)
            d = -1;
        if (in.pressed & IN_RIGHT)
            d = 1;
        if (in.pressed & IN_UP)
            d = -3;
        if (in.pressed & IN_DOWN)
            d = 3;
        g->cursor = (g->cursor + d + SHOP_TABS) % SHOP_TABS;
        bool open = in.pressed & IN_A;
        if (in.touch && in.touch_x >= 12 && in.touch_y >= 52 && in.touch_y < 198) {
            int col = (in.touch_x - 12) / 100, row = (in.touch_y - 52) / 50;
            if (col < 3 && row < 3) {
                g->cursor = row * 3 + col;
                open = true;
            }
        }
        if (open) {
            g->tab = g->cursor;
            g->screen = SHOP;
            g->cursor = g->tab == SHOP_CARS ? g->profile.selected : 0;
            g->notice_time = 0;
        }
        return;
    }
    if (g->screen == SHOP) {
        if (g->confirm_purchase) {
            if (in.pressed & IN_B || (in.touch && in.touch_y > 140 && in.touch_x < 160)) {
                g->confirm_purchase = false;
                return;
            }
            if (in.pressed & IN_A || (in.touch && in.touch_y > 140 && in.touch_x >= 160)) {
                game_purchase(g);
                return;
            }
            return;
        }
        if (in.pressed & IN_B || (in.touch && in.touch_y >= 215 && in.touch_x < 120)) {
            garage(g);
            return;
        }
        if (in.pressed & IN_L) {
            g->tab = (g->tab + SHOP_TABS - 1) % SHOP_TABS;
            g->cursor = 0;
            g->notice_time = 0;
        }
        if (in.pressed & IN_R) {
            g->tab = (g->tab + 1) % SHOP_TABS;
            g->cursor = 0;
            g->notice_time = 0;
        }
        int n = shop_count(g->tab);
        if (in.pressed & IN_UP) {
            g->cursor = (g->cursor + n - 1) % n;
            g->notice_time = 0;
        }
        if (in.pressed & IN_DOWN) {
            g->cursor = (g->cursor + 1) % n;
            g->notice_time = 0;
        }
        if (in.held & IN_LEFT)
            g->preview_yaw -= dt * 1.5f;
        if (in.held & IN_RIGHT)
            g->preview_yaw += dt * 1.5f;
        if (in.pressed & IN_X)
            g->preview_yaw = 0;
        if (in.pressed & IN_A)
            buy(g);
        if (in.touch) {
            if (in.touch_y >= 52 && in.touch_y < 164) {
                int i = (g->cursor / 4) * 4 + (in.touch_y - 52) / 28;
                if (i < n) {
                    g->cursor = i;
                    g->notice_time = 0;
                }
            } else if (in.touch_y >= 187 && in.touch_y < 211) {
                int i = g->cursor + (in.touch_x < 160 ? -4 : 4);
                g->cursor = i < 0 ? n - 1 : i >= n ? 0 : i;
                g->notice_time = 0;
            } else if (in.touch_y >= 215 && in.touch_x >= 120)
                buy(g);
        }
        return;
    }
    int n = g->screen == TITLE ? 5 : g->screen == PAUSE ? 5 : 4;
    if (in.pressed & IN_SELECT) {
        settings(g);
        return;
    }
    if (in.pressed & IN_UP)
        g->cursor = (g->cursor + n - 1) % n;
    if (in.pressed & IN_DOWN)
        g->cursor = (g->cursor + 1) % n;
    if (g->screen == PAUSE && (in.pressed & (IN_B | IN_START))) {
        g->screen = PLAY;
        return;
    }
    bool activate = (in.pressed & IN_A) != 0;
    if (in.touch && in.touch_y >= 55 && in.touch_y < 55 + n * 28) {
        g->cursor = (in.touch_y - 55) / 28;
        activate = true;
    }
    if (!activate)
        return;
    if (g->screen == TITLE) {
        switch (g->cursor) {
        case 0:
            game_start(g);
            break;
        case 1:
            garage(g);
            break;
        case 2:
            settings(g);
            break;
        case 3:
            g->return_screen = TITLE;
            g->old_cursor = 3;
            g->screen = HELP;
            break;
        case 4:
            g->quit = true;
            break;
        }
    } else if (g->screen == PAUSE) {
        switch (g->cursor) {
        case 0:
            g->screen = PLAY;
            break;
        case 1:
            g->map_return = PAUSE;
            g->screen = CITYMAP;
            break;
        case 2:
            settings(g);
            break;
        case 3:
            g->return_screen = PAUSE;
            g->old_cursor = 3;
            g->screen = HELP;
            break;
        case 4:
            game_end(g, 2);
            break;
        }
    } else if (g->screen == RESULT) {
        switch (g->cursor) {
        case 0:
            game_start(g);
            break;
        case 1:
            garage(g);
            break;
        case 2:
            g->screen = TITLE;
            g->cursor = 0;
            break;
        case 3:
            g->quit = true;
            break;
        }
    }
}

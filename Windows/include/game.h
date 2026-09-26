#ifndef COAST_GAME_H
#define COAST_GAME_H
#include <stdbool.h>
#include <stdint.h>
#define PI 3.14159265358979323846f
#define CELL 64.0f
#define ROAD 18.0f
#define MAP_SIDE 32
#define MAP_TILES (MAP_SIDE * MAP_SIDE)
#define MAX_TRAFFIC 12
#define MAX_POLICE 24
#define MAX_ACTORS (MAX_TRAFFIC + MAX_POLICE)
#define MAX_HELIS 3
#define MAX_PARTICLES 128
#define CACHE_SIDE 13
#define CAR_COUNT 32
#define SHORE_SIDE (MAP_SIDE * 4 + 1)
#define SHORE_STEP (CELL / 4)
#define PAINT_COUNT 8
#define ANTENNA_COUNT 8
#define HAT_COUNT 8
#define THEME_COUNT 5
#define DECAL_COUNT 8
#define WHEEL_COUNT 7
#define ENGINE_COUNT 6
#define SETTING_COUNT 17
#define MAX_PEOPLE 40
#define MAX_DROPS 5
#define LANDMARK_COUNT 20
#define SHOP_TABS 9

typedef struct {
    float x, y, z;
} Vec3;
typedef struct {
    float r, g, b, a;
} Color;
typedef struct {
    float x, z, vx, vz, yaw, steer, hp, nitro, wheel_spin, lean, pitch;
} Vehicle;
typedef struct {
    const char *name;
    int price;
    float speed, accel, grip, armor, width, length, height;
} CarSpec;
typedef struct {
    const char *name;
    Color outline, accent, sky, sea;
} Theme;
typedef struct {
    const char *name, *description;
    int price;
    float speed, accel, grip;
} PartSpec;
typedef struct {
    uint8_t paint, antenna, hat, decal, wheel, engine;
} CarStyle;
enum {
    SHOP_CARS,
    SHOP_UPGRADES,
    SHOP_PAINT,
    SHOP_DECALS,
    SHOP_WHEELS,
    SHOP_ENGINES,
    SHOP_ANTENNAS,
    SHOP_HATS,
    SHOP_THEMES
};
enum {
    OCEAN,
    DOWNTOWN,
    OLD_TOWN,
    SUBURBS,
    MARINA,
    PARK,
    INDUSTRY,
    BEACH,
    CIVIC,
    BRIDGE_EW,
    BRIDGE_NS
};
enum { CIVILIAN, PATROL, POLICE_SUV, SWAT, INTERCEPTOR };
typedef struct {
    float x, z, w, d, h;
    uint8_t style, windows_x, windows_z, floors, door, roof, palette, business;
} Building;
typedef struct {
    int x, z;
    uint32_t hash;
    Building b[8];
    uint8_t count, park, kind, roads, coast, landmark;
} Chunk;
typedef struct {
    uint8_t kind, roads, landmark;
    int8_t ox, oz;
} Tile;
typedef struct {
    Chunk chunks[MAP_TILES];
    Tile tiles[MAP_TILES];
    int cx, cz, radius, spawn, land_count, bridge_count;
    uint32_t seed;
    unsigned generated;
    int layout;
    int16_t shore[SHORE_SIDE * SHORE_SIDE];
    uint8_t shore_kind[MAP_TILES];
    char name[40];
} World;
typedef struct {
    int preset, view, bloom, outline, detail, shadows, particles, shake, fov, fps, fog, white,
        traffic, camera, sfx, music, volume, showfps, steering, hints, lighting, weather, auto_lod;
} Settings;
typedef struct {
    uint32_t money, best, run_count, cars, paints, antennas, hats, themes, decals, wheels, engines;
    uint8_t selected, theme, upgrades[CAR_COUNT][4];
    CarStyle styles[CAR_COUNT];
    Settings settings;
} CoastProfile;
typedef struct {
    Vehicle v;
    float tx, tz, age, stuck, hit_cool, recovery, think, control, desired, commit, charge_x,
        charge_z;
    bool direct;
    int dir, kind, active, passed, node, target, model, variant;
} Actor;
typedef struct {
    Vec3 p;
    float yaw, rotor, spot, altitude, think;
    bool active;
} Helicopter;
typedef struct {
    float x, z, tx, tz, yaw, walk, pause;
    int node, target, side, stage, steps;
    uint32_t rng;
    bool active;
} Pedestrian;
enum { POWER_GIANT, POWER_SHIELD, POWER_MAGNET, POWER_REPAIR };
typedef struct {
    Vec3 p;
    float life;
    int type;
    bool active;
} SupplyDrop;
typedef struct {
    Vec3 p, v;
    Color color;
    float life, maxlife;
} Particle;
typedef struct {
    int x, z, type, node;
    uint32_t key;
    float px, pz;
    bool active;
} Pickup;
typedef enum { TITLE, PLAY, PAUSE, GARAGE, SHOP, SETTINGS, RESULT, HELP, CITYMAP } Screen;
enum {
    IN_A = 1,
    IN_B = 2,
    IN_X = 4,
    IN_Y = 8,
    IN_L = 16,
    IN_R = 32,
    IN_START = 64,
    IN_SELECT = 128,
    IN_UP = 256,
    IN_DOWN = 512,
    IN_LEFT = 1024,
    IN_RIGHT = 2048,
    IN_MAP = 4096
};
typedef struct {
    unsigned held, pressed;
    float steer;
    int touch_x, touch_y;
    bool touch;
} Input;
typedef struct {
    CoastProfile profile;
    World world;
    Vehicle player;
    Actor actors[MAX_ACTORS];
    Helicopter helis[MAX_HELIS];
    Particle particles[MAX_PARTICLES];
    Pedestrian people[MAX_PEOPLE];
    SupplyDrop drops[MAX_DROPS];
    Helicopter helper;
    float giant, shield, magnet, supply_clock, people_clock;
    int supply_serial, landmark_bits, landmark_count, sprint_node, sprint_count;
    float sprint_time, combo_time;
    float property_damage, drift_score, drift_chain, drift_grace;
    int combo;
    Pickup pickups[25];
    uint8_t collected[MAP_TILES], visited[MAP_TILES];
    int16_t chase_field[MAP_TILES], gps_field[MAP_TILES];
    Screen screen, return_screen, map_return;
    int cursor, tab, settings_cursor, old_cursor, waypoint, last_tile;
    uint32_t rng, seed, fx_rng;
    float time, run_time, distance, drift_distance, heat, unseen, bust, hit_cool, spawn_clock,
        nav_clock;
    float camera_yaw, camera_x, camera_z, camera_height, shake, flash, notice_time, recover_time,
        preview_yaw;
    int earned, escapes, near_misses, peak_heat, mission_bits, last_payout, reason, wave, takedowns,
        explored;
    bool new3ds, quit, dirty, save_failed, rear, boosting, drifting, high_camera, confirm_purchase;
    unsigned audio_events;
    char notice[64];
    float measured_fps;
} Game;
extern const CarSpec CARS[CAR_COUNT];
extern const Color PAINTS[PAINT_COUNT];
extern const char *PAINT_NAMES[PAINT_COUNT], *ANTENNA_NAMES[ANTENNA_COUNT], *HAT_NAMES[HAT_COUNT],
    *DECAL_NAMES[DECAL_COUNT], *SHOP_NAMES[SHOP_TABS];
extern const PartSpec ENGINES[ENGINE_COUNT], WHEELS[WHEEL_COUNT];
extern const Theme THEMES[THEME_COUNT];
extern const int ROAD_DX[4], ROAD_DZ[4];
float clampf(float x, float a, float b);
float angle_delta(float a, float b);
float vehicle_speed(const Vehicle *v);
uint32_t hash_cell(int x, int z, uint32_t seed);
uint32_t random_u32(Game *g);
void world_init(World *w, uint32_t seed);
const Chunk *world_chunk(World *w, int x, int z);
void world_stream(World *w, float x, float z, int radius);
const Tile *world_tile(const World *w, int x, int z);
int world_node(float x, float z);
Vec3 world_center(const World *w, int node);
int world_neighbor(const World *w, int node, int dir);
int world_nearest(const World *w, float x, float z);
void world_distances(const World *w, int target, int16_t field[MAP_TILES]);
float world_coast(const World *w, float x, float z);
bool world_surface(const World *w, float x, float z);
bool world_solid(World *w, float x, float z, float radius);
bool world_resolve(World *w, Vehicle *v, float radius, float *impact);
const char *district_name(int kind);
void settings_preset(Settings *s, int preset);
void profile_defaults(CoastProfile *p, bool new3ds);
bool profile_load(CoastProfile *p, const char *path, bool new3ds);
bool profile_save(const CoastProfile *p, const char *path);
void drive_step(Game *g, Vehicle *v, float throttle, float brake, float steering, bool drift,
                bool boost, int model, float dt);
void police_step(Game *g, float dt);
void ambience_step(Game *g, float dt);
void supply_collect(Game *g, int type);
float player_scale(const Game *g);
float player_radius(const Game *g);
void vehicle_contact(Game *g, Actor *a);
void game_arrest_step(Game *g, float dt);
void vehicle_resolve(Game *g, Vehicle *v, int model, float *impact);
const char *landmark_name(int type);
void menus_step(Game *g, Input in, float dt);
void game_init(Game *g, bool new3ds, uint32_t seed);
void game_start(Game *g);
void game_update(Game *g, Input in, float dt);
void game_end(Game *g, int reason);
void game_notice(Game *g, const char *text);
void game_particle(Game *g, float x, float y, float z, Color color, int count);
void game_purchase(Game *g);
float game_pressure(const Game *g);
void game_drift_score(Game *g, float traveled, float dt);
int game_police_budget(float seconds);
int game_heli_budget(float seconds);
void game_set_waypoint(Game *g, int node);
int shop_count(int tab);
int shop_price(const Game *g, int tab, int item);
bool shop_owned(const Game *g, int tab, int item);
bool shop_equipped(const Game *g, int tab, int item);
const char *shop_name(int tab, int item);
const char *shop_description(int tab, int item);
CarSpec car_tuned(const CoastProfile *p, int car, int engine_override, int wheel_override);
CarStyle preview_style(const Game *g);
int preview_car(const Game *g);
int preview_theme(const Game *g);
void setting_change(Game *g, int index, int delta);
const char *setting_name(int i);
void setting_value(const Settings *s, int i, char *out, int size);
#endif

#ifndef GAME_H
#define GAME_H

#include <pspkernel.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspaudio.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define WORLD_W 3400.0f
#define WORLD_H 2000.0f

#define MAX_UNITS 256
#define MAX_PARTICLES 256
#define MAX_MISSIONS 16
#define FOREST_TREES 48
#define MINE_MARKERS 40

typedef enum {
    ROLE_OFFICER = 0, ROLE_SNIPER, ROLE_MEDIC, ROLE_SUPPORT,
    ROLE_SOLDIER, ROLE_ANTITANK, ROLE_RADIOMAN, ROLE_TANK, ROLE_COUNT
} Role;

typedef enum {
    FAC_GB = 0, FAC_US, FAC_SU, FAC_DE, FAC_JP, FAC_NPC = -1
} Faction;

typedef enum {
    ORD_NONE = 0, ORD_MOVE, ORD_ATTACK, ORD_DEFEND
} Order;

typedef enum {
    ST_TITLE = 0, ST_PLAY, ST_WIN, ST_LOSE, ST_PAUSE
} GameStateId;

typedef struct {
    float hp, maxHp;
    float speed, range, dps;
    float setupTime;
    float healRange, healRate;
    float auraRange, auraBoost;
    float maxAmmo;
    float antiTankBonus;
    const char* label;
} RoleDef;

extern const RoleDef ROLE_DEFS[ROLE_COUNT];

typedef struct {
    char id[16];
    int faction;
    int role;
    float x, y, angle;
    float hp, maxHp;
    int dead;
    float ammo;
    float maxAmmo;
    int order;
    float tx, ty;
    float shootCd;
    int grenades;
    float grenadeCd;
    float deadAt;
    float aiTimer;
    int selected;
} Unit;

typedef struct {
    int kind;
    float cx, cy;
    float r;
    float hp, maxHp;
    int destroyed;
    const char* label;
    int destroyer;
} Mission;

typedef struct {
    float x1, x2, y1, y2;
    float hp, maxHp;
    int destroyed;
    int isObjective;
} Bridge;

typedef struct { float x, y, w, h; int villageId; } House;

typedef struct {
    float x, y, r;
    float trees[FOREST_TREES * 2];
    int ntree;
} Forest;

typedef struct { float x, y, r, hp, maxHp; int destroyed; } Bunker;
typedef struct { float x, y, r, hp, maxHp; int destroyed; } Arty;
typedef struct { float x, y, r, hp, maxHp; int destroyed; } Hq;
typedef struct { float x, y, r; float markers[MINE_MARKERS * 2]; int n; } Mine;

typedef struct {
    float riverX1, riverX2;
    Bridge bridges[4]; int nBridge;
    House houses[48]; int nHouse;
    Forest forests[8]; int nForest;
    Bunker bunkers[4]; int nBunker;
    Arty artillery[3]; int nArty;
    Hq hqs[1]; int nHq;
    Mine mines[3]; int nMine;
    float grass[90 * 3]; int nGrass;
} World;

typedef struct {
    int state;
    int playerFaction;
    int factionCursor;
    float camX, camY, zoom;
    float curX, curY;
    int orderMode;
    int respawns;
    float respawnCd;
    int missionDone, missionTotal;
    float time;
    float shake;
} GameState;

extern GameState G;
extern World world;
extern Unit units[MAX_UNITS];
extern int unitCount;
extern Mission missions[MAX_MISSIONS];
extern int missionCount;

typedef struct {
    float x, y, vx, vy;
    float life, maxLife;
    float size;
    float r, g, b;
    int kind;
} Particle;

extern Particle particles[MAX_PARTICLES];

void game_init(void);
void game_new_battle(int playerFaction);
void game_update(float dt);
void spawn_platoon(int ownerId, int faction, float x, float y);
void issue_order_to_selected(int order, float x, float y);
void respawn_player(void);
const char* log_line(int i);

void gfx_init(void);
void render_begin(void);
void render_end(void);
void render_title(void);
void render_play(void);
void render_paused(void);
void render_end_screen(int win);

void audio_init(void);
void audio_update(void);
void sfx_shot(void);
void sfx_explosion(void);
void sfx_death(void);
void sfx_ui(void);
int alliance_of(int f);
int hostile(int a, int b);
float distf(float ax, float ay, float bx, float by);
void add_particle(float x, float y, float vx, float vy, float life, float size, float r, float g, float b, int kind);
void add_explosion(float x, float y, float radius);
void log_push(const char* msg);
const char* faction_name(int f);
unsigned int faction_color(int f);

#endif

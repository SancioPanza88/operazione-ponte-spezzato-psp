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
#define MAX_TRACERS 64
#define MAX_SHELLS 32
#define MAX_PLANES 8
#define MAX_SUPPLIES 16
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
    ORD_NONE = 0, ORD_MOVE, ORD_ATTACK, ORD_DEFEND, ORD_SUPPRESS, ORD_RETREAT, ORD_FORTIFY
} Order;

typedef enum {
    ABIL_NONE = 0, ABIL_ARTILLERY, ABIL_AIRSTRIKE, ABIL_PARATROOP, ABIL_RESUPPLY, ABIL_TANK, ABIL_COUNT
} Ability;

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
    float range, dps, speed;
    int order;
    float tx, ty;
    float shootCd;
    int grenades;
    float grenadeCd;
    float deadAt;
    float aiTimer;
    int fortified;
    float fortifyTimer;
    float suppressedUntil;
    int isTank;
    float reloadTimer;
    int descending;
    float descendStart, descendLife;
    float startX, startY, landX, landY;
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
    int type; // 0: destroy, 1: defend
    float progress, threshold;
    int failed;
} Mission;

typedef struct {
    float x1, x2, y1, y2;
    float hp, maxHp;
    int destroyed;
    int isObjective;
    const char* label;
} Bridge;

typedef struct { float x, y, w, h; int villageId; } House;

typedef struct {
    float x, y, r;
    float trees[FOREST_TREES * 2];
    int ntree;
} Forest;

typedef struct { float x, y, r, hp, maxHp; int destroyed; const char* label; } Bunker;
typedef struct { float x, y, r, hp, maxHp; int destroyed; const char* label; } Arty;
typedef struct { float x, y, r, hp, maxHp; int destroyed; const char* label; } Hq;
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
    float x1, y1, x2, y2;
    float life, maxLife;
    unsigned int color;
} Tracer;

typedef struct {
    float x1, y1, x2, y2;
    float createdAt, life;
    int done;
    int faction;
    float dmg, radius;
    int noArc;
} Projectile;

typedef struct {
    float x1, y1, x2, y2;
    float createdAt, life;
} Plane;

typedef struct {
    float x, y, startX, startY;
    float createdAt, triggerAt;
    int landed;
    float radius;
    int capacity, remaining;
} SupplyDrop;

typedef struct {
    int state;
    int playerFaction;
    int factionCursor;
    float camX, camY, zoom;
    float curX, curY;
    int orderMode;
    int armedAbility;
    float abilityCd[ABIL_COUNT];
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
extern Tracer tracers[MAX_TRACERS];
extern Projectile projectiles[MAX_SHELLS];
extern Plane planes[MAX_PLANES];
extern SupplyDrop supplies[MAX_SUPPLIES];

typedef struct {
    float x, y, vx, vy;
    float life, maxLife;
    float size;
    float r, g, b;
    int kind;
} Particle;

extern Particle particles[MAX_PARTICLES];

typedef struct { unsigned char* data; int w, h; } Tex;
extern Tex tex_tank[6];
extern Tex tex_soldier[7];
extern Tex tex_bridge[2];
extern Tex tex_bunker[2];
extern Tex tex_arty[2];
extern Tex tex_hq[2];
extern Tex tex_house;
extern Tex tex_tree[2];
void load_assets(void);
void blit(const Tex* t, float cx, float cy, float scale, float ang, int alpha);

void game_init(void);
void game_new_battle(int playerFaction);
void game_update(float dt);
void spawn_platoon(int ownerId, int faction, float x, float y);
void issue_order_to_selected(int order, float x, float y);
void trigger_officer_ability(int ability, float x, float y);
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
void add_tracer(float x1, float y1, float x2, float y2, unsigned int color, float life);
void log_push(const char* msg);
const char* faction_name(int f);
unsigned int faction_color(int f);
int isInCover(float x, float y);

#endif

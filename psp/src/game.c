#include "game.h"

const RoleDef ROLE_DEFS[ROLE_COUNT] = {
    { 120, 120, 55, 95, 9, 0, 0, 0, 130, 0.25, 40, "UFFICIALE" },
    { 65, 65, 38, 260, 16, 0.4f, 0, 0, 0, 0, 30, "CECCHINO" },
    { 90, 90, 56, 40, 3, 0, 75, 7, 0, 0, 20, "MEDICO" },
    { 140, 140, 34, 150, 15, 1.6f, 0, 0, 0, 0, 180, "SUPPORTO" },
    { 100, 100, 60, 110, 10, 0, 0, 0, 0, 0, 60, "SOLDATO" },
    { 100, 100, 52, 130, 7, 0.3f, 0, 0, 0, 0, 12, "ANTICARRO" },
    { 90, 90, 56, 70, 5, 0, 0, 0, 0, 0, 32, "MARCONISTA" },
    { 420, 420, 24, 220, 26, 0.8f, 0, 0, 0, 0, 1000000.0f, "CARRO" },
};

typedef struct { const char* name; unsigned int color; unsigned int accent; int alliance; float sx, sy; } FacInfo;
static const FacInfo FAC_INFO[5] = {
    { "GRAN BRETAGNA", 0x4b5d3a, 0xd9c98a, 0, 180, 420 },
    { "STATI UNITI", 0x6b5a3a, 0xc9a227, 0, 180, 1580 },
    { "UNIONE SOVIETICA", 0x7a3b3b, 0xc2a24a, 0, 180, 1000 },
    { "GERMANIA", 0x5c6470, 0xd8dde0, 1, 3220, 420 },
    { "GIAPPONE", 0x8a7233, 0xb23c3c, 1, 3220, 1580 },
};

static const int PLATOON[7] = {
    ROLE_OFFICER, ROLE_SNIPER, ROLE_MEDIC, ROLE_SUPPORT, ROLE_SOLDIER, ROLE_ANTITANK, ROLE_RADIOMAN
};

static unsigned int g_rng = 0x12345678;
static float rndf(void) { g_rng = g_rng * 1664525u + 1013904223u; return (float)(g_rng & 0x7fffffff) / (float)0x7fffffff; }

GameState G;
World world;
Unit units[MAX_UNITS];
int unitCount = 0;
Mission missions[MAX_MISSIONS];
int missionCount = 0;
Particle particles[MAX_PARTICLES];

typedef struct { float x, y, r; } Placed;
static Placed g_placed[64];
static int g_nplaced;
static float g_px, g_py;
static unsigned int g_wseed;

static char logBuf[6][64];
static int logIdx = 0;

void log_push(const char* msg) {
    if (!msg) return;
    strncpy(logBuf[logIdx], msg, 63);
    logBuf[logIdx][63] = 0;
    logIdx = (logIdx + 1) % 6;
}

const char* log_line(int i) {
    if (i < 0 || i > 5) return "";
    int k = (logIdx - 1 - i);
    k = ((k % 6) + 6) % 6;
    return logBuf[k];
}

const char* faction_name(int f) {
    if (f == FAC_NPC) return "DIFESA NEMICA";
    if (f >= 0 && f < 5) return FAC_INFO[f].name;
    return "?";
}

unsigned int faction_color(int f) {
    if (f == FAC_NPC) return 0x2b2a26;
    if (f >= 0 && f < 5) return FAC_INFO[f].color;
    return 0x888888;
}

int alliance_of(int f) {
    if (f == FAC_NPC) return -1;
    if (f >= 0 && f < 5) return FAC_INFO[f].alliance;
    return -1;
}

int hostile(int a, int b) {
    if (a == FAC_NPC || b == FAC_NPC) return 1;
    if (a < 0 || b < 0) return 0;
    return alliance_of(a) != alliance_of(b);
}

float distf(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return (float)sqrtf(dx * dx + dy * dy);
}

static unsigned int hashStringToSeed(const char* str) {
    unsigned int h = 2166136261u;
    while (*str) { h ^= (unsigned char)(*str); h *= 16777619u; str++; }
    return h;
}

static float w_rand(void) {
    g_wseed = (g_wseed + 0x6D2B79F5u) | 0;
    unsigned int t = (g_wseed ^ (g_wseed >> 15)) * (1u | g_wseed);
    t = (t + ((t ^ (t >> 7)) * 61u | t)) ^ t;
    return ((t ^ (t >> 14)) >> 0) / 4294967296.0f;
}
static int w_randInt(int a, int b) { return (int)(a + w_rand() * (b - a + 1)); }

static int pickPoint(float minDist, float margin) {
    for (int t = 0; t < 50; t++) {
        float x = 300 + w_rand() * (WORLD_W - 600);
        float y = 200 + w_rand() * (WORLD_H - 400);
        if (x > world.riverX1 - margin && x < world.riverX2 + margin) continue;
        int ok = 1;
        for (int p = 0; p < g_nplaced; p++) {
            if (distf(x, y, g_placed[p].x, g_placed[p].y) < (minDist > g_placed[p].r ? minDist : g_placed[p].r)) { ok = 0; break; }
        }
        if (ok) { g_px = x; g_py = y; return 1; }
    }
    g_px = 300 + w_rand() * (WORLD_W - 600);
    g_py = 200 + w_rand() * (WORLD_H - 400);
    return 0;
}

static void generateWorld(unsigned int seed) {
    g_wseed = seed;
    g_nplaced = 0;
    memset(&world, 0, sizeof(World));

    float riverX = WORLD_W * 0.44f + w_rand() * (WORLD_W * 0.12f);
    float riverW = 85 + w_rand() * 30;
    world.riverX1 = riverX;
    world.riverX2 = riverX + riverW;

    int bridgeCount = w_randInt(3, 4);
    const char* blabels[4] = { "PONTE NORD", "PONTE C.NORD", "PONTE C.SUD", "PONTE SUD" };
    (void)blabels;
    float segH = WORLD_H / bridgeCount;
    for (int i = 0; i < bridgeCount; i++) {
        float cy = segH * i + (WORLD_H / bridgeCount) * (0.28f + w_rand() * 0.44f);
        int isObj = i < 2;
        Bridge* b = &world.bridges[world.nBridge++];
        b->x1 = riverX - 25; b->x2 = riverX + riverW + 25;
        b->y1 = cy - 45; b->y2 = cy + 45;
        b->hp = isObj ? 300 : 0; b->maxHp = isObj ? 300 : 0;
        b->destroyed = 0; b->isObjective = isObj;
    }

    int villageCount = w_randInt(2, 3);
    for (int vi = 0; vi < villageCount; vi++) {
        pickPoint(420, 90);
        float cx = g_px, cy = g_py;
        g_placed[g_nplaced].x = cx; g_placed[g_nplaced].y = cy; g_placed[g_nplaced].r = 220; g_nplaced++;
        int houseCount = w_randInt(4, 7);
        for (int hi = 0; hi < houseCount; hi++) {
            float a = w_rand() * 6.28318f, d = 20 + w_rand() * 130;
            float hw = 45 + w_rand() * 27, hh = 38 + w_rand() * 24;
            House* h = &world.houses[world.nHouse++];
            h->x = cx + cosf(a) * d - hw / 2; h->y = cy + sinf(a) * d - hh / 2;
            h->w = hw; h->h = hh; h->villageId = vi;
        }
    }

    int forestCount = w_randInt(4, 6);
    for (int i = 0; i < forestCount; i++) {
        pickPoint(300, 60);
        float cx = g_px, cy = g_py;
        float r = 100 + w_rand() * 70;
        g_placed[g_nplaced].x = cx; g_placed[g_nplaced].y = cy; g_placed[g_nplaced].r = r; g_nplaced++;
        Forest* f = &world.forests[world.nForest++];
        f->x = cx; f->y = cy; f->r = r;
        unsigned int fseed = (unsigned int)(cx * 13 + cy * 7);
        int n = (int)(r / 14);
        if (n > FOREST_TREES) n = FOREST_TREES;
        f->ntree = n;
        for (int j = 0; j < n; j++) {
            fseed = (fseed * 1103515245u + 12345u) & 0x7fffffff;
            float fr = (fseed % 1000) / 1000.0f;
            float a = fr * 6.28318f;
            fseed = (fseed * 1103515245u + 12345u) & 0x7fffffff;
            float fr2 = (fseed % 1000) / 1000.0f;
            float d = fr2 * r * 0.85f;
            f->trees[j * 2] = cosf(a) * d; f->trees[j * 2 + 1] = sinf(a) * d;
        }
    }

    int bunkerCount = w_randInt(2, 3);
    for (int i = 0; i < bunkerCount; i++) {
        pickPoint(380, 80);
        Bunker* b = &world.bunkers[world.nBunker++];
        b->x = g_px; b->y = g_py; b->r = 56;
        b->hp = 350 + w_rand() * 100; b->maxHp = b->hp; b->destroyed = 0;
        g_placed[g_nplaced].x = b->x; g_placed[g_nplaced].y = b->y; g_placed[g_nplaced].r = 150; g_nplaced++;
    }

    int artyCount = w_randInt(1, 2);
    for (int i = 0; i < artyCount; i++) {
        pickPoint(350, 80);
        Arty* a = &world.artillery[world.nArty++];
        a->x = g_px; a->y = g_py; a->r = 40;
        a->hp = 230 + w_rand() * 70; a->maxHp = a->hp; a->destroyed = 0;
        g_placed[g_nplaced].x = a->x; g_placed[g_nplaced].y = a->y; g_placed[g_nplaced].r = 130; g_nplaced++;
    }

    {
        pickPoint(500, 100);
        Hq* h = &world.hqs[world.nHq++];
        h->x = g_px; h->y = g_py; h->r = 70;
        h->hp = 500 + w_rand() * 100; h->maxHp = h->hp; h->destroyed = 0;
        g_placed[g_nplaced].x = h->x; g_placed[g_nplaced].y = h->y; g_placed[g_nplaced].r = 170; g_nplaced++;
    }

    int mineCount = w_randInt(1, 2);
    for (int i = 0; i < mineCount; i++) {
        pickPoint(300, 40);
        float r = 100 + w_rand() * 60;
        Mine* m = &world.mines[world.nMine++];
        m->x = g_px; m->y = g_py; m->r = r;
        unsigned int mseed = (unsigned int)(g_px * 11 + g_py * 17);
        int mk = (int)(r / 22);
        if (mk > MINE_MARKERS) mk = MINE_MARKERS;
        m->n = mk;
        for (int j = 0; j < mk; j++) {
            mseed = (mseed * 1103515245u + 12345u) & 0x7fffffff;
            float fr = (mseed % 1000) / 1000.0f;
            float a = fr * 6.28318f;
            m->markers[j * 2] = cosf(a) * r * 0.92f; m->markers[j * 2 + 1] = sinf(a) * r * 0.92f;
        }
        g_placed[g_nplaced].x = m->x; g_placed[g_nplaced].y = m->y; g_placed[g_nplaced].r = r; g_nplaced++;
    }

    unsigned int gseed = (unsigned int)(w_rand() * 999999);
    for (int i = 0; i < 90; i++) {
        gseed = (gseed * 1103515245u + 12345u) & 0x7fffffff;
        float fr = (gseed % 1000) / 1000.0f;
        world.grass[i * 3] = fr * WORLD_W;
        gseed = (gseed * 1103515245u + 12345u) & 0x7fffffff;
        float fr2 = (gseed % 1000) / 1000.0f;
        world.grass[i * 3 + 1] = fr2 * WORLD_H;
        gseed = (gseed * 1103515245u + 12345u) & 0x7fffffff;
        float fr3 = (gseed % 1000) / 1000.0f;
        world.grass[i * 3 + 2] = fr3;
    }
}

static void buildMissions(void) {
    missionCount = 0;
    for (int i = 0; i < world.nBridge; i++) {
        Bridge* b = &world.bridges[i];
        if (!b->isObjective) continue;
        Mission* m = &missions[missionCount++];
        m->kind = 0; m->cx = (b->x1 + b->x2) / 2; m->cy = (b->y1 + b->y2) / 2;
        m->r = 50; m->hp = b->hp; m->maxHp = b->maxHp; m->destroyed = 0; m->label = "PONTE"; m->destroyer = -2;
    }
    for (int i = 0; i < world.nBunker; i++) {
        Bunker* b = &world.bunkers[i];
        Mission* m = &missions[missionCount++];
        m->kind = 1; m->cx = b->x; m->cy = b->y; m->r = b->r; m->hp = b->hp; m->maxHp = b->maxHp; m->destroyed = 0; m->label = "BUNKER"; m->destroyer = -2;
    }
    for (int i = 0; i < world.nArty; i++) {
        Arty* a = &world.artillery[i];
        Mission* m = &missions[missionCount++];
        m->kind = 2; m->cx = a->x; m->cy = a->y; m->r = a->r; m->hp = a->hp; m->maxHp = a->maxHp; m->destroyed = 0; m->label = "ARTIGLIERIA"; m->destroyer = -2;
    }
    for (int i = 0; i < world.nHq; i++) {
        Hq* h = &world.hqs[i];
        Mission* m = &missions[missionCount++];
        m->kind = 3; m->cx = h->x; m->cy = h->y; m->r = h->r; m->hp = h->hp; m->maxHp = h->maxHp; m->destroyed = 0; m->label = "QG"; m->destroyer = -2;
    }
    G.missionTotal = missionCount;
}

static void spawn_squad(int owner, int faction, float x, float y, const int* roles, int n) {
    (void)owner;
    for (int i = 0; i < n; i++) {
        if (unitCount >= MAX_UNITS) break;
        Unit* u = &units[unitCount];
        memset(u, 0, sizeof(Unit));
        u->faction = faction; u->role = roles[i];
        float a = i * 0.9f;
        u->x = x + cosf(a) * 18.0f; u->y = y + sinf(a) * 18.0f;
        const RoleDef* d = &ROLE_DEFS[u->role];
        u->maxHp = d->hp; u->hp = d->hp;
        u->maxAmmo = d->maxAmmo; u->ammo = d->maxAmmo;
        u->range = d->range; u->dps = d->dps; u->speed = d->speed;
        u->order = ORD_NONE; u->grenades = 3; u->dead = 0; u->angle = 0;
        sprintf(u->id, "F%dU%d", faction, unitCount);
        unitCount++;
    }
}

void spawn_platoon(int owner, int faction, float x, float y) {
    spawn_squad(owner, faction, x, y, PLATOON, 7);
}

static int nearestMissionIdx(float x, float y) {
    int best = -1; float bd = 1e18f;
    for (int i = 0; i < missionCount; i++) {
        if (missions[i].destroyed) continue;
        float d = distf(x, y, missions[i].cx, missions[i].cy);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static Unit* nearestHostileUnit(Unit* u, float maxd) {
    Unit* best = 0; float bd = maxd;
    for (int i = 0; i < unitCount; i++) {
        Unit* o = &units[i];
        if (o->dead) continue;
        if (!hostile(u->faction, o->faction)) continue;
        float d = distf(u->x, u->y, o->x, o->y);
        if (d < bd) { bd = d; best = o; }
    }
    return best;
}

static Mission* nearestMission(Unit* u, float maxd) {
    Mission* best = 0; float bd = maxd;
    for (int i = 0; i < missionCount; i++) {
        Mission* m = &missions[i];
        if (m->destroyed) continue;
        float d = distf(u->x, u->y, m->cx, m->cy);
        if (d < bd) { bd = d; best = m; }
    }
    return best;
}

static void damageMission(Mission* m, float dmg, int by) {
    if (m->destroyed) return;
    m->hp -= dmg;
    if (m->hp <= 0) {
        m->hp = 0; m->destroyed = 1; m->destroyer = by;
        if (m->kind == 1) for (int i = 0; i < world.nBunker; i++) if (!world.bunkers[i].destroyed && distf(world.bunkers[i].x, world.bunkers[i].y, m->cx, m->cy) < world.bunkers[i].r + 5) world.bunkers[i].destroyed = 1;
        if (m->kind == 2) for (int i = 0; i < world.nArty; i++) if (!world.artillery[i].destroyed && distf(world.artillery[i].x, world.artillery[i].y, m->cx, m->cy) < world.artillery[i].r + 5) world.artillery[i].destroyed = 1;
        if (m->kind == 3) for (int i = 0; i < world.nHq; i++) if (!world.hqs[i].destroyed && distf(world.hqs[i].x, world.hqs[i].y, m->cx, m->cy) < world.hqs[i].r + 5) world.hqs[i].destroyed = 1;
        add_explosion(m->cx, m->cy, 60);
        sfx_explosion();
        G.shake = (float)fmaxf(G.shake, 3.0f);
        char buf[64]; sprintf(buf, "%s DISTRUTTO!", m->label);
        log_push(buf);
    }
}

void add_particle(float x, float y, float vx, float vy, float life, float size, float r, float g, float b, int kind) {
    static int cur = 0;
    Particle* p = &particles[cur];
    cur = (cur + 1) % MAX_PARTICLES;
    p->x = x; p->y = y; p->vx = vx; p->vy = vy;
    p->life = life; p->maxLife = life; p->size = size;
    p->r = r; p->g = g; p->b = b; p->kind = kind;
}

void add_explosion(float x, float y, float radius) {
    add_particle(x, y, 0, 0, 0.34f, radius * 0.6f, 1, 0.9f, 0.6f, 1);
    for (int i = 0; i < 22; i++) {
        float a = rndf() * 6.28318f, sp = 20 + rndf() * radius * 3;
        add_particle(x, y, cosf(a) * sp, sinf(a) * sp, 0.3f + rndf() * 0.5f, 1 + rndf() * 3,
            rndf() < 0.5f ? 1.0f : 0.9f, 0.8f, 0.25f, 0);
    }
    for (int i = 0; i < 10; i++) {
        float a = rndf() * 6.28318f, sp = 4 + rndf() * 26;
        add_particle(x + rndf() * 6, y + rndf() * 6, cosf(a) * sp, sinf(a) * sp - 16, 1 + rndf(), 9 + rndf() * 12, 0.15f, 0.14f, 0.13f, 2);
    }
}

static void update_particles(float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &particles[i];
        if (p->life <= 0) continue;
        p->life -= dt;
        if (p->kind == 1) { p->size += 120 * dt; }
        else if (p->kind == 2) { p->size += 18 * dt; p->y -= 10 * dt; }
        else { p->x += p->vx * dt; p->y += p->vy * dt; p->vx *= 0.98f; p->vy *= 0.98f; }
    }
}

void issue_order_to_selected(int order, float x, float y) {
    for (int i = 0; i < unitCount; i++) {
        Unit* u = &units[i];
        if (u->dead) continue;
        if (u->faction != G.playerFaction) continue;
        u->order = order; u->tx = x; u->ty = y;
    }
    G.orderMode = order;
}

void game_init(void) {
    generateWorld(hashStringToSeed("ROOM_SEED_BASE::0"));
    buildMissions();
    G.state = ST_TITLE;
    G.playerFaction = FAC_GB;
    G.factionCursor = 0;
    G.zoom = 1;
    G.respawns = 5;
    G.respawnCd = -1;
}

void game_new_battle(int playerFaction) {
    G.playerFaction = playerFaction;
    unitCount = 0;
    memset(units, 0, sizeof(units));
    memset(particles, 0, sizeof(particles));
    memset(missions, 0, sizeof(missions));
    buildMissions();
    G.missionDone = 0;

    int villageCount = 0;
    for (int i = 0; i < world.nHouse; i++) { if (world.houses[i].villageId + 1 > villageCount) villageCount = world.houses[i].villageId + 1; }
    for (int vi = 0; vi < villageCount; vi++) {
        float cx = 0, cy = 0, cnt = 0;
        for (int i = 0; i < world.nHouse; i++) if (world.houses[i].villageId == vi) { cx += world.houses[i].x; cy += world.houses[i].y; cnt++; }
        if (cnt) { cx /= cnt; cy /= cnt; }
        int pr[4] = { ROLE_OFFICER, ROLE_SOLDIER, ROLE_SOLDIER, ROLE_SOLDIER };
        spawn_squad(100 + vi, FAC_NPC, cx, cy, pr, 4);
        for (int k = unitCount - 4; k < unitCount; k++) { units[k].order = ORD_DEFEND; units[k].tx = cx; units[k].ty = cy; }
    }
    for (int i = 0; i < world.nBunker; i++) {
        int r = (rndf() < 0.5f) ? 4 : 3;
        int roles[4]; roles[0] = ROLE_SUPPORT; roles[1] = ROLE_SOLDIER; roles[2] = ROLE_SOLDIER;
        if (r == 4) roles[3] = ROLE_TANK; else roles[3] = ROLE_SOLDIER;
        spawn_squad(200 + i, FAC_NPC, world.bunkers[i].x, world.bunkers[i].y, roles, r);
        for (int k = unitCount - r; k < unitCount; k++) { units[k].order = ORD_DEFEND; units[k].tx = world.bunkers[i].x; units[k].ty = world.bunkers[i].y; }
    }
    for (int i = 0; i < world.nArty; i++) {
        int ar[2] = { ROLE_SOLDIER, ROLE_SOLDIER };
        spawn_squad(300 + i, FAC_NPC, world.artillery[i].x, world.artillery[i].y, ar, 2);
        for (int k = unitCount - 2; k < unitCount; k++) { units[k].order = ORD_DEFEND; units[k].tx = world.artillery[i].x; units[k].ty = world.artillery[i].y; }
    }
    for (int i = 0; i < world.nHq; i++) {
        int hr[6] = { ROLE_OFFICER, ROLE_SUPPORT, ROLE_SOLDIER, ROLE_SOLDIER, ROLE_SOLDIER, ROLE_TANK };
        spawn_squad(400 + i, FAC_NPC, world.hqs[i].x, world.hqs[i].y, hr, 6);
        for (int k = unitCount - 6; k < unitCount; k++) { units[k].order = ORD_DEFEND; units[k].tx = world.hqs[i].x; units[k].ty = world.hqs[i].y; }
    }

    for (int f = 0; f < 5; f++) {
        spawn_platoon(f, f, FAC_INFO[f].sx, FAC_INFO[f].sy);
        int base = unitCount - 7;
        if (f == playerFaction) {
            for (int k = base; k < unitCount; k++) units[k].order = ORD_NONE;
        } else {
            int mi = nearestMissionIdx(FAC_INFO[f].sx, FAC_INFO[f].sy);
            float gx = FAC_INFO[f].sx, gy = FAC_INFO[f].sy;
            if (mi >= 0) { gx = missions[mi].cx; gy = missions[mi].cy; }
            for (int k = base; k < unitCount; k++) { units[k].order = ORD_ATTACK; units[k].tx = gx; units[k].ty = gy; }
        }
    }

    G.camX = FAC_INFO[playerFaction].sx - 240.0f / G.zoom;
    G.camY = FAC_INFO[playerFaction].sy - 136.0f / G.zoom;
    G.curX = FAC_INFO[playerFaction].sx;
    G.curY = FAC_INFO[playerFaction].sy;
    G.state = ST_PLAY;
    G.time = 0;
    G.respawns = 5;
    G.respawnCd = -1;
    G.orderMode = ORD_NONE;
}

void game_update(float dt) {
    if (G.state == ST_WIN || G.state == ST_LOSE || G.state == ST_TITLE) { update_particles(dt); return; }
    G.time += dt;
    if (G.shake > 0) G.shake = (float)fmaxf(0, G.shake - dt * 6);

    float aura[MAX_UNITS];
    for (int i = 0; i < unitCount; i++) aura[i] = 1;
    for (int i = 0; i < unitCount; i++) {
        Unit* o = &units[i];
        if (o->dead || o->role != ROLE_OFFICER) continue;
        for (int j = 0; j < unitCount; j++) {
            Unit* v = &units[j];
            if (v->dead || v->faction != o->faction) continue;
            if (distf(o->x, o->y, v->x, v->y) < ROLE_DEFS[ROLE_OFFICER].auraRange) aura[j] = 1 + ROLE_DEFS[ROLE_OFFICER].auraBoost;
        }
    }

    for (int i = 0; i < unitCount; i++) {
        Unit* u = &units[i];
        if (u->dead) continue;

        if (u->role == ROLE_MEDIC) {
            for (int j = 0; j < unitCount; j++) {
                Unit* a = &units[j];
                if (a->dead || a->faction != u->faction || a == u) continue;
                if (distf(u->x, u->y, a->x, a->y) < ROLE_DEFS[ROLE_MEDIC].healRange)
                    a->hp = (float)fminf(a->maxHp, a->hp + ROLE_DEFS[ROLE_MEDIC].healRate * dt);
            }
        }

        if (u->faction != G.playerFaction && u->order == ORD_ATTACK) {
            int mi = nearestMissionIdx(u->x, u->y);
            if (mi >= 0) { u->tx = missions[mi].cx; u->ty = missions[mi].cy; }
        }

        Unit* tu = nearestHostileUnit(u, u->range);
        Mission* tm = 0;
        if (u->order == ORD_ATTACK || u->faction == FAC_NPC) tm = nearestMission(u, u->range);

        int fired = 0;
        if (tu && distf(u->x, u->y, tu->x, tu->y) <= u->range) {
            u->angle = (float)atan2f(tu->y - u->y, tu->x - u->x);
            if (u->ammo > 0) {
                tu->hp -= u->dps * aura[i] * dt;
                u->ammo -= dt * 2.5f;
                fired = 1;
                if (u->shootCd <= 0) { u->shootCd = 0.12f; add_particle(u->x + cosf(u->angle) * 12, u->y + sinf(u->angle) * 12, 0, 0, 0.09f, 6, 1, 0.9f, 0.6f, 3); sfx_shot(); }
            }
        } else if (tm && distf(u->x, u->y, tm->cx, tm->cy) <= u->range) {
            u->angle = (float)atan2f(tm->cy - u->y, tm->cx - u->x);
            if (u->ammo > 0) {
                damageMission(tm, u->dps * aura[i] * dt, u->faction);
                u->ammo -= dt * 2.5f;
                fired = 1;
                if (u->shootCd <= 0) { u->shootCd = 0.12f; sfx_shot(); }
            }
        } else {
            float gx = -1, gy = -1;
            if (tu) { gx = tu->x; gy = tu->y; }
            else if (tm) { gx = tm->cx; gy = tm->cy; }
            else if (u->order == ORD_MOVE || u->order == ORD_ATTACK) { gx = u->tx; gy = u->ty; }
            else if (u->order == ORD_DEFEND) {
                if (distf(u->x, u->y, u->tx, u->ty) > 8) { gx = u->tx; gy = u->ty; }
            } else {
                if (u->faction != G.playerFaction) {
                    int mi = nearestMissionIdx(u->x, u->y);
                    if (mi >= 0) { gx = missions[mi].cx; gy = missions[mi].cy; }
                }
            }
            if (gx >= 0) {
                float d = distf(u->x, u->y, gx, gy);
                if (d > 4) {
                    float dx = (gx - u->x) / d, dy = (gy - u->y) / d;
                    u->x += dx * u->speed * dt;
                    u->y += dy * u->speed * dt;
                    u->angle = (float)atan2f(dy, dx);
                    u->x = (float)fmaxf(0, fminf(WORLD_W, u->x));
                    u->y = (float)fmaxf(0, fminf(WORLD_H, u->y));
                }
            }
        }

        if (u->shootCd > 0) u->shootCd -= dt;
        if (fired) { if (u->shootCd <= 0) u->shootCd = 0.12f; }
        else u->ammo = (float)fminf(u->maxAmmo, u->ammo + dt * 3.0f);

        if ((u->role == ROLE_SOLDIER || u->role == ROLE_SUPPORT) && u->grenades > 0 && u->grenadeCd <= 0) {
            Unit* gu = nearestHostileUnit(u, 140);
            if (gu) {
                for (int j = 0; j < unitCount; j++) {
                    Unit* e = &units[j];
                    if (e->dead || !hostile(u->faction, e->faction)) continue;
                    if (distf(gu->x, gu->y, e->x, e->y) < 45) e->hp -= 55;
                }
                add_explosion(gu->x, gu->y, 45);
                u->grenades--; u->grenadeCd = 5.5f;
            }
        }
        if (u->grenadeCd > 0) u->grenadeCd -= dt;

        if (u->hp <= 0) {
            u->dead = 1; u->deadAt = G.time;
            add_particle(u->x, u->y, 0, 0, 0.5f, 8, 0.6f, 0.55f, 0.4f, 0);
            sfx_death();
        }
    }

    update_particles(dt);

    int done = 0, alivePlayer = 0;
    for (int i = 0; i < missionCount; i++) if (missions[i].destroyed) done++;
    G.missionDone = done;
    for (int i = 0; i < unitCount; i++) if (!units[i].dead && units[i].faction == G.playerFaction) alivePlayer++;

    if (done >= missionCount) { G.state = ST_WIN; log_push("OPERAZIONE COMPLETATA!"); return; }

    if (alivePlayer == 0) {
        if (G.respawnCd < 0) G.respawnCd = 6.0f;
        else if (G.respawnCd > 0) G.respawnCd -= dt;
        if (G.respawnCd <= 0 && G.respawns <= 0) { G.state = ST_LOSE; log_push("PLOTONE ANNIENTATO."); }
    } else {
        G.respawnCd = -1;
    }
}

void respawn_player(void) {
    if (G.respawns <= 0 || G.respawnCd > 0) return;
    int f = G.playerFaction;
    spawn_platoon(f, f, FAC_INFO[f].sx, FAC_INFO[f].sy);
    int base = unitCount - 7;
    for (int k = base; k < unitCount; k++) units[k].order = ORD_NONE;
    G.respawns--;
    G.respawnCd = -1;
    log_push("RINFORZI SCHIERATI");
}

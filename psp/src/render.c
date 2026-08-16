#include "game.h"

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

static u32 fb[2][BUF_WIDTH * SCR_HEIGHT] __attribute__((aligned(16)));
static int g_draw = 0;
static u32* g_fb = 0;
static unsigned int __attribute__((aligned(16))) gu_list[262144];

static inline u32 rgb(u8 r, u8 g, u8 b) { return 0xff000000u | ((u32)b << 16) | ((u32)g << 8) | r; }
static inline u32 fc(int c) { return rgb((u8)((c >> 16) & 255), (u8)((c >> 8) & 255), (u8)(c & 255)); }

static void pxset(int x, int y, u32 c) {
    if (x < 0 || x >= SCR_WIDTH || y < 0 || y >= SCR_HEIGHT) return;
    g_fb[y * BUF_WIDTH + x] = c;
}

static void clear_screen(u32 c) {
    for (int i = 0; i < SCR_WIDTH * SCR_HEIGHT; i++) g_fb[i] = c;
}

static void fill_rect(int x0, int y0, int w, int h, u32 c) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            pxset(x, y, c);
}

static void draw_line(int x0, int y0, int x1, int y1, u32 c) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        pxset(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void fill_circle(int cx, int cy, int r, u32 c) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) pxset(cx + x, cy + y, c);
}

static void ring_circle(int cx, int cy, int r, u32 c) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++) {
            int d = x * x + y * y;
            if (d <= r * r && d >= (r - 2) * (r - 2)) pxset(cx + x, cy + y, c);
        }
}

static const unsigned char FONT[128][7] = {
    [' '] = { 0,0,0,0,0,0,0 },
    ['A'] = { 14,17,17,31,17,17,17 },
    ['B'] = { 30,17,17,30,17,17,30 },
    ['C'] = { 14,17,16,16,16,17,14 },
    ['D'] = { 30,17,17,17,17,17,30 },
    ['E'] = { 31,16,16,30,16,16,31 },
    ['F'] = { 31,16,16,30,16,16,16 },
    ['G'] = { 14,17,16,19,17,17,14 },
    ['H'] = { 17,17,17,31,17,17,17 },
    ['I'] = { 31,4,4,4,4,4,31 },
    ['J'] = { 7,2,2,2,18,18,12 },
    ['K'] = { 17,18,20,24,20,18,17 },
    ['L'] = { 16,16,16,16,16,16,31 },
    ['M'] = { 17,27,21,21,17,17,17 },
    ['N'] = { 17,25,21,19,17,17,17 },
    ['O'] = { 14,17,17,17,17,17,14 },
    ['P'] = { 30,17,17,30,16,16,16 },
    ['Q'] = { 14,17,17,17,21,18,13 },
    ['R'] = { 30,17,17,30,20,18,17 },
    ['S'] = { 14,17,16,14,1,17,14 },
    ['T'] = { 31,4,4,4,4,4,4 },
    ['U'] = { 17,17,17,17,17,17,14 },
    ['V'] = { 17,17,17,17,17,10,4 },
    ['W'] = { 17,17,17,21,21,27,17 },
    ['X'] = { 17,17,10,4,10,17,17 },
    ['Y'] = { 17,17,10,4,4,4,4 },
    ['Z'] = { 31,1,2,4,8,16,31 },
    ['0'] = { 14,17,17,17,17,17,14 },
    ['1'] = { 4,12,4,4,4,4,14 },
    ['2'] = { 14,17,1,6,8,16,31 },
    ['3'] = { 31,2,2,14,2,2,31 },
    ['4'] = { 2,6,10,18,31,2,2 },
    ['5'] = { 31,16,16,30,1,17,14 },
    ['6'] = { 14,16,16,30,17,17,14 },
    ['7'] = { 31,1,2,4,8,8,8 },
    ['8'] = { 14,17,17,14,17,17,14 },
    ['9'] = { 14,17,17,15,1,1,14 },
    [':'] = { 0,4,0,0,0,4,0 },
    ['.'] = { 0,0,0,0,0,0,4 },
    ['!'] = { 4,4,4,4,4,0,4 },
    ['?'] = { 14,17,2,4,0,0,4 },
    ['-'] = { 0,0,0,14,0,0,0 },
    ['/'] = { 1,1,2,4,8,16,16 },
    ['('] = { 2,4,8,8,8,4,2 },
    [')'] = { 8,4,2,2,2,4,8 },
    ['+'] = { 0,4,4,31,4,4,0 },
    ['%'] = { 17,17,9,6,18,17,17 },
};

static void put_char(int x, int y, u32 c, char ch, int scale) {
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch < 32 || ch > 126) ch = 32;
    const unsigned char* g = FONT[(int)ch];
    for (int row = 0; row < 7; row++) {
        unsigned char bits = g[row];
        for (int col = 0; col < 5; col++) {
            if (bits & (1 << (4 - col))) {
                if (scale <= 1) pxset(x + col, y + row, c);
                else fill_rect(x + col * scale, y + row * scale, scale, scale, c);
            }
        }
    }
}

static void put_text(int x, int y, u32 c, const char* s, int scale) {
    if (!s) return;
    int cx = x;
    while (*s) {
        put_char(cx, y, c, *s, scale);
        cx += 6 * scale;
        s++;
    }
}

static int wsx(float wx) { return (int)((wx - G.camX) * G.zoom); }
static int wsy(float wy) { return (int)((wy - G.camY) * G.zoom); }

void gfx_init(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, fb[0], BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, fb[1], BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void render_begin(void) {
    sceGuStart(GU_DIRECT, gu_list);
    g_fb = fb[g_draw];
    clear_screen(rgb(35, 31, 25));
}

void render_end(void) {
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    g_draw ^= 1;
}

static void draw_world(void) {
    int water = rgb(46, 84, 104);
    int bridge = rgb(120, 98, 60);
    int bridgeDead = rgb(40, 36, 30);
    int house = rgb(96, 78, 52);
    int tree = rgb(40, 78, 36);
    int bunker = rgb(90, 86, 78);
    int arty = rgb(110, 80, 60);
    int hq = rgb(120, 110, 70);
    int mine = rgb(150, 40, 40);

    float camX = G.camX, camY = G.camY, z = G.zoom;
    (void)camX; (void)camY; (void)z;

    int rx0 = wsx(world.riverX1), rx1 = wsx(world.riverX2);
    fill_rect(rx0, 0, rx1 - rx0, SCR_HEIGHT, water);

    for (int i = 0; i < world.nGrass; i++) {
        int gx = wsx(world.grass[i * 3]), gy = wsy(world.grass[i * 3 + 1]);
        pxset(gx, gy, (world.grass[i * 3 + 2] > 0.5f) ? rgb(70, 74, 48) : rgb(44, 48, 32));
    }

    for (int i = 0; i < world.nForest; i++) {
        Forest* f = &world.forests[i];
        int fx = wsx(f->x), fy = wsy(f->y);
        if (fx < -120 || fx > SCR_WIDTH + 120 || fy < -120 || fy > SCR_HEIGHT + 120) continue;
        for (int j = 0; j < f->ntree; j++) {
            int tx = wsx(f->x + f->trees[j * 2]);
            int ty = wsy(f->y + f->trees[j * 2 + 1]);
            fill_circle(tx, ty, (int)(7 * z), tree);
        }
    }

    for (int i = 0; i < world.nHouse; i++) {
        House* h = &world.houses[i];
        int hx = wsx(h->x), hy = wsy(h->y);
        int hw = (int)(h->w * z), hh = (int)(h->h * z);
        if (hx + hw < 0 || hx > SCR_WIDTH || hy + hh < 0 || hy > SCR_HEIGHT) continue;
        fill_rect(hx, hy, hw, hh, house);
        draw_line(hx, hy, hx + hw, hy, rgb(60, 48, 32));
    }

    for (int i = 0; i < world.nBridge; i++) {
        Bridge* b = &world.bridges[i];
        int bx = wsx(b->x1), by = wsy(b->y1);
        int bw = (int)((b->x2 - b->x1) * z), bh = (int)((b->y2 - b->y1) * z);
        fill_rect(bx, by, bw, bh, b->destroyed ? bridgeDead : bridge);
    }

    for (int i = 0; i < world.nMine; i++) {
        Mine* m = &world.mines[i];
        int mx = wsx(m->x), my = wsy(m->y);
        int mr = (int)(m->r * z);
        ring_circle(mx, my, mr, mine);
        for (int j = 0; j < m->n; j++) {
            int kx = wsx(m->x + m->markers[j * 2]);
            int ky = wsy(m->y + m->markers[j * 2 + 1]);
            pxset(kx, ky, mine);
        }
    }

    for (int i = 0; i < world.nBunker; i++) {
        Bunker* b = &world.bunkers[i];
        int bx = wsx(b->x), by = wsy(b->y);
        int br = (int)(b->r * z);
        fill_circle(bx, by, br, b->destroyed ? bridgeDead : bunker);
        if (!b->destroyed) {
            int bw = (int)(br * 2 * (b->hp / b->maxHp));
            fill_rect(bx - br, by - br - 4, bw, 3, rgb(180, 60, 60));
        }
    }
    for (int i = 0; i < world.nArty; i++) {
        Arty* a = &world.artillery[i];
        int ax = wsx(a->x), ay = wsy(a->y);
        int ar = (int)(a->r * z);
        fill_circle(ax, ay, ar, a->destroyed ? bridgeDead : arty);
        if (!a->destroyed) {
            int aw = (int)(ar * 2 * (a->hp / a->maxHp));
            fill_rect(ax - ar, ay - ar - 4, aw, 3, rgb(180, 60, 60));
        }
    }
    for (int i = 0; i < world.nHq; i++) {
        Hq* h = &world.hqs[i];
        int hx = wsx(h->x), hy = wsy(h->y);
        int hr = (int)(h->r * z);
        fill_circle(hx, hy, hr, h->destroyed ? bridgeDead : hq);
        if (!h->destroyed) {
            int hw = (int)(hr * 2 * (h->hp / h->maxHp));
            fill_rect(hx - hr, hy - hr - 4, hw, 3, rgb(180, 60, 60));
        }
    }

    for (int i = 0; i < missionCount; i++) {
        Mission* m = &missions[i];
        int mx = wsx(m->cx), my = wsy(m->cy);
        ring_circle(mx, my, (int)(m->r * z) + 4, m->destroyed ? rgb(80, 80, 80) : rgb(217, 166, 79));
    }

    for (int i = 0; i < unitCount; i++) {
        Unit* u = &units[i];
        if (u->dead) continue;
        int ux = wsx(u->x), uy = wsy(u->y);
        if (ux < -20 || ux > SCR_WIDTH + 20 || uy < -20 || uy > SCR_HEIGHT + 20) continue;
        int rad = (u->role == ROLE_TANK) ? (int)(10 * z) : (int)(5 * z);
        u32 col = fc(faction_color(u->faction));
        fill_circle(ux, uy, rad, col);
        if (u->faction == G.playerFaction) ring_circle(ux, uy, rad + 2, rgb(217, 166, 79));
        if (u->role == ROLE_OFFICER) pxset(ux, uy - rad - 2, rgb(255, 230, 120));
        int bw = (int)(rad * 2 * (u->hp / u->maxHp));
        if (bw < 0) bw = 0;
        fill_rect(ux - rad, uy - rad - 4, bw, 2, rgb(120, 220, 120));
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &particles[i];
        if (p->life <= 0) continue;
        int px = wsx(p->x), py = wsy(p->y);
        u32 c = rgb((u8)(p->r * 255), (u8)(p->g * 255), (u8)(p->b * 255));
        if (p->kind == 1) ring_circle(px, py, (int)p->size, c);
        else if (p->kind == 2) fill_circle(px, py, (int)(p->size * 0.4f), c);
        else fill_circle(px, py, (int)(p->size * 0.5f) + 1, c);
    }

    int rx = wsx(G.curX), ry = wsy(G.curY);
    draw_line(rx - 8, ry, rx + 8, ry, rgb(255, 240, 160));
    draw_line(rx, ry - 8, rx, ry + 8, rgb(255, 240, 160));
    ring_circle(rx, ry, 9, rgb(255, 240, 160));
}

static void draw_hud(void) {
    int panel = rgb(28, 26, 18);
    fill_rect(0, 0, SCR_WIDTH, 14, panel);
    char buf[64];
    sprintf(buf, "OBBIETTIVI %d/%d", G.missionDone, G.missionTotal);
    put_text(4, 3, rgb(217, 166, 79), buf, 1);
    sprintf(buf, "RINFORZI %d", G.respawns);
    put_text(180, 3, rgb(200, 200, 180), buf, 1);
    const char* om = "NULLA";
    if (G.orderMode == ORD_MOVE) om = "MUOVI";
    else if (G.orderMode == ORD_ATTACK) om = "ATTACCA";
    else if (G.orderMode == ORD_DEFEND) om = "DIFENDI";
    sprintf(buf, "ORDINE:%s", om);
    put_text(300, 3, rgb(200, 200, 180), buf, 1);

    fill_rect(0, SCR_HEIGHT - 16, SCR_WIDTH, 16, panel);
    put_text(4, SCR_HEIGHT - 14, rgb(200, 200, 180), "X MUOVI O ATTACCA TR DIFENDI SQ RINF. L/R ZOOM START PAUSA", 1);

    for (int i = 0; i < 4; i++) {
        const char* l = log_line(i);
        if (l && l[0]) put_text(4, 20 + i * 9, rgb(240, 233, 210), l, 1);
    }

    int mmx = SCR_WIDTH - 144, mmy = 18, mmw = 140, mmh = (int)(mmw * WORLD_H / WORLD_W);
    fill_rect(mmx, mmy, mmw, mmh, rgb(18, 18, 14));
    float sx = (float)mmw / WORLD_W, sy = (float)mmh / WORLD_H;
    for (int i = 0; i < missionCount; i++) {
        Mission* m = &missions[i];
        int dx = mmx + (int)(m->cx * sx), dy = mmy + (int)(m->cy * sy);
        pxset(dx, dy, m->destroyed ? rgb(120, 120, 120) : rgb(217, 166, 79));
    }
    for (int i = 0; i < unitCount; i++) {
        Unit* u = &units[i];
        if (u->dead) continue;
        int dx = mmx + (int)(u->x * sx), dy = mmy + (int)(u->y * sy);
        pxset(dx, dy, fc(faction_color(u->faction)));
    }
    int vx = mmx + (int)(G.camX * sx), vy = mmy + (int)(G.camY * sy);
    int vw = (int)(SCR_WIDTH / G.zoom * sx), vh = (int)(SCR_HEIGHT / G.zoom * sy);
    draw_line(vx, vy, vx + vw, vy, rgb(255, 255, 255));
    draw_line(vx, vy + vh, vx + vw, vy + vh, rgb(255, 255, 255));
    draw_line(vx, vy, vx, vy + vh, rgb(255, 255, 255));
    draw_line(vx + vw, vy, vx + vw, vy + vh, rgb(255, 255, 255));
}

void render_title(void) {
    clear_screen(rgb(30, 30, 20));
    put_text(40, 24, rgb(217, 166, 79), "OPERAZIONE PONTE SPEZZATO", 2);
    put_text(60, 60, rgb(200, 200, 180), "SCEGLI LA FAZIONE", 1);
    const char* names[5] = { "GRAN BRETAGNA", "STATI UNITI", "UNIONE SOVIETICA", "GERMANIA", "GIAPPONE" };
    for (int i = 0; i < 5; i++) {
        int y = 90 + i * 22;
        u32 c = (i == G.factionCursor) ? rgb(255, 230, 120) : rgb(200, 200, 180);
        if (i == G.factionCursor) fill_rect(40, y - 2, 400, 18, rgb(60, 56, 36));
        put_text(50, y, c, names[i], 1);
    }
    put_text(60, 220, rgb(200, 200, 180), "SINISTRA/DESTRA SELEZIONA  X CONFERMA", 1);
    put_text(60, 236, rgb(150, 150, 130), "PORT PSP - SINGLE PLAYER (NPC)", 1);
}

void render_paused(void) {
    draw_world();
    draw_hud();
    fill_rect(150, 120, 180, 40, rgb(20, 18, 12));
    put_text(196, 132, rgb(255, 230, 120), "PAUSA", 2);
}

void render_end_screen(int win) {
    draw_world();
    draw_hud();
    fill_rect(80, 90, 320, 100, rgb(20, 18, 12));
    draw_line(80, 90, 400, 90, rgb(217, 166, 79));
    draw_line(80, 190, 400, 190, rgb(217, 166, 79));
    put_text(150, 110, win ? rgb(120, 220, 120) : rgb(220, 90, 90), win ? "VITTORIA" : "SCONFITTA", 3);
    put_text(110, 150, rgb(230, 230, 210), win ? "OBBIETTIVI DISTRUTTI" : "PLOTONE ANNIENTATO", 1);
    put_text(120, 168, rgb(200, 200, 180), "X RIGIOCA  START MENU", 1);
}

void render_play(void) {
    draw_world();
    draw_hud();
}

#include "game.h"

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

#define wsx(wx) ((int)(((wx) - G.camX) * G.zoom))
#define wsy(wy) ((int)(((wy) - G.camY) * G.zoom))

static unsigned int __attribute__((aligned(16))) gu_list[262144];
static u32 __attribute__((aligned(16))) fb[2][BUF_WIDTH * SCR_HEIGHT];
static u32* g_fb = fb[0];
static int g_draw = 0;

Tex tex_tank[6];
Tex tex_soldier[7];
Tex tex_bridge[2];
Tex tex_bunker[2];
Tex tex_arty[2];
Tex tex_hq[2];
Tex tex_house;
Tex tex_tree[2];

static unsigned int rgb(int r, int g, int b) {
    return 0xff000000 | ((unsigned int)(b & 255) << 16) | ((unsigned int)(g & 255) << 8) | (unsigned int)(r & 255);
}

static void pxset(int x, int y, unsigned int c) {
    if (x < 0 || y < 0 || x >= SCR_WIDTH || y >= SCR_HEIGHT) return;
    g_fb[y * BUF_WIDTH + x] = c;
}

static void fill_rect(int x, int y, int w, int h, unsigned int c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_WIDTH) w = SCR_WIDTH - x;
    if (y + h > SCR_HEIGHT) h = SCR_HEIGHT - y;
    if (w <= 0 || h <= 0) return;
    for (int j = y; j < y + h; j++) {
        u32* row = g_fb + j * BUF_WIDTH;
        for (int i = x; i < x + w; i++) row[i] = c;
    }
}

static void line(int x0, int y0, int x1, int y1, unsigned int c) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        pxset(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void draw_circle(int cx, int cy, int r, unsigned int c) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        line(cx - x, cy + y, cx + x, cy + y, c);
        line(cx - y, cy + x, cx + y, cy + x, c);
        line(cx - x, cy - y, cx + x, cy - y, c);
        line(cx - y, cy - x, cx + y, cy - x, c);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

static void draw_disc(int cx, int cy, int r, unsigned int c) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) pxset(cx + x, cy + y, c);
}

static const unsigned char FONT[128][7] = {
    ['A'] = {0x7C,0x44,0x44,0x7C,0x44,0x44,0x44},
    ['B'] = {0x78,0x44,0x78,0x44,0x44,0x44,0x78},
    ['C'] = {0x3C,0x40,0x40,0x40,0x40,0x40,0x3C},
    ['D'] = {0x78,0x44,0x44,0x44,0x44,0x44,0x78},
    ['E'] = {0x7C,0x40,0x78,0x40,0x40,0x40,0x7C},
    ['F'] = {0x7C,0x40,0x78,0x40,0x40,0x40,0x40},
    ['G'] = {0x3C,0x40,0x40,0x4C,0x44,0x44,0x3C},
    ['H'] = {0x44,0x44,0x7C,0x44,0x44,0x44,0x44},
    ['I'] = {0x7C,0x10,0x10,0x10,0x10,0x10,0x7C},
    ['L'] = {0x40,0x40,0x40,0x40,0x40,0x40,0x7C},
    ['M'] = {0x44,0x6C,0x54,0x54,0x44,0x44,0x44},
    ['N'] = {0x44,0x64,0x54,0x4C,0x44,0x44,0x44},
    ['O'] = {0x38,0x44,0x44,0x44,0x44,0x44,0x38},
    ['P'] = {0x78,0x44,0x44,0x78,0x40,0x40,0x40},
    ['Q'] = {0x38,0x44,0x44,0x44,0x54,0x48,0x38},
    ['R'] = {0x78,0x44,0x44,0x78,0x48,0x44,0x44},
    ['S'] = {0x3C,0x40,0x40,0x38,0x04,0x04,0x78},
    ['T'] = {0x7C,0x10,0x10,0x10,0x10,0x10,0x10},
    ['U'] = {0x44,0x44,0x44,0x44,0x44,0x44,0x38},
    ['V'] = {0x44,0x44,0x44,0x44,0x44,0x28,0x10},
    ['Z'] = {0x7C,0x04,0x08,0x10,0x20,0x40,0x7C},
    ['0'] = {0x38,0x44,0x4C,0x54,0x64,0x44,0x38},
    ['1'] = {0x10,0x30,0x10,0x10,0x10,0x10,0x38},
    ['2'] = {0x78,0x04,0x04,0x38,0x40,0x40,0x7C},
    ['3'] = {0x78,0x04,0x04,0x38,0x04,0x04,0x78},
    ['4'] = {0x44,0x44,0x44,0x7C,0x04,0x04,0x04},
    ['5'] = {0x7C,0x40,0x40,0x78,0x04,0x04,0x78},
    ['6'] = {0x3C,0x40,0x40,0x78,0x44,0x44,0x38},
    ['7'] = {0x7C,0x04,0x08,0x10,0x10,0x10,0x10},
    ['8'] = {0x38,0x44,0x44,0x38,0x44,0x44,0x38},
    ['9'] = {0x38,0x44,0x44,0x3C,0x04,0x04,0x38},
    [' '] = {0,0,0,0,0,0,0},
    ['-'] = {0,0,0,0x7C,0,0,0},
    ['.'] = {0,0,0,0,0,0,0x18},
    ['/'] = {0x04,0x04,0x08,0x10,0x20,0x40,0x40},
    ['%'] = {0x44,0x64,0x18,0x30,0x4C,0x44,0,},
    [':'] = {0,0,0x18,0,0,0x18,0},
    ['!'] = {0x10,0x10,0x10,0x10,0x10,0,0x10},
    ['?'] = {0x38,0x44,0x04,0x18,0x10,0,0x10},
};

static void put_glyph(int x, int y, unsigned int c, char ch, int scale) {
    const unsigned char* g = FONT[(unsigned char)ch];
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

static void put_text(int x, int y, unsigned int c, const char* s, int scale) {
    int cx = x;
    for (const char* p = s; *p; p++) {
        char ch = *p;
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        put_glyph(cx, y, c, ch, scale);
        cx += 6 * scale;
    }
}

static void put_text_centered(int cx, int y, unsigned int c, const char* s, int scale) {
    int w = (int)strlen(s) * 6 * scale;
    put_text(cx - w / 2, y, c, s, scale);
}

static float sscale(float worldSize, const Tex* t) {
    if (!t || t->w <= 0 || t->h <= 0) return 1.0f;
    int m = t->w > t->h ? t->w : t->h;
    return (worldSize * G.zoom) / (float)m;
}

static int onscreen(int sx, int sy, int m) {
    return !(sx < -m || sy < -m || sx > SCR_WIDTH + m || sy > SCR_HEIGHT + m);
}

static int load_rgba(const char* path, Tex* out) {
    out->data = 0; out->w = 0; out->h = 0;
    FILE* fp = fopen(path, "rb");
    if (!fp) return 0;
    unsigned int w = 0, h = 0;
    if (fread(&w, 4, 1, fp) != 1 || fread(&h, 4, 1, fp) != 1) { fclose(fp); return 0; }
    unsigned char* data = (unsigned char*)malloc((size_t)w * h * 4);
    if (!data) { fclose(fp); return 0; }
    if (fread(data, 1, (size_t)w * h * 4, fp) != (size_t)w * h * 4) { free(data); fclose(fp); return 0; }
    fclose(fp);
    out->w = (int)w; out->h = (int)h; out->data = data;
    return 1;
}

void load_assets(void) {
    load_rgba("assets/tank_gb.rgba", &tex_tank[0]);
    load_rgba("assets/tank_us.rgba", &tex_tank[1]);
    load_rgba("assets/tank_su.rgba", &tex_tank[2]);
    load_rgba("assets/tank_de.rgba", &tex_tank[3]);
    load_rgba("assets/tank_jp.rgba", &tex_tank[4]);
    load_rgba("assets/tank_npc.rgba", &tex_tank[5]);
    load_rgba("assets/soldier_gb_officer.rgba", &tex_soldier[0]);
    load_rgba("assets/soldier_gb_sniper.rgba", &tex_soldier[1]);
    load_rgba("assets/soldier_gb_medic.rgba", &tex_soldier[2]);
    load_rgba("assets/soldier_gb_support.rgba", &tex_soldier[3]);
    load_rgba("assets/soldier_gb_soldier.rgba", &tex_soldier[4]);
    load_rgba("assets/soldier_gb_antitank.rgba", &tex_soldier[5]);
    load_rgba("assets/soldier_gb_radioman.rgba", &tex_soldier[6]);
    load_rgba("assets/bridge.rgba", &tex_bridge[0]);
    load_rgba("assets/bridge_dead.rgba", &tex_bridge[1]);
    load_rgba("assets/bunker.rgba", &tex_bunker[0]);
    load_rgba("assets/bunker_dead.rgba", &tex_bunker[1]);
    load_rgba("assets/artillery.rgba", &tex_arty[0]);
    load_rgba("assets/artillery_dead.rgba", &tex_arty[1]);
    load_rgba("assets/hq.rgba", &tex_hq[0]);
    load_rgba("assets/hq_dead.rgba", &tex_hq[1]);
    load_rgba("assets/house_a.rgba", &tex_house);
    load_rgba("assets/tree_a.rgba", &tex_tree[0]);
    load_rgba("assets/tree_b.rgba", &tex_tree[1]);
}

void blit(const Tex* t, float cx, float cy, float scale, float ang, int alpha) {
    if (!t || !t->data) return;
    int w = t->w, h = t->h;
    float hw = w * scale * 0.5f, hh = h * scale * 0.5f;
    int x0 = (int)(cx - hw), y0 = (int)(cy - hh), x1 = (int)(cx + hw), y1 = (int)(cy + hh);
    if (x1 < 0 || y1 < 0 || x0 >= SCR_WIDTH || y0 >= SCR_HEIGHT) return;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 >= SCR_WIDTH) x1 = SCR_WIDTH - 1;
    if (y1 >= SCR_HEIGHT) y1 = SCR_HEIGHT - 1;
    float ca = (float)cosf(-ang), sa = (float)sinf(-ang);
    float iw = 1.0f / w, ih = 1.0f / h;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float lx = x - cx, ly = y - cy;
            float sx = (lx * ca - ly * sa) * iw + 0.5f;
            float sy = (lx * sa + ly * ca) * ih + 0.5f;
            int ix = (int)sx, iy = (int)sy;
            if (ix < 0 || iy < 0 || ix >= w || iy >= h) continue;
            const unsigned char* p = t->data + (iy * w + ix) * 4;
            unsigned char a = p[3];
            if (a == 0) continue;
            int aa = (alpha < 255) ? (a * alpha) >> 8 : a;
            if (aa <= 0) continue;
            unsigned int dst = g_fb[y * BUF_WIDTH + x];
            if (aa >= 250) {
                g_fb[y * BUF_WIDTH + x] = 0xff000000u | ((unsigned int)p[2] << 16) | ((unsigned int)p[1] << 8) | p[0];
            } else {
                unsigned int dr = dst & 0xff, dg = (dst >> 8) & 0xff, db = (dst >> 16) & 0xff;
                unsigned int rr = (p[0] * aa + dr * (255 - aa)) / 255;
                unsigned int gg = (p[1] * aa + dg * (255 - aa)) / 255;
                unsigned int bb = (p[2] * aa + db * (255 - aa)) / 255;
                g_fb[y * BUF_WIDTH + x] = 0xff000000u | (bb << 16) | (gg << 8) | rr;
            }
        }
    }
}

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
    g_fb = fb[g_draw];
    sceGuStart(GU_DIRECT, gu_list);
}

void render_end(void) {
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    g_draw = 1 - g_draw;
}

static void draw_world(void) {
    unsigned int water = rgb(46, 84, 104);
    fill_rect(0, 0, SCR_WIDTH, SCR_HEIGHT, water);

    for (int i = 0; i < 90; i++) {
        float wx = world.grass[i * 3], wy = world.grass[i * 3 + 1];
        int sx = wsx(wx), sy = wsy(wy);
        if (sx < 0 || sy < 0 || sx >= SCR_WIDTH || sy >= SCR_HEIGHT) continue;
        unsigned int c = (world.grass[i * 3 + 2] > 0.5f) ? rgb(60, 78, 42) : rgb(72, 88, 50);
        pxset(sx, sy, c);
    }

    int rx1 = wsx(world.riverX1), rx2 = wsx(world.riverX2);
    if (rx1 < 0) rx1 = 0;
    if (rx2 > SCR_WIDTH) rx2 = SCR_WIDTH;
    if (rx2 > rx1) fill_rect(rx1, 0, rx2 - rx1, SCR_HEIGHT, water);

    for (int i = 0; i < world.nBridge; i++) {
        Bridge* b = &world.bridges[i];
        float cx = (b->x1 + b->x2) / 2, cy = (b->y1 + b->y2) / 2;
        int sx = wsx(cx), sy = wsy(cy);
        if (!onscreen(sx, sy, 80)) continue;
        float sc = (b->x2 - b->x1) * G.zoom / (float)(tex_bridge[0].w > 0 ? tex_bridge[0].w : 1);
        blit(&tex_bridge[0], (float)sx, (float)sy, sc, 0, 255);
    }

    for (int i = 0; i < world.nHouse; i++) {
        House* h = &world.houses[i];
        float cx = h->x + h->w / 2, cy = h->y + h->h / 2;
        int sx = wsx(cx), sy = wsy(cy);
        if (!onscreen(sx, sy, 60)) continue;
        blit(&tex_house, (float)sx, (float)sy, sscale((h->w > h->h ? h->w : h->h), &tex_house), 0, 255);
    }

    for (int i = 0; i < world.nForest; i++) {
        Forest* f = &world.forests[i];
        for (int j = 0; j < f->ntree; j++) {
            float tx = f->x + f->trees[j * 2], ty = f->y + f->trees[j * 2 + 1];
            int sx = wsx(tx), sy = wsy(ty);
            if (!onscreen(sx, sy, 30)) continue;
            blit(&tex_tree[j & 1], (float)sx, (float)sy, sscale(26, &tex_tree[j & 1]), 0, 255);
        }
    }

    for (int i = 0; i < world.nBunker; i++) {
        Bunker* b = &world.bunkers[i];
        int sx = wsx(b->x), sy = wsy(b->y);
        if (onscreen(sx, sy, 50)) blit(&tex_bunker[b->destroyed ? 1 : 0], (float)sx, (float)sy, sscale(2 * b->r, &tex_bunker[0]), 0, 255);
    }
    for (int i = 0; i < world.nArty; i++) {
        Arty* a = &world.artillery[i];
        int sx = wsx(a->x), sy = wsy(a->y);
        if (onscreen(sx, sy, 50)) blit(&tex_arty[a->destroyed ? 1 : 0], (float)sx, (float)sy, sscale(2 * a->r, &tex_arty[0]), 0, 255);
    }
    for (int i = 0; i < world.nHq; i++) {
        Hq* h = &world.hqs[i];
        int sx = wsx(h->x), sy = wsy(h->y);
        if (onscreen(sx, sy, 70)) blit(&tex_hq[h->destroyed ? 1 : 0], (float)sx, (float)sy, sscale(2 * h->r, &tex_hq[0]), 0, 255);
    }

    for (int i = 0; i < world.nMine; i++) {
        Mine* m = &world.mines[i];
        int sx = wsx(m->x), sy = wsy(m->y);
        if (onscreen(sx, sy, 60)) draw_circle(sx, sy, (int)(m->r * G.zoom), rgb(140, 90, 30));
    }

    for (int i = 0; i < unitCount; i++) {
        Unit* u = &units[i];
        if (u->dead) continue;
        int sx = wsx(u->x), sy = wsy(u->y);
        if (!onscreen(sx, sy, 30)) continue;
        if (u->role == ROLE_TANK) {
            int ti = (u->faction < 0) ? 5 : u->faction;
            blit(&tex_tank[ti], (float)sx, (float)sy, sscale(34, &tex_tank[ti]), u->angle, 255);
        } else {
            blit(&tex_soldier[u->role], (float)sx, (float)sy, sscale(18, &tex_soldier[u->role]), u->angle, 255);
        }
        if (u->hp < u->maxHp) {
            int bw = 18, bx = sx - bw / 2, by = sy - 14;
            fill_rect(bx - 1, by - 1, bw + 2, 4, rgb(0, 0, 0));
            fill_rect(bx, by, (int)(bw * (u->hp / u->maxHp)), 2, rgb(90, 220, 90));
        }
    }

    for (int i = 0; i < missionCount; i++) {
        Mission* m = &missions[i];
        int sx = wsx(m->cx), sy = wsy(m->cy);
        if (!onscreen(sx, sy, 70)) continue;
        unsigned int c = m->destroyed ? rgb(120, 120, 120) : rgb(235, 210, 90);
        draw_circle(sx, sy, (int)(m->r * G.zoom), c);
        if (!m->destroyed) {
            int bw = 44, bx = sx - bw / 2, by = sy - (int)(m->r * G.zoom) - 9;
            fill_rect(bx - 1, by - 1, bw + 2, 5, rgb(0, 0, 0));
            fill_rect(bx, by, (int)(bw * (m->hp / m->maxHp)), 3, rgb(235, 80, 60));
            put_text_centered(sx, by - 13, c, m->label, 1);
        }
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &particles[i];
        if (p->life <= 0) continue;
        int sx = wsx(p->x), sy = wsy(p->y);
        if (sx < 0 || sy < 0 || sx >= SCR_WIDTH || sy >= SCR_HEIGHT) continue;
        int sz = (int)(p->size * G.zoom);
        if (sz < 1) sz = 1;
        unsigned int col = rgb((int)(p->r * 255), (int)(p->g * 255), (int)(p->b * 255));
        draw_disc(sx, sy, sz, col);
    }

    int cx = wsx(G.curX), cy = wsy(G.curY);
    if (cx >= 0 && cy >= 0 && cx < SCR_WIDTH && cy < SCR_HEIGHT) {
        unsigned int rc = rgb(255, 60, 40);
        line(cx - 9, cy, cx - 3, cy, rc); line(cx + 3, cy, cx + 9, cy, rc);
        line(cx, cy - 9, cx, cy - 3, rc); line(cx, cy + 3, cx, cy + 9, rc);
    }
}

static void draw_hud(void) {
    put_text(4, 4, rgb(235, 230, 200), "OPERAZIONE PONTE SPEZZATO", 1);
    char buf[80];
    sprintf(buf, "FASE: %s", faction_name(G.playerFaction));
    put_text(4, 16, rgb(210, 210, 180), buf, 1);
    sprintf(buf, "OBBIETTIVI %d/%d   RINF. %d", G.missionDone, G.missionTotal, G.respawns);
    put_text(4, 28, rgb(210, 210, 180), buf, 1);
    if (G.respawnCd > 0) { sprintf(buf, "RINFORZI IN %d", (int)G.respawnCd + 1); put_text(4, 40, rgb(255, 180, 120), buf, 1); }

    for (int i = 0; i < 6; i++) {
        const char* l = log_line(i);
        if (l && l[0]) put_text(SCR_WIDTH - 150, 4 + i * 12, rgb(200, 200, 200), l, 1);
    }
    put_text(4, SCR_HEIGHT - 14, rgb(200, 200, 180), "X MUOVI O ATTACCA TR DIFENDI SQ RINF. L/R ZOOM START PAUSA", 1);
}

void render_title(void) {
    fill_rect(0, 0, SCR_WIDTH, SCR_HEIGHT, rgb(28, 32, 24));
    put_text_centered(SCR_WIDTH / 2, 26, rgb(235, 215, 140), "OPERAZIONE", 3);
    put_text_centered(SCR_WIDTH / 2, 58, rgb(235, 215, 140), "PONTE SPEZZATO", 3);
    const char* names[5] = { "GRAN BRETAGNA", "STATI UNITI", "UNIONE SOVIETICA", "GERMANIA", "GIAPPONE" };
    for (int i = 0; i < 5; i++) {
        unsigned int c = (i == G.factionCursor) ? rgb(255, 235, 120) : rgb(200, 200, 180);
        int y = 120 + i * 22;
        put_text_centered(SCR_WIDTH / 2, y, c, names[i], 1);
        if (i == G.factionCursor) put_text_centered(SCR_WIDTH / 2, y - 12, rgb(255, 235, 120), "<", 1);
    }
    put_text_centered(SCR_WIDTH / 2, SCR_HEIGHT - 18, rgb(180, 200, 180), "D-PAD SCEGLI  X CONFERMA", 1);
}

void render_play(void) {
    draw_world();
    draw_hud();
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
    unsigned int c = win ? rgb(120, 230, 120) : rgb(235, 90, 70);
    put_text_centered(SCR_WIDTH / 2, 110, c, win ? "VITTORIA" : "SCONFITTA", 3);
    put_text_centered(SCR_WIDTH / 2, 150, rgb(220, 220, 200), "X ALTRO SCONTRO", 1);
    put_text_centered(SCR_WIDTH / 2, 166, rgb(220, 220, 200), "START MENU", 1);
}

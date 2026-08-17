#include "game.h"
#include "assets_embedded.h"

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

#define wsx(wx) ((int)(((wx) - G.camX) * G.zoom))
#define wsy(wy) ((int)(((wy) - G.camY) * G.zoom))

static unsigned int __attribute__((aligned(16))) gu_list[16384];
static u32* vram_fb[2];
static u32* g_fb = 0;
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
    ['A'] = {0x3E,0x22,0x22,0x3E,0x22,0x22,0x22},
    ['B'] = {0x1E,0x22,0x1E,0x22,0x22,0x22,0x1E},
    ['C'] = {0x3C,0x02,0x02,0x02,0x02,0x02,0x3C},
    ['D'] = {0x1E,0x22,0x22,0x22,0x22,0x22,0x1E},
    ['E'] = {0x3E,0x02,0x1E,0x02,0x02,0x02,0x3E},
    ['F'] = {0x3E,0x02,0x1E,0x02,0x02,0x02,0x02},
    ['G'] = {0x3C,0x02,0x02,0x32,0x22,0x22,0x3C},
    ['H'] = {0x22,0x22,0x3E,0x22,0x22,0x22,0x22},
    ['I'] = {0x3E,0x08,0x08,0x08,0x08,0x08,0x3E},
    ['J'] = {0x38,0x10,0x10,0x10,0x12,0x12,0x0C},
    ['K'] = {0x22,0x12,0x0A,0x06,0x0A,0x12,0x22},
    ['L'] = {0x02,0x02,0x02,0x02,0x02,0x02,0x3E},
    ['M'] = {0x22,0x36,0x2A,0x2A,0x22,0x22,0x22},
    ['N'] = {0x22,0x26,0x2A,0x32,0x22,0x22,0x22},
    ['O'] = {0x1C,0x22,0x22,0x22,0x22,0x22,0x1C},
    ['P'] = {0x1E,0x22,0x22,0x1E,0x02,0x02,0x02},
    ['Q'] = {0x1C,0x22,0x22,0x22,0x2A,0x12,0x1C},
    ['R'] = {0x1E,0x22,0x22,0x1E,0x12,0x22,0x22},
    ['S'] = {0x3C,0x02,0x02,0x1C,0x20,0x20,0x1E},
    ['T'] = {0x3E,0x08,0x08,0x08,0x08,0x08,0x08},
    ['U'] = {0x22,0x22,0x22,0x22,0x22,0x22,0x1C},
    ['V'] = {0x22,0x22,0x22,0x22,0x22,0x14,0x08},
    ['W'] = {0x22,0x22,0x22,0x2A,0x2A,0x36,0x22},
    ['X'] = {0x22,0x22,0x14,0x08,0x14,0x22,0x22},
    ['Y'] = {0x22,0x22,0x14,0x08,0x08,0x08,0x08},
    ['Z'] = {0x3E,0x20,0x10,0x08,0x04,0x02,0x3E},
    ['0'] = {0x1C,0x22,0x32,0x2A,0x26,0x22,0x1C},
    ['1'] = {0x08,0x0C,0x08,0x08,0x08,0x08,0x1C},
    ['2'] = {0x1E,0x20,0x20,0x1C,0x02,0x02,0x3E},
    ['3'] = {0x1E,0x20,0x20,0x1C,0x20,0x20,0x1E},
    ['4'] = {0x22,0x22,0x22,0x3E,0x20,0x20,0x20},
    ['5'] = {0x3E,0x02,0x02,0x1E,0x20,0x20,0x1E},
    ['6'] = {0x3C,0x02,0x02,0x1E,0x22,0x22,0x1C},
    ['7'] = {0x3E,0x20,0x10,0x08,0x08,0x08,0x08},
    ['8'] = {0x1C,0x22,0x22,0x1C,0x22,0x22,0x1C},
    ['9'] = {0x1C,0x22,0x22,0x3C,0x20,0x20,0x1C},
    [' '] = {0,0,0,0,0,0,0},
    ['-'] = {0,0,0,0x3E,0,0,0},
    ['+'] = {0,0x08,0x08,0x3E,0x08,0x08,0},
    ['.'] = {0,0,0,0,0,0,0x18},
    [','] = {0,0,0,0,0x18,0x18,0x0C},
    ['/'] = {0x20,0x20,0x10,0x08,0x04,0x02,0x02},
    ['%'] = {0x22,0x26,0x18,0x0C,0x32,0x22,0},
    [':'] = {0,0x18,0x18,0,0x18,0x18,0},
    ['!'] = {0x08,0x08,0x08,0x08,0x08,0,0x08},
    ['?'] = {0x1C,0x22,0x20,0x18,0x08,0,0x08},
    ['<'] = {0x10,0x08,0x04,0x02,0x04,0x08,0x10},
    ['>'] = {0x04,0x08,0x10,0x20,0x10,0x08,0x04},
    ['('] = {0x10,0x08,0x04,0x04,0x04,0x08,0x10},
    [')'] = {0x04,0x08,0x10,0x10,0x10,0x08,0x04},
};

static void put_glyph(int x, int y, unsigned int c, char ch, int scale) {
    unsigned char uc = (unsigned char)ch;
    if (uc >= 128) return;
    const unsigned char* g = FONT[uc];
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

static const unsigned char* find_embedded(const char* name, unsigned int* size) {
    for (int i = 0; i < g_embedded_asset_count; i++) {
        if (strcmp(g_embedded_assets[i].name, name) == 0) {
            *size = g_embedded_assets[i].size;
            return g_embedded_assets[i].data;
        }
    }
    return 0;
}

static int load_rgba(const char* path, Tex* out) {
    out->data = 0; out->w = 0; out->h = 0;
    char tmp[96];
    unsigned int esize = 0;
    const unsigned char* em = find_embedded(path, &esize);
    if (em && esize >= 8) {
        unsigned int w = (unsigned int)em[0] | ((unsigned int)em[1] << 8) | ((unsigned int)em[2] << 16) | ((unsigned int)em[3] << 24);
        unsigned int h = (unsigned int)em[4] | ((unsigned int)em[5] << 8) | ((unsigned int)em[6] << 16) | ((unsigned int)em[7] << 24);
        if ((size_t)w * h * 4 <= esize - 8) {
            out->w = (int)w; out->h = (int)h;
            out->data = (unsigned char*)em + 8;
            return 1;
        }
    }
    FILE* fp = fopen(path, "rb");
    if (!fp) { sprintf(tmp, "MISSING %s", path); log_push(tmp); return 0; }
    unsigned int w = 0, h = 0;
    if (fread(&w, 4, 1, fp) != 1 || fread(&h, 4, 1, fp) != 1) { sprintf(tmp, "BAD %s", path); log_push(tmp); fclose(fp); return 0; }
    unsigned char* data = (unsigned char*)malloc((size_t)w * h * 4);
    if (!data) { sprintf(tmp, "OOM %s", path); log_push(tmp); fclose(fp); return 0; }
    if (fread(data, 1, (size_t)w * h * 4, fp) != (size_t)w * h * 4) { free(data); sprintf(tmp, "TRUNC %s", path); log_push(tmp); fclose(fp); return 0; }
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
    if (scale <= 0.001f) return;
    float inv_scale = 1.0f / scale;
    float ca = (float)cosf(-ang), sa = (float)sinf(-ang);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float lx = (x - cx) * inv_scale;
            float ly = (y - cy) * inv_scale;
            float sx = (lx * ca - ly * sa) + w * 0.5f;
            float sy = (lx * sa + ly * ca) + h * 0.5f;
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
    u32* vram = (u32*)0x04000000;
    vram_fb[0] = vram;
    vram_fb[1] = vram + (BUF_WIDTH * SCR_HEIGHT);
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)(BUF_WIDTH * SCR_HEIGHT * 4), BUF_WIDTH);
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
    g_fb = vram_fb[g_draw];
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
    unsigned int grassCol = rgb(84, 112, 58);
    unsigned int waterCol = rgb(53, 107, 130);
    unsigned int bankCol = rgb(111, 90, 54);
    fill_rect(0, 0, SCR_WIDTH, SCR_HEIGHT, grassCol);

    for (int i = 0; i < 90; i++) {
        float wx = world.grass[i * 3], wy = world.grass[i * 3 + 1];
        int sx = wsx(wx), sy = wsy(wy);
        if (sx < 0 || sy < 0 || sx >= SCR_WIDTH || sy >= SCR_HEIGHT) continue;
        unsigned int c = (world.grass[i * 3 + 2] > 0.5f) ? rgb(55, 78, 38) : rgb(110, 135, 70);
        pxset(sx, sy, c);
        pxset(sx + 1, sy, c);
        pxset(sx, sy + 1, c);
    }

    int rx1 = wsx(world.riverX1), rx2 = wsx(world.riverX2);
    int bankW = (int)(14 * G.zoom);
    if (bankW < 2) bankW = 2;

    // Draw left bank
    int b1_x1 = rx1 - bankW, b1_x2 = rx1;
    if (b1_x1 < 0) b1_x1 = 0;
    if (b1_x2 > SCR_WIDTH) b1_x2 = SCR_WIDTH;
    if (b1_x2 > b1_x1) fill_rect(b1_x1, 0, b1_x2 - b1_x1, SCR_HEIGHT, bankCol);

    // Draw right bank
    int b2_x1 = rx2, b2_x2 = rx2 + bankW;
    if (b2_x1 < 0) b2_x1 = 0;
    if (b2_x2 > SCR_WIDTH) b2_x2 = SCR_WIDTH;
    if (b2_x2 > b2_x1) fill_rect(b2_x1, 0, b2_x2 - b2_x1, SCR_HEIGHT, bankCol);

    // Draw river water
    int wr1 = rx1, wr2 = rx2;
    if (wr1 < 0) wr1 = 0;
    if (wr2 > SCR_WIDTH) wr2 = SCR_WIDTH;
    if (wr2 > wr1) fill_rect(wr1, 0, wr2 - wr1, SCR_HEIGHT, waterCol);

    // Bridges
    for (int i = 0; i < world.nBridge; i++) {
        Bridge* b = &world.bridges[i];
        int bx1 = wsx(b->x1), bx2 = wsx(b->x2);
        int by1 = wsy(b->y1), by2 = wsy(b->y2);
        int bw = bx2 - bx1, bh = by2 - by1;
        if (bw > 0 && bh > 0 && onscreen((bx1 + bx2) / 2, (by1 + by2) / 2, 80)) {
            if (tex_bridge[b->destroyed ? 1 : 0].data) {
                float sc = (float)bw / (tex_bridge[0].w > 0 ? tex_bridge[0].w : 1);
                blit(&tex_bridge[b->destroyed ? 1 : 0], (float)(bx1 + bx2) / 2.0f, (float)(by1 + by2) / 2.0f, sc, 0, 255);
            } else {
                if (b->destroyed) {
                    fill_rect(bx1, by1 + bh / 3, bw / 3, bh / 3, rgb(74, 58, 34));
                    fill_rect(bx2 - bw / 3, by1 + bh / 3, bw / 3, bh / 3, rgb(74, 58, 34));
                } else {
                    fill_rect(bx1, by1, bw, bh, rgb(181, 144, 92));
                    fill_rect(bx1, by1, bw, 3, rgb(92, 69, 39));
                    fill_rect(bx1, by2 - 3, bw, 3, rgb(92, 69, 39));
                    for (int py = by1 + 4; py < by2 - 3; py += 6) {
                        line(bx1, py, bx2, py, rgb(96, 72, 42));
                    }
                }
            }
        }
    }

    // Houses
    for (int i = 0; i < world.nHouse; i++) {
        House* h = &world.houses[i];
        int hx = wsx(h->x), hy = wsy(h->y);
        int hw = (int)(h->w * G.zoom), hh = (int)(h->h * G.zoom);
        if (onscreen(hx + hw / 2, hy + hh / 2, 60)) {
            if (tex_house.data) {
                blit(&tex_house, (float)(hx + hw / 2), (float)(hy + hh / 2), sscale(h->w > h->h ? h->w : h->h, &tex_house), 0, 255);
            } else {
                // Procedural house
                fill_rect(hx + 2, hy + 2, hw, hh, rgb(20, 18, 12)); // shadow
                fill_rect(hx, hy, hw, hh, rgb(154, 140, 102)); // wall
                fill_rect(hx, hy, hw, hh / 3, rgb(90, 74, 54)); // roof
                fill_rect(hx + hw / 3, hy + hh / 2, hw / 3, hh / 2, rgb(46, 42, 32)); // door
            }
        }
    }

    // Forests & Trees
    for (int i = 0; i < world.nForest; i++) {
        Forest* f = &world.forests[i];
        int fx = wsx(f->x), fy = wsy(f->y), fr = (int)(f->r * G.zoom);
        if (onscreen(fx, fy, fr + 20)) {
            draw_circle(fx, fy, fr, rgb(35, 55, 25));
            for (int j = 0; j < f->ntree; j++) {
                float tx = f->x + f->trees[j * 2], ty = f->y + f->trees[j * 2 + 1];
                int sx = wsx(tx), sy = wsy(ty);
                if (!onscreen(sx, sy, 30)) continue;
                if (tex_tree[j & 1].data) {
                    blit(&tex_tree[j & 1], (float)sx, (float)sy, sscale(26, &tex_tree[j & 1]), 0, 255);
                } else {
                    draw_disc(sx, sy, (int)(7 * G.zoom), (j & 1) ? rgb(44, 74, 34) : rgb(60, 97, 48));
                }
            }
        }
    }

    // Bunkers
    for (int i = 0; i < world.nBunker; i++) {
        Bunker* b = &world.bunkers[i];
        int sx = wsx(b->x), sy = wsy(b->y), br = (int)(b->r * G.zoom);
        if (!onscreen(sx, sy, br + 20)) continue;
        if (tex_bunker[b->destroyed ? 1 : 0].data) {
            blit(&tex_bunker[b->destroyed ? 1 : 0], (float)sx, (float)sy, sscale(2 * b->r, &tex_bunker[0]), 0, 255);
        } else {
            // Procedural bunker (circular sandbags + concrete dome + firing slit)
            draw_disc(sx + 3, sy + 3, br, rgb(20, 18, 12));
            draw_disc(sx, sy, br, b->destroyed ? rgb(70, 66, 58) : rgb(138, 134, 118));
            draw_circle(sx, sy, br, rgb(35, 33, 27));
            if (!b->destroyed) {
                int slitW = (int)(18 * G.zoom), slitH = (int)(6 * G.zoom);
                fill_rect(sx - slitW / 2, sy - slitH / 2, slitW, slitH, rgb(21, 19, 14));
                line(sx, sy, sx + (b->x < WORLD_W / 2 ? br : -br), sy, rgb(44, 44, 38));
            } else {
                line(sx - br / 2, sy - br / 2, sx + br / 2, sy + br / 2, rgb(21, 19, 14));
                line(sx + br / 2, sy - br / 2, sx - br / 2, sy + br / 2, rgb(21, 19, 14));
            }
        }
    }

    // Artillery
    for (int i = 0; i < world.nArty; i++) {
        Arty* a = &world.artillery[i];
        int sx = wsx(a->x), sy = wsy(a->y), ar = (int)(a->r * G.zoom);
        if (!onscreen(sx, sy, ar + 20)) continue;
        if (tex_arty[a->destroyed ? 1 : 0].data) {
            blit(&tex_arty[a->destroyed ? 1 : 0], (float)sx, (float)sy, sscale(2 * a->r, &tex_arty[0]), 0, 255);
        } else {
            // Procedural artillery (circular pit + barrel)
            draw_disc(sx + 2, sy + 2, ar, rgb(20, 18, 12));
            draw_disc(sx, sy, ar, a->destroyed ? rgb(70, 66, 58) : rgb(125, 115, 99));
            draw_circle(sx, sy, ar, rgb(35, 33, 27));
            if (!a->destroyed) {
                int gunLen = (int)(ar * 1.3f);
                int dir = (a->x < WORLD_W / 2 ? 1 : -1);
                line(sx, sy, sx + gunLen * dir, sy - 2, rgb(28, 26, 21));
                line(sx, sy + 1, sx + gunLen * dir, sy - 1, rgb(58, 53, 44));
            } else {
                line(sx - ar / 2, sy - ar / 2, sx + ar / 2, sy + ar / 2, rgb(21, 19, 14));
            }
        }
    }

    // HQ (Quartier Generale)
    for (int i = 0; i < world.nHq; i++) {
        Hq* h = &world.hqs[i];
        int sx = wsx(h->x), sy = wsy(h->y), hr = (int)(h->r * G.zoom);
        if (!onscreen(sx, sy, hr + 20)) continue;
        if (tex_hq[h->destroyed ? 1 : 0].data) {
            blit(&tex_hq[h->destroyed ? 1 : 0], (float)sx, (float)sy, sscale(2 * h->r, &tex_hq[0]), 0, 255);
        } else {
            // Procedural HQ building
            int hw = (int)(hr * 1.6f), hh = (int)(hr * 1.1f);
            fill_rect(sx - hw / 2 + 3, sy - hh / 2 + 3, hw, hh, rgb(20, 18, 12));
            fill_rect(sx - hw / 2, sy - hh / 2, hw, hh, h->destroyed ? rgb(58, 54, 48) : rgb(138, 128, 104));
            fill_rect(sx - hw / 2, sy - hh / 2, hw, hh / 4, rgb(90, 74, 54)); // roof
            if (!h->destroyed) {
                fill_rect(sx - 4, sy + hh / 4, 8, hh / 4, rgb(30, 26, 16)); // door
                // Flag
                line(sx + hw / 3, sy - hh / 2, sx + hw / 3, sy - hh / 2 - 16, rgb(40, 36, 26));
                fill_rect(sx + hw / 3, sy - hh / 2 - 16, 10, 6, rgb(140, 47, 47));
            } else {
                line(sx - hw / 2, sy - hh / 2, sx + hw / 2, sy + hh / 2, rgb(21, 19, 14));
                line(sx + hw / 2, sy - hh / 2, sx - hw / 2, sy + hh / 2, rgb(21, 19, 14));
            }
        }
    }

    // Minefields
    for (int i = 0; i < world.nMine; i++) {
        Mine* m = &world.mines[i];
        int sx = wsx(m->x), sy = wsy(m->y), mr = (int)(m->r * G.zoom);
        if (onscreen(sx, sy, mr + 20)) {
            draw_circle(sx, sy, mr, rgb(192, 57, 43));
            for (int k = 0; k < m->n; k++) {
                int mkx = sx + (int)(m->markers[k * 2] * G.zoom);
                int mky = sy + (int)(m->markers[k * 2 + 1] * G.zoom);
                pxset(mkx, mky, rgb(42, 38, 32));
            }
        }
    }

    for (int i = 0; i < unitCount; i++) {
        Unit* u = &units[i];
        if (u->dead) continue;
        int sx = wsx(u->x), sy = wsy(u->y);
        if (!onscreen(sx, sy, 30)) continue;

        // Squad/faction selection halo disc
        if (u->faction == G.playerFaction) {
            draw_circle(sx, sy, (int)(11 * G.zoom), rgb(191, 247, 176));
        } else if (u->faction == FAC_NPC) {
            draw_circle(sx, sy, (int)(10 * G.zoom), rgb(224, 72, 59));
        } else {
            draw_circle(sx, sy, (int)(10 * G.zoom), rgb(217, 201, 138));
        }

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

    // Draw bullet tracers
    for (int i = 0; i < MAX_TRACERS; i++) {
        Tracer* t = &tracers[i];
        if (t->life <= 0) continue;
        int sx1 = wsx(t->x1), sy1 = wsy(t->y1);
        int sx2 = wsx(t->x2), sy2 = wsy(t->y2);
        line(sx1, sy1, sx2, sy2, t->color);
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

    // Mini radar map (bottom right, above controls)
    int mw = 70, mh = (int)(mw * (WORLD_H / WORLD_W));
    int mx0 = SCR_WIDTH - mw - 4, my0 = SCR_HEIGHT - mh - 18;
    fill_rect(mx0 - 1, my0 - 1, mw + 2, mh + 2, rgb(20, 18, 12));
    fill_rect(mx0, my0, mw, mh, rgb(43, 48, 36));
    float msx = (float)mw / WORLD_W, msy = (float)mh / WORLD_H;
    for (int i = 0; i < world.nBunker; i++) if (!world.bunkers[i].destroyed) pxset(mx0 + (int)(world.bunkers[i].x * msx), my0 + (int)(world.bunkers[i].y * msy), rgb(138, 128, 100));
    for (int i = 0; i < world.nArty; i++) if (!world.artillery[i].destroyed) pxset(mx0 + (int)(world.artillery[i].x * msx), my0 + (int)(world.artillery[i].y * msy), rgb(201, 162, 39));
    for (int i = 0; i < world.nHq; i++) if (!world.hqs[i].destroyed) pxset(mx0 + (int)(world.hqs[i].x * msx), my0 + (int)(world.hqs[i].y * msy), rgb(178, 60, 60));
    for (int i = 0; i < unitCount; i++) {
        if (units[i].dead) continue;
        unsigned int uc = (units[i].faction == G.playerFaction) ? rgb(191, 247, 176) : ((units[i].faction == FAC_NPC) ? rgb(224, 86, 63) : rgb(217, 201, 138));
        pxset(mx0 + (int)(units[i].x * msx), my0 + (int)(units[i].y * msy), uc);
    }
}

static void draw_hud(void) {
    // Header
    put_text(4, 4, rgb(235, 230, 200), "OPERAZIONE PONTE SPEZZATO", 1);
    char buf[80];
    sprintf(buf, "COMANDO: %s", faction_name(G.playerFaction));
    put_text(4, 15, rgb(210, 210, 180), buf, 1);
    sprintf(buf, "OBIETTIVI: %d/%d   RINFORZI: %d", G.missionDone, G.missionTotal, G.respawns);
    put_text(4, 26, rgb(210, 210, 180), buf, 1);

    // Abilities HUD
    const char* anames[5] = { "ARTIGL", "AEREO", "PARACAD", "RIFORN", "CARRO" };
    for (int a = 1; a <= 5; a++) {
        int ax = 4 + (a - 1) * 44, ay = 38;
        fill_rect(ax, ay, 40, 11, rgb(30, 26, 16));
        unsigned int col = (G.abilityCd[a] > 0) ? rgb(140, 130, 110) : rgb(217, 166, 79);
        char abuf[16];
        if (G.abilityCd[a] > 0) sprintf(abuf, "%d:%ds", a, (int)G.abilityCd[a]);
        else sprintf(abuf, "%d:%s", a, anames[a-1]);
        put_text(ax + 2, ay + 2, col, abuf, 1);
    }

    if (G.respawnCd > 0) { sprintf(buf, "RINFORZI IN %d", (int)G.respawnCd + 1); put_text(4, 52, rgb(255, 180, 120), buf, 1); }

    for (int i = 0; i < 6; i++) {
        const char* l = log_line(i);
        if (l && l[0]) put_text(SCR_WIDTH - 165, 4 + i * 11, rgb(220, 220, 200), l, 1);
    }
    put_text(4, SCR_HEIGHT - 12, rgb(200, 200, 180), "X MUOVI  O ATTACCA  TR DIFENDI  SQ RINF/ABIL  L/R ZOOM", 1);
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
    const char* e0 = log_line(0);
    const char* e1 = log_line(1);
    if (e0 && e0[0]) put_text(8, 92, rgb(235, 140, 120), e0, 1);
    if (e1 && e1[0]) put_text(8, 104, rgb(235, 140, 120), e1, 1);
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

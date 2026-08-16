#include "game.h"

PSP_MODULE_INFO("Operazione Ponte Spezzato", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

static int done = 0;

static int exit_callback(int arg1, int arg2, void* common) {
    done = 1;
    return 0;
}

static int CallbackThread(SceSize args, void* argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static int setupCallbacks(void) {
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
    return 0;
}

static unsigned int prevButtons = 0;

static int pressed(SceCtrlData* pad, int btn) {
    return (pad->Buttons & btn) && !(prevButtons & btn);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    setupCallbacks();
    gfx_init();
    audio_init();
    game_init();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    SceCtrlData pad;
    unsigned int last = sceKernelGetSystemTimeLow();

    while (!done) {
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned int now = sceKernelGetSystemTimeLow();
        float dt = (float)(now - last) / 1000000.0f;
        last = now;
        if (dt > 0.05f) dt = 0.05f;
        if (dt <= 0) dt = 0.016f;

        if (G.state == ST_TITLE) {
            if (pressed(&pad, PSP_CTRL_LEFT)) { G.factionCursor = (G.factionCursor + 4) % 5; sfx_ui(); }
            if (pressed(&pad, PSP_CTRL_RIGHT)) { G.factionCursor = (G.factionCursor + 1) % 5; sfx_ui(); }
            if (pressed(&pad, PSP_CTRL_CROSS)) { game_new_battle(G.factionCursor); sfx_ui(); }
        } else if (G.state == ST_PLAY) {
            float sp = 300.0f / G.zoom * dt;
            if (pad.Buttons & PSP_CTRL_LEFT) G.curX -= sp;
            if (pad.Buttons & PSP_CTRL_RIGHT) G.curX += sp;
            if (pad.Buttons & PSP_CTRL_UP) G.curY -= sp;
            if (pad.Buttons & PSP_CTRL_DOWN) G.curY += sp;
            float ax = (pad.Lx - 128) / 128.0f;
            float ay = (pad.Ly - 128) / 128.0f;
            G.curX += ax * sp * 1.6f;
            G.curY += ay * sp * 1.6f;
            G.curX = (float)fmaxf(0, fminf(WORLD_W, G.curX));
            G.curY = (float)fmaxf(0, fminf(WORLD_H, G.curY));

            if (pad.Buttons & PSP_CTRL_LTRIGGER) G.zoom = (float)fmaxf(0.4, G.zoom - 0.6f * dt);
            if (pad.Buttons & PSP_CTRL_RTRIGGER) G.zoom = (float)fminf(1.8, G.zoom + 0.6f * dt);

            if (pressed(&pad, PSP_CTRL_CROSS)) { issue_order_to_selected(ORD_MOVE, G.curX, G.curY); sfx_ui(); }
            if (pressed(&pad, PSP_CTRL_CIRCLE)) { issue_order_to_selected(ORD_ATTACK, G.curX, G.curY); sfx_ui(); }
            if (pressed(&pad, PSP_CTRL_TRIANGLE)) { issue_order_to_selected(ORD_DEFEND, G.curX, G.curY); sfx_ui(); }
            if (pressed(&pad, PSP_CTRL_SQUARE)) { respawn_player(); }
            if (pressed(&pad, PSP_CTRL_START)) { G.state = ST_PAUSE; }

            game_update(dt);
        } else if (G.state == ST_PAUSE) {
            if (pressed(&pad, PSP_CTRL_START)) G.state = ST_PLAY;
        } else if (G.state == ST_WIN || G.state == ST_LOSE) {
            if (pressed(&pad, PSP_CTRL_CROSS)) { game_new_battle(G.playerFaction); sfx_ui(); }
            if (pressed(&pad, PSP_CTRL_START)) { game_init(); sfx_ui(); }
            game_update(dt);
        }

        float vw = 480.0f / G.zoom, vh = 272.0f / G.zoom;
        float sxoff = 0, syoff = 0;
        if (G.shake > 0) { sxoff = (float)((rand() & 7) - 3) * G.shake; syoff = (float)((rand() & 7) - 3) * G.shake; }
        G.camX = G.curX - vw / 2 + sxoff;
        G.camY = G.curY - vh / 2 + syoff;
        if (G.camX < 0) G.camX = 0;
        if (G.camY < 0) G.camY = 0;
        if (G.camX + vw > WORLD_W) G.camX = WORLD_W - vw;
        if (G.camY + vh > WORLD_H) G.camY = WORLD_H - vh;
        if (vw >= WORLD_W) G.camX = 0;
        if (vh >= WORLD_H) G.camY = 0;

        audio_update();

        render_begin();
        if (G.state == ST_TITLE) render_title();
        else if (G.state == ST_PAUSE) render_paused();
        else if (G.state == ST_WIN) render_end_screen(1);
        else if (G.state == ST_LOSE) render_end_screen(0);
        else render_play();
        render_end();

        prevButtons = pad.Buttons;
        sceDisplayWaitVblankStart();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}

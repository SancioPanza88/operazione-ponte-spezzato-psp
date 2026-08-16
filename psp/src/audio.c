#include "game.h"
#include <pspaudio.h>

#define SR 44100
#define SAMPLES 512

static int g_chan = -1;
static short g_buf[SAMPLES];

typedef struct { int type; int pos; int len; float vol; } Voice;
static Voice voices[8];

static unsigned int a_rng = 0x9e3779b9u;
static int arand(void) { a_rng = a_rng * 1664525u + 1013904223u; return (int)(a_rng & 0xff); }

static int voice_len(int type) {
    if (type == 0) return (int)(0.06f * SR);
    if (type == 1) return (int)(0.5f * SR);
    if (type == 2) return (int)(0.3f * SR);
    return (int)(0.09f * SR);
}

static short gen_voice(int type, int i) {
    if (type == 0) {
        float env = (float)expf(-(float)i / (0.05f * SR));
        return (short)((arand() - 128) * env * 0.5f);
    }
    if (type == 1) {
        float env = (float)expf(-(float)i / (0.4f * SR));
        float n = (arand() - 128) * env * 1.1f;
        float t = (float)i / SR;
        float freq = 90.0f - 62.0f * (t / 0.45f);
        if (freq < 28) freq = 28;
        float s = (float)sinf(2 * 3.14159265f * freq * t) * env * 3000.0f;
        return (short)(n + s);
    }
    if (type == 2) {
        float t = (float)i / SR;
        float env = (float)expf(-(float)i / (0.25f * SR));
        float freq = 220.0f - 160.0f * (t / 0.28f);
        return (short)((float)sinf(2 * 3.14159265f * freq * t) * env * 3500.0f);
    }
    float t = (float)i / SR;
    return (short)(((t * 330.0f * 2) > 1 ? 1 : 0) * 2 - 1) * 3000;
}

static void enqueue(int type, float vol) {
    for (int i = 0; i < 8; i++) {
        if (voices[i].pos >= voices[i].len || voices[i].len == 0) {
            voices[i].type = type; voices[i].pos = 0; voices[i].len = voice_len(type); voices[i].vol = vol;
            return;
        }
    }
}

void audio_init(void) {
    sceAudioSetFrequency(PSP_AUDIO_FREQ_44K);
    g_chan = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, PSP_AUDIO_SAMPLE_ALIGN(SAMPLES), PSP_AUDIO_FORMAT_MONO);
    for (int i = 0; i < 8; i++) voices[i].len = 0;
}

void sfx_shot(void) { enqueue(0, 0.6f); }
void sfx_explosion(void) { enqueue(1, 1.0f); }
void sfx_death(void) { enqueue(2, 0.7f); }
void sfx_ui(void) { enqueue(3, 0.6f); }

void audio_update(void) {
    if (g_chan < 0) return;
    for (int i = 0; i < SAMPLES; i++) {
        int acc = 0;
        for (int v = 0; v < 8; v++) {
            if (voices[v].pos < voices[v].len) {
                acc += (int)(gen_voice(voices[v].type, voices[v].pos) * voices[v].vol);
                voices[v].pos++;
            }
        }
        if (acc > 32767) acc = 32767;
        if (acc < -32768) acc = -32768;
        g_buf[i] = (short)acc;
    }
    for (int v = 0; v < 8; v++) if (voices[v].pos >= voices[v].len) voices[v].len = 0;

    if (G.state == ST_PLAY) {
        static float amb = 0;
        amb -= 1.0f / 60.0f;
        if (amb <= 0) {
            amb = 0.25f + ((arand() & 31) / 31.0f) * 0.5f;
            if ((arand() & 3) == 0) enqueue(1, 0.35f); else enqueue(0, 0.3f);
        }
    }
    sceAudioOutputBlocking(g_chan, PSP_AUDIO_VOLUME_MAX, g_buf);
}

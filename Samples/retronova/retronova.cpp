#include <xtl.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "SDL_test_common.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480
#define NUM_STARS 200
#define NUM_VERTICES 8
#define SINE_TABLE_SIZE 360

static SDLTest_CommonState* state;
static int done = 0;
static float sineTable[SINE_TABLE_SIZE];

// ---- Starfield data ----
typedef struct {
    float x, y, z;
    Uint8 r, g, b;
} Star;

static Star stars[NUM_STARS];

// -------------------- Math helpers --------------------
static void InitSineTable(void) {
    for (int i = 0; i < SINE_TABLE_SIZE; i++) {
        sineTable[i] = (float)sin(i * M_PI / 180.0f);
    }
}

static float GetSine(int angle) {
    angle %= SINE_TABLE_SIZE;
    if (angle < 0) angle += SINE_TABLE_SIZE;
    return sineTable[angle];
}

// -------------------- Starfield --------------------
static void InitStars(void) {
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].x = (float)(rand() % WINDOW_WIDTH - WINDOW_WIDTH / 2);
        stars[i].y = (float)(rand() % WINDOW_HEIGHT - WINDOW_HEIGHT / 2);
        stars[i].z = (float)(rand() % 200 + 1); // 1..200
        stars[i].r = (Uint8)(rand() % 256);
        stars[i].g = (Uint8)(rand() % 256);
        stars[i].b = (Uint8)(rand() % 256);
    }
}

static void UpdateStars(float dt) {
    // Move toward camera; dt ~= 1 at 60fps
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].z -= 2.0f * dt;
        if (stars[i].z <= 0.0f) {
            stars[i].x = (float)(rand() % WINDOW_WIDTH - WINDOW_WIDTH / 2);
            stars[i].y = (float)(rand() % WINDOW_HEIGHT - WINDOW_HEIGHT / 2);
            stars[i].z = 200.0f;
        }
    }
}

static void DrawStars(SDL_Renderer* renderer) {
    for (int i = 0; i < NUM_STARS; i++) {
        // 3D -> 2D (very simple perspective)
        int screen_x = (int)((stars[i].x / stars[i].z) * 100.0f + WINDOW_WIDTH / 2);
        int screen_y = (int)((stars[i].y / stars[i].z) * 100.0f + WINDOW_HEIGHT / 2);

        // Scale size with depth; clamp to at least 1 pixel
        int size = (int)((1.0f - stars[i].z / 200.0f) * 3.0f);
        if (size < 1) size = 1;

        SDL_SetRenderDrawColor(renderer, stars[i].r, stars[i].g, stars[i].b, 255);
        SDL_Rect rect = { screen_x - size / 2, screen_y - size / 2, size, size };
        SDL_RenderFillRect(renderer, &rect);
    }
}

// -------------------- Rotating cube --------------------
static void Draw3DCube(SDL_Renderer* renderer, Uint32 time_ms) {
    static float vertices[NUM_VERTICES][3] = {
        {-50, -50, -50}, { 50, -50, -50}, { 50,  50, -50}, {-50,  50, -50},
        {-50, -50,  50}, { 50, -50,  50}, { 50,  50,  50}, {-50,  50,  50}
    };
    static int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    float angle = (float)time_ms / 1000.0f;
    float cosA = cosf(angle), sinA = sinf(angle);

    // Draw edges
    for (int i = 0; i < 12; i++) {
        int v0 = edges[i][0];
        int v1 = edges[i][1];

        float x0 = vertices[v0][0], y0 = vertices[v0][1], z0 = vertices[v0][2];
        float x1 = vertices[v1][0], y1 = vertices[v1][1], z1 = vertices[v1][2];

        // Rotate around Y
        float rx0 = x0 * cosA - z0 * sinA, rz0 = x0 * sinA + z0 * cosA;
        float rx1 = x1 * cosA - z1 * sinA, rz1 = x1 * sinA + z1 * cosA;

        // Project
        const float proj = 300.0f; // logical-space scale
        int sx0 = (int)((rx0 / (rz0 + 200.0f)) * proj + WINDOW_WIDTH / 2);
        int sy0 = (int)((y0 / (rz0 + 200.0f)) * proj + WINDOW_HEIGHT / 2);
        int sx1 = (int)((rx1 / (rz1 + 200.0f)) * proj + WINDOW_WIDTH / 2);
        int sy1 = (int)((y1 / (rz1 + 200.0f)) * proj + WINDOW_HEIGHT / 2);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(renderer, sx0, sy0, sx1, sy1);
    }
}

// -------------------- Sine wave --------------------
static void DrawSineWave(SDL_Renderer* renderer, Uint32 time_ms) {
    const int waveAmplitude = 100;
    const int waveFrequency = 6;
    const int thickness = 3;

    for (int x = 0; x < WINDOW_WIDTH; x++) {
        int y = (int)(WINDOW_HEIGHT / 2 +
            waveAmplitude * GetSine((x * waveFrequency + (int)(time_ms / 5))));

        Uint8 r = (Uint8)((GetSine((x + (int)(time_ms / 10)) % SINE_TABLE_SIZE) + 1.0f) * 127);
        Uint8 g = (Uint8)((GetSine((x + (int)(time_ms / 15)) % SINE_TABLE_SIZE) + 1.0f) * 127);
        Uint8 b = (Uint8)((GetSine((x + (int)(time_ms / 20)) % SINE_TABLE_SIZE) + 1.0f) * 127);

        SDL_SetRenderDrawColor(renderer, r, g, b, 255);

        for (int t = -thickness / 2; t <= thickness / 2; t++) {
            SDL_RenderDrawPoint(renderer, x, y + t);
        }
    }
}

// -------------------- Main loop --------------------
static void loop(void) {
    static Uint32 last_ticks = 0;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, &done);
    }

    Uint32 now = SDL_GetTicks();
    float dt = (last_ticks ? (now - last_ticks) : 16) / 16.6667f; // ~1.0 at 60Hz
    last_ticks = now;

    for (int i = 0; i < state->num_windows; i++) {
        SDL_Renderer* renderer = state->renderers[i];
        if (!renderer) continue;

        // Clear
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Draw
        UpdateStars(dt);
        DrawStars(renderer);
        DrawSineWave(renderer, now);
        Draw3DCube(renderer, now);

        SDL_RenderPresent(renderer);
    }
}

// -------------------- Entry point --------------------
int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL));

    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) return 1;
    if (!SDLTest_CommonInit(state)) return 2;

    // Scale our 640x480 logical world to the actual output (e.g., 1920x1080 on Xbox)
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear"); // nicer scaling for lines/rects
    for (int i = 0; i < state->num_windows; ++i) {
        if (state->renderers[i]) {
            SDL_RenderSetLogicalSize(state->renderers[i], WINDOW_WIDTH, WINDOW_HEIGHT);
            // Optional: crisp integer scaling for pixel-art look
            // SDL_RenderSetIntegerScale(state->renderers[i], SDL_TRUE);
        }
    }

    InitSineTable();
    InitStars();

    while (!done) {
        loop();
    }

    SDLTest_CommonQuit(state);
    return 0;
}

#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Display configuration
#define RENDER_WIDTH 320
#define RENDER_HEIGHT 200  // render area, can be smaller than physical screen

// Fixed-point precision (16.16 format)
#define FP_SHIFT 16
#define FP_ONE (1 << FP_SHIFT)
#define INT_TO_FP(x) ((x) << FP_SHIFT)
#define FP_TO_INT(x) ((x) >> FP_SHIFT)
#define FP_MUL(a, b) (((int64_t)(a) * (b)) >> FP_SHIFT)
#define FP_DIV(a, b) (((int64_t)(a) << FP_SHIFT) / (b))

// Vertex
struct Vertex {
    int32_t x, y, z;     // World position (fixed-point)
    int32_t sx, sy;      // Screen position (fixed-point), negative sx indicates clipped/behind
    uint16_t color;
};

// API
bool voodoo_init(TFT_eSPI *externalTft = nullptr);
void voodoo_clearBuffers(uint16_t color);
void voodoo_swapBuffers();
void voodoo_present();
void voodoo_setCamera(int32_t x, int32_t y, int32_t z, int16_t yaw, int16_t pitch);
void voodoo_moveCamera(int32_t dx, int32_t dy, int32_t dz);
void voodoo_rotateCamera(int16_t dyaw, int16_t dpitch);
void voodoo_drawCube(int32_t cx, int32_t cy, int32_t cz, int32_t size, uint16_t color);
void voodoo_frameOnce(); // render one frame (non-blocking)

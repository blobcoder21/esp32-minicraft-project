#include "3dfx.h"
#include <TFT_eSPI.h>
#include <math.h>

// Display object
static TFT_eSPI* tft = nullptr;

// Framebuffers in PSRAM
static uint16_t* frontBuffer = nullptr;
static uint16_t* backBuffer = nullptr;
static uint16_t* zBuffer = nullptr;

// Camera state
struct Camera {
    int32_t x, y, z;
    int16_t yaw;    // 0-1023 (0-360 degrees)
    int16_t pitch;  // -256 to +256 (-90 to +90 degrees)
    int32_t fov;
} camera = { INT_TO_FP(0), INT_TO_FP(0), INT_TO_FP(0), 0, 0, INT_TO_FP(60) };

// Fast sin/cos using float math (could add LUT later)
inline int32_t fastSin(int16_t angle) {
    return (int32_t)(sin(angle * 2.0f * M_PI / 1024.0f) * FP_ONE);
}

inline int32_t fastCos(int16_t angle) {
    return (int32_t)(cos(angle * 2.0f * M_PI / 1024.0f) * FP_ONE);
}

bool voodoo_init(TFT_eSPI *externalTft) {
    if (externalTft) {
        tft = externalTft;
    } else {
        static TFT_eSPI defaultTft;
        tft = &defaultTft;
        tft->init();
        tft->setRotation(1);
    }
    
    // Allocate buffers in PSRAM
    size_t bufPixels = RENDER_WIDTH * RENDER_HEIGHT;
    frontBuffer = (uint16_t*)ps_malloc(bufPixels * sizeof(uint16_t));
    backBuffer  = (uint16_t*)ps_malloc(bufPixels * sizeof(uint16_t));
    zBuffer     = (uint16_t*)ps_malloc(bufPixels * sizeof(uint16_t));
    
    if (!frontBuffer || !backBuffer || !zBuffer) {
        return false;
    }
    
    voodoo_clearBuffers(0x0000);
    tft->fillScreen(TFT_BLACK);
    return true;
}

void voodoo_clearBuffers(uint16_t color) {
    int total = RENDER_WIDTH * RENDER_HEIGHT;
    for (int i = 0; i < total; ++i) {
        backBuffer[i] = color;
        zBuffer[i] = 0xFFFF;
    }
}

void voodoo_swapBuffers() {
    uint16_t* tmp = frontBuffer;
    frontBuffer = backBuffer;
    backBuffer = tmp;
}

void voodoo_present() {
    tft->pushImage(0, 0, RENDER_WIDTH, RENDER_HEIGHT, frontBuffer);
}

// Camera control with proper clamping
void voodoo_setCamera(int32_t x, int32_t y, int32_t z, int16_t yaw, int16_t pitch) {
    camera.x = x;
    camera.y = y;
    camera.z = z;
    
    // Yaw wraps around (360 degrees)
    camera.yaw = yaw & 1023;
    
    // Pitch clamped to prevent gimbal lock
    if (pitch > 256) pitch = 256;
    else if (pitch < -256) pitch = -256;
    camera.pitch = pitch;
}

void voodoo_moveCamera(int32_t dx, int32_t dy, int32_t dz) {
    camera.x += dx;
    camera.y += dy;
    camera.z += dz;
}

void voodoo_rotateCamera(int16_t dyaw, int16_t dpitch) {
    camera.yaw = (camera.yaw + dyaw) & 1023;
    
    int16_t newPitch = camera.pitch + dpitch;
    if (newPitch > 256) newPitch = 256;
    else if (newPitch < -256) newPitch = -256;
    camera.pitch = newPitch;
}

// Transform vertex from world space to screen space
void voodoo_transformVertex(Vertex* v) {
    // Translate to camera space
    int32_t x = v->x - camera.x;
    int32_t y = v->y - camera.y;
    int32_t z = v->z - camera.z;

    // Rotate around Y axis (yaw)
    int32_t cosYaw = fastCos(camera.yaw);
    int32_t sinYaw = fastSin(camera.yaw);
    int32_t xRot = FP_MUL(x, cosYaw) - FP_MUL(z, sinYaw);
    int32_t zRot = FP_MUL(x, sinYaw) + FP_MUL(z, cosYaw);

    // Rotate around X axis (pitch)
    int32_t cosPitch = fastCos(camera.pitch);
    int32_t sinPitch = fastSin(camera.pitch);
    int32_t yRot = FP_MUL(y, cosPitch) - FP_MUL(zRot, sinPitch);
    int32_t zFinal = FP_MUL(y, sinPitch) + FP_MUL(zRot, cosPitch);

    // Perspective projection with near plane clipping
    if (zFinal > FP_ONE / 4) {
        int32_t halfWidth = INT_TO_FP(RENDER_WIDTH / 2);
        int32_t halfHeight = INT_TO_FP(RENDER_HEIGHT / 2);
        
        int32_t scale = FP_DIV(halfWidth, zFinal);
        v->sx = FP_MUL(xRot, scale) + halfWidth;
        v->sy = FP_MUL(yRot, scale) + halfHeight;
    } else {
        // Behind camera - mark as clipped
        v->sx = -FP_ONE;
        v->sy = -FP_ONE;
    }
}

// Low-level pixel writer
inline void voodoo_drawPixelRaw(int x, int y, uint16_t color) {
    if (x < 0 || x >= RENDER_WIDTH || y < 0 || y >= RENDER_HEIGHT) return;
    backBuffer[y * RENDER_WIDTH + x] = color;
}

// Bresenham line algorithm
void voodoo_drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    
    while (true) {
        voodoo_drawPixelRaw(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Draw wireframe cube
void voodoo_drawCube(int32_t cx, int32_t cy, int32_t cz, int32_t size, uint16_t color) {
    Vertex v[8];
    int32_t hs = size / 2;
    
    // Define 8 vertices of the cube
    v[0] = {cx - hs, cy - hs, cz - hs, 0, 0, color};
    v[1] = {cx + hs, cy - hs, cz - hs, 0, 0, color};
    v[2] = {cx + hs, cy + hs, cz - hs, 0, 0, color};
    v[3] = {cx - hs, cy + hs, cz - hs, 0, 0, color};
    v[4] = {cx - hs, cy - hs, cz + hs, 0, 0, color};
    v[5] = {cx + hs, cy - hs, cz + hs, 0, 0, color};
    v[6] = {cx + hs, cy + hs, cz + hs, 0, 0, color};
    v[7] = {cx - hs, cy + hs, cz + hs, 0, 0, color};

    // Transform all vertices
    for (int i = 0; i < 8; i++) {
        voodoo_transformVertex(&v[i]);
    }

    // 12 edges of a cube
    int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},  // front face
        {4,5}, {5,6}, {6,7}, {7,4},  // back face
        {0,4}, {1,5}, {2,6}, {3,7}   // connecting edges
    };

    // Draw each edge
    for (int i = 0; i < 12; i++) {
        Vertex &a = v[edges[i][0]];
        Vertex &b = v[edges[i][1]];
        
        // Skip if either vertex is clipped
        if (a.sx < 0 || b.sx < 0) continue;
        
        // Convert to integer coords
        int x0 = FP_TO_INT(a.sx);
        int y0 = FP_TO_INT(a.sy);
        int x1 = FP_TO_INT(b.sx);
        int y1 = FP_TO_INT(b.sy);
        
        // Quick bounds check
        if ((x0 < -16 && x1 < -16) || (y0 < -16 && y1 < -16) ||
            (x0 > RENDER_WIDTH + 16 && x1 > RENDER_WIDTH + 16) ||
            (y0 > RENDER_HEIGHT + 16 && y1 > RENDER_HEIGHT + 16)) {
            continue;
        }
        
        voodoo_drawLine(x0, y0, x1, y1, color);
    }
}

// Render one frame (called from loop)
void voodoo_frameOnce() {
    static uint32_t lastFps = 0;
    static int fpsCount = 0;
    static int16_t angle = 0;

    voodoo_clearBuffers(0x0000);
    
    // Rotate camera around the cube
    voodoo_setCamera(INT_TO_FP(0), INT_TO_FP(0), INT_TO_FP(0), angle, 85);
    
    // Draw cube in front of camera
    voodoo_drawCube(INT_TO_FP(0), INT_TO_FP(0), INT_TO_FP(10), INT_TO_FP(3), 0xF81F);
    
    angle = (angle + 4) & 1023;
    
    voodoo_swapBuffers();
    voodoo_present();

    fpsCount++;
    if (millis() - lastFps >= 1000) {
        Serial.printf("3dfx: FPS = %d\n", fpsCount);
        fpsCount = 0;
        lastFps = millis();
    }
}
#include <TFT_eSPI.h>
#include <tgx.h>
using namespace tgx;

TFT_eSPI tft = TFT_eSPI();
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

uint16_t* fb = nullptr;
Image<RGB565> img;

// Cube vertices (unit cube)
Vec3<float> cubeVerts[8] = {
    {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}
};

// Edges (pairs of vertex indices)
const uint8_t edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0}, // bottom
    {4,5},{5,6},{6,7},{7,4}, // top
    {0,4},{1,5},{2,6},{3,7}  // verticals
};

// Project 3D -> 2D (simple)
void project(const Vec3<float> &v, int &x, int &y, float fov = 64.0f) {
    float z = v.z + 2.0f;
    x = SCREEN_WIDTH/2 + (v.x * fov) / z;
    y = SCREEN_HEIGHT -100- (v.y * fov) / z;  
}
// Draw a cube at offset
void drawCube(const Vec3<float> offset) {
    int sx[8], sy[8];
    for(int i=0;i<8;i++){
        Vec3<float> v = cubeVerts[i];
        v.x += offset.x;
        v.y += offset.y;
        v.z += offset.z;
        project(v,sx[i],sy[i]);
    }
    for(int i=0;i<12;i++){
        img.drawLine(sx[edges[i][0]],sy[edges[i][0]], sx[edges[i][1]],sy[edges[i][1]]);
    }
}

#define CHUNK_SIZE 8
bool voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];

// 6 face directions: right, left, top, bottom, front, back
const int faceDir[6][3] = {
    {1, 0, 0},   // right
    {-1, 0, 0},  // left
    {0, 1, 0},   // top
    {0, -1, 0},  // bottom
    {0, 0, 1},   // front
    {0, 0, -1}   // back
};

// Face vertices (relative to voxel position)
const float faceVerts[6][4][3] = {
    // Right face (+X)
    {{0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}},
    // Left face (-X)
    {{-0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, 0.5f}},
    // Top face (+Y)
    {{-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}},
    // Bottom face (-Y)
    {{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}},
    // Front face (+Z)
    {{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}},
    // Back face (-Z)
    {{0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}}
};

void initVoxels() {
    memset(voxels, 0, sizeof(voxels));
    // Flat ground plane
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            voxels[x][0][z] = true;
        }
    }
}

bool hasVoxel(int x, int y, int z) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return false;
    return voxels[x][y][z];
}

void drawQuadFilled(float cx, float cy, float cz, int faceIdx) {
    int sx[4], sy[4];
    for (int i = 0; i < 4; i++) {
        Vec3<float> v = {
            cx + faceVerts[faceIdx][i][0],
            cy + faceVerts[faceIdx][i][1],
            cz + faceVerts[faceIdx][i][2]
        };
        project(v, sx[i], sy[i]);
    }
    
    // Fill with RGB565_Gray
    img.fillTriangle({sx[0], sy[0]}, {sx[1], sy[1]}, {sx[2], sy[2]}, RGB565_Gray, RGB565_Gray);
    img.fillTriangle({sx[0], sy[0]}, {sx[2], sy[2]}, {sx[3], sy[3]}, RGB565_Gray, RGB565_Gray);
    
    // Draw RGB565_Red edges
    for (int i = 0; i < 4; i++) {
        img.drawLine({sx[i], sy[i]}, {sx[(i+1)%4], sy[(i+1)%4]}, RGB565_Red);
    }
}

void drawVoxelMesh() {
    for (int y = 0; y < CHUNK_SIZE; y++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                if (!hasVoxel(x, y, z)) continue;
                
                // Check all 6 faces
                for (int face = 0; face < 6; face++) {
                    int nx = x + faceDir[face][0];
                    int ny = y + faceDir[face][1];
                    int nz = z + faceDir[face][2];
                    
                    // Draw face if neighbor is empty
                    if (!hasVoxel(nx, ny, nz)) {
                        drawQuadFilled(x, y, z, face);
                    }
                }
            }
        }
    }
}

void setup() {
    tft.init(); tft.setRotation(0);
    fb = (uint16_t*)ps_malloc(SCREEN_WIDTH*SCREEN_HEIGHT*sizeof(uint16_t));
    if(!fb) while(1);
    img.set(fb,SCREEN_WIDTH,SCREEN_HEIGHT);
}

void loop() {
    img.fillScreen(RGB565_Black);
    
    // Draw ground plane grid
    for (int z = 0; z < 8; z++) {
        for (int x = -4; x <= 4; x++) {
            drawCube({x * 1.0f, -1.5f, z * 1.2f});
        }
    }
    
    tft.pushImage(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,fb);
    delay(20);
}
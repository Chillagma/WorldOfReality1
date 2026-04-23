#define NOMINMAX

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <random>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include "game.h"
#include <thread>
#include <atomic>
// ============================================================================
// Utilities
// ============================================================================

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { std::fprintf(stderr, "Failed to open: %s\n", path.c_str()); return {}; }
    const auto size = f.tellg();
    f.seekg(0);
    std::string content(static_cast<size_t>(size), '\0');
    f.read(content.data(), size);
    return content;
}

GLuint compileShader(GLenum type, const char* src) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader compile error:\n%s\n", log);
        glDeleteShader(id);
        return 0;
    }
    return id;
}

GLuint createProgram(const char* vsSrc, const char* fsSrc) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!v || !f) { if (v) glDeleteShader(v); if (f) glDeleteShader(f); return 0; }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Program link error:\n%s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

bool createFboTexturePair(GLuint& fbo, GLuint& tex, int w, int h) {
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
    glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

// ============================================================================
// STL Loader
// ============================================================================

static const int MESH_TEX_WIDTH = 1024;

// Hard cap — GLSL loop will freeze/crash GPU if too many triangles
// 500 is safe for a raymarcher running every pixel every frame
static const int MAX_TRIANGLES = 500;
static const int SDF_RES = 64; // 64x64x64 grid, baked once at startup



GLuint bakeSTLtoSDF(const std::string& path, int& numTriangles) {
    numTriangles = 0;

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { std::fprintf(stderr, "Cannot open %s\n", path.c_str()); return 0; }

    char header[80];
    fread(header, 80, 1, f);
    uint32_t triCount = 0;
    fread(&triCount, 4, 1, f);

    struct Tri { float ax, ay, az, bx, by, bz, cx, cy, cz; };
    std::vector<Tri> tris(triCount);

    for (uint32_t i = 0; i < triCount; i++) {
        float n[3]; uint16_t attr;
        fread(n, 12, 1, f);
        fread(&tris[i].ax, 4, 1, f); fread(&tris[i].ay, 4, 1, f); fread(&tris[i].az, 4, 1, f);
        fread(&tris[i].bx, 4, 1, f); fread(&tris[i].by, 4, 1, f); fread(&tris[i].bz, 4, 1, f);
        fread(&tris[i].cx, 4, 1, f); fread(&tris[i].cy, 4, 1, f); fread(&tris[i].cz, 4, 1, f);
        fread(&attr, 2, 1, f);
    }
    fclose(f);
    numTriangles = triCount;

    // Bounding box
    float bminX = 1e10f, bminY = 1e10f, bminZ = 1e10f;
    float bmaxX = -1e10f, bmaxY = -1e10f, bmaxZ = -1e10f;
    for (auto& t : tris) {
        bminX = std::min({ bminX,t.ax,t.bx,t.cx });
        bminY = std::min({ bminY,t.ay,t.by,t.cy });
        bminZ = std::min({ bminZ,t.az,t.bz,t.cz });
        bmaxX = std::max({ bmaxX,t.ax,t.bx,t.cx });
        bmaxY = std::max({ bmaxY,t.ay,t.by,t.cy });
        bmaxZ = std::max({ bmaxZ,t.az,t.bz,t.cz });
    }
    float cx = (bminX + bmaxX) * 0.5f;
    float cy = (bminY + bmaxY) * 0.5f;
    float cz = (bminZ + bmaxZ) * 0.5f;
    float ext = std::max({ bmaxX - bminX,bmaxY - bminY,bmaxZ - bminZ }) * 0.6f;
    if (ext < 1e-8f) ext = 1.0f;

    int R = SDF_RES;
    int slicesPerRow = (int)ceilf(sqrtf((float)R));
    int atlasW = slicesPerRow * R;
    int atlasH = ((R + slicesPerRow - 1) / slicesPerRow) * R;
    std::vector<float> sdf(atlasW * atlasH, 1.0f);

    std::printf("Baking SDF %dx%dx%d from %d triangles on %d threads...\n",
        R, R, R, triCount, (int)std::thread::hardware_concurrency());

    // Point to triangle distance (standalone function for threads)
    auto ptTri = [](float px, float py, float pz,
        float ax, float ay, float az,
        float bx, float by, float bz,
        float cx, float cy, float cz) -> float {
            auto edgeDist = [](float px, float py, float pz,
                float ax, float ay, float az,
                float bx, float by, float bz) -> float {
                    float abx = bx - ax, aby = by - ay, abz = bz - az;
                    float apx = px - ax, apy = py - ay, apz = pz - az;
                    float t = std::max(0.f, std::min(1.f,
                        (apx * abx + apy * aby + apz * abz) / (abx * abx + aby * aby + abz * abz + 1e-10f)));
                    float dx = apx - abx * t, dy = apy - aby * t, dz = apz - abz * t;
                    return sqrtf(dx * dx + dy * dy + dz * dz);
                };
            return std::min({ edgeDist(px,py,pz,ax,ay,az,bx,by,bz),
                             edgeDist(px,py,pz,bx,by,bz,cx,cy,cz),
                             edgeDist(px,py,pz,cx,cy,cz,ax,ay,az) });
        };

    auto isInside = [&](float px, float py, float pz) -> bool {
        int crossings = 0;
        for (auto& t : tris) {
            float ax = t.ax - px, az = t.az - pz;
            float bx = t.bx - px, bz = t.bz - pz;
            float cx = t.cx - px, cz = t.cz - pz;
            float d0 = ax * bz - az * bx, d1 = bx * cz - bz * cx, d2 = cx * az - cz * ax;
            bool has_neg = (d0 < 0) || (d1 < 0) || (d2 < 0);
            bool has_pos = (d0 > 0) || (d1 > 0) || (d2 > 0);
            if (has_neg && has_pos) continue;
            float area = d0 + d1 + d2;
            if (std::abs(area) < 1e-10f) continue;
            float hitY = (d1 / area) * t.ay + (d2 / area) * t.by + (1.f - d1 / area - d2 / area) * t.cy;
            if (hitY > py) crossings++;
        }
        return (crossings % 2) == 1;
        };

    // Split Z slices across threads
    int numThreads = (int)std::thread::hardware_concurrency();
    if (numThreads < 1) numThreads = 4;
    std::atomic<int> slicesDone(0);
    std::vector<std::thread> threads(numThreads);

    for (int ti = 0; ti < numThreads; ti++) {
        threads[ti] = std::thread([&, ti]() {
            for (int z = ti; z < R; z += numThreads) {
                for (int y = 0; y < R; y++) {
                    for (int x = 0; x < R; x++) {
                        float wx = cx + ((x + 0.5f) / R - 0.5f) * 2.0f * ext;
                        float wy = cy + ((y + 0.5f) / R - 0.5f) * 2.0f * ext;
                        float wz = cz + ((z + 0.5f) / R - 0.5f) * 2.0f * ext;

                        float minD = 1e10f;
                        for (auto& t : tris) {
                            float d = ptTri(wx, wy, wz,
                                t.ax, t.ay, t.az,
                                t.bx, t.by, t.bz,
                                t.cx, t.cy, t.cz);
                            if (d < minD) minD = d;
                        }

                        if (isInside(wx, wy, wz)) minD = -minD;

                        float normalized = (minD / ext) * 0.5f + 0.5f;

                        int tileX = z % slicesPerRow;
                        int tileY = z / slicesPerRow;
                        int ax2 = tileX * R + x;
                        int ay2 = tileY * R + y;
                        sdf[ay2 * atlasW + ax2] = normalized;
                    }
                }
                // Safe progress print
                int done = ++slicesDone;
                std::printf("  slice %d/%d done\n", done, R);
                fflush(stdout);
            }
            });
    }

    // Wait for all threads to finish
    for (auto& t : threads) t.join();

    std::printf("SDF bake complete.\n");

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, atlasW, atlasH, 0, GL_RED, GL_FLOAT, sdf.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    std::printf("SDF texture: %dx%d atlas, %d slices per row\n",
        atlasW, atlasH, slicesPerRow);
    return tex;
}
// ============================================================================
// App State
// ============================================================================

struct App {
    GLFWwindow* window = nullptr;
    int fbW = 0, fbH = 0;
    GLuint quadVao = 0, quadVbo = 0;
    GLuint bufAFbo[2] = { 0 }, bufATex[2] = { 0 };
    int bufAIdx = 0;
    GLuint progBufferA = 0, progImage = 0;
    ShaderUniforms uniformsA, uniformsImage;
    bool keyW = false, keyS = false, keyA = false, keyD = false;
    bool mouseLeft = false;
    double mouseX = 0, mouseY = 0;
    Thumbnail goalThumbnail, playerThumbnail;
    CameraState goalCamera;
    PlayerCamera playerCamera;
    GoalGenerator goalGenerator;
    TextRenderer textRenderer;
    OverlayRenderer overlayRenderer;
    bool spacePressed = false;
    bool goalCaptured = false;
    bool showingSuccess = false;
    double successStartTime = 0.0;
    MatchResult lastMatch, liveMatch;

    GLuint meshTex = 0;
    int    numMeshTris = 0;
    int    meshTexWidth = MESH_TEX_WIDTH;
    // Replace in App struct:
   
    int    sdfRes = SDF_RES;
    int    sdfSlicesPerRow = 0;

    static constexpr float SUCCESS_THRESHOLD = 75.0f;
    static constexpr float SUCCESS_DISPLAY_TIME = 3.0f;
    static constexpr float THUMBNAIL_SIZE = 0.35f;
    static constexpr float MARGIN = 0.02f;
    static constexpr float MIN_OBJECT_COVERAGE = 0.10f;
    std::mt19937 rng;

    void recreateFbos() {
        for (int i = 0; i < 2; ++i) {
            if (bufATex[i]) glDeleteTextures(1, &bufATex[i]);
            if (bufAFbo[i]) glDeleteFramebuffers(1, &bufAFbo[i]);
            bufATex[i] = bufAFbo[i] = 0;
            createFboTexturePair(bufAFbo[i], bufATex[i], fbW, fbH);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void resetForNewGoal() {
        goalCaptured = false;
        goalGenerator.generateGoal(goalCamera, rng);
        playerThumbnail.hasContent = false;
        playerCamera.reset();
    }
    void bindMeshUniforms(GLuint prog) {
        if (meshTex == 0) return;
        GLint locTex = glGetUniformLocation(prog, "uMeshTex");
        GLint locRes = glGetUniformLocation(prog, "uSdfRes");
        GLint locSPR = glGetUniformLocation(prog, "uSlicesPerRow");

        static bool printed = false;
        if (!printed && locTex >= 0) {  // only print when we find valid uniforms
            std::printf("uniform locs: tex=%d res=%d spr=%d\n", locTex, locRes, locSPR);
            std::printf("uniform vals: res=%d spr=%d\n", sdfRes, sdfSlicesPerRow);
            printed = true;
        }

        if (locTex >= 0) glUniform1i(locTex, 2);
        if (locRes >= 0) glUniform1i(locRes, sdfRes);
        if (locSPR >= 0) glUniform1i(locSPR, sdfSlicesPerRow);
    }
};

App* gApp = nullptr;

// ============================================================================
// Callbacks
// ============================================================================

void framebuffer_size_cb(GLFWwindow*, int, int) {
    if (!gApp) return;
    glfwGetFramebufferSize(gApp->window, &gApp->fbW, &gApp->fbH);
    if (gApp->fbW <= 0 || gApp->fbH <= 0) return;
    glViewport(0, 0, gApp->fbW, gApp->fbH);
    gApp->recreateFbos();
}

void key_cb(GLFWwindow*, int key, int, int action, int) {
    if (!gApp) return;
    const bool down = (action == GLFW_PRESS || action == GLFW_REPEAT);
    switch (key) {
    case GLFW_KEY_W: gApp->keyW = down; break;
    case GLFW_KEY_S: gApp->keyS = down; break;
    case GLFW_KEY_A: gApp->keyA = down; break;
    case GLFW_KEY_D: gApp->keyD = down; break;
    case GLFW_KEY_SPACE:
        if (action == GLFW_PRESS && !gApp->spacePressed) gApp->spacePressed = true;
        else if (action == GLFW_RELEASE) gApp->spacePressed = false;
        break;
    case GLFW_KEY_R:
        if (action == GLFW_PRESS) gApp->resetForNewGoal();
        break;
    case GLFW_KEY_ESCAPE:
        if (action == GLFW_PRESS) {
            gApp->playerThumbnail.hasContent = false;
            gApp->showingSuccess = false;
        }
        break;
    }
}

void mouse_btn_cb(GLFWwindow*, int button, int action, int) {
    if (gApp && button == GLFW_MOUSE_BUTTON_LEFT)
        gApp->mouseLeft = (action == GLFW_PRESS || action == GLFW_REPEAT);
}

static void activateMeshTexture(App& app) {
    if (app.meshTex == 0) return;
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, app.meshTex);
}

// ============================================================================
// Goal Validation
// ============================================================================

float checkObjectCoverage(App& app, float time, float resX, float resY) {
    glBindVertexArray(app.quadVao);
    int readIdx = app.bufAIdx, writeIdx = 1 - readIdx;

    glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[writeIdx]);
    glViewport(0, 0, app.fbW, app.fbH);
    glUseProgram(app.progBufferA);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, app.bufATex[readIdx]);
    activateMeshTexture(app);
    app.bindMeshUniforms(app.progBufferA);
    app.uniformsA.setCommon(time, resX, resY, 0, 0, 0);
    app.uniformsA.setKeys(false, false, false, false);
    app.uniformsA.setCameraOverride(true, app.goalCamera);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    app.bufAIdx = writeIdx;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, app.fbW, app.fbH);
    glUseProgram(app.progImage);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, app.bufATex[app.bufAIdx]);
    activateMeshTexture(app);
    app.bindMeshUniforms(app.progImage);
    app.uniformsImage.setCommon(time, resX, resY, 0, 0, 0);
    app.uniformsImage.setCameraOverride(true, app.goalCamera);
    app.uniformsImage.setDetectionMode(true);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    std::vector<unsigned char> pixels(app.fbW * app.fbH * 3);
    glReadPixels(0, 0, app.fbW, app.fbH, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    int objectPixels = 0, totalChecked = 0;
    int marginX = app.fbW / 5, marginY = app.fbH / 5;
    for (int y = marginY; y < app.fbH - marginY; y += 4)
        for (int x = marginX; x < app.fbW - marginX; x += 4) {
            if (pixels[(y * app.fbW + x) * 3] < 64) objectPixels++;
            totalChecked++;
        }
    return static_cast<float>(objectPixels) / static_cast<float>(totalChecked);
}

void renderGoalFrame(App& app, float time, float resX, float resY) {
    glBindVertexArray(app.quadVao);
    int readIdx = app.bufAIdx, writeIdx = 1 - readIdx;

    glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[writeIdx]);
    glViewport(0, 0, app.fbW, app.fbH);
    glUseProgram(app.progBufferA);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, app.bufATex[readIdx]);
    activateMeshTexture(app);
    app.bindMeshUniforms(app.progBufferA);
    app.uniformsA.setCommon(time, resX, resY, 0, 0, 0);
    app.uniformsA.setKeys(false, false, false, false);
    app.uniformsA.setCameraOverride(true, app.goalCamera);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    app.bufAIdx = writeIdx;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, app.fbW, app.fbH);
    glUseProgram(app.progImage);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, app.bufATex[app.bufAIdx]);
    activateMeshTexture(app);
    app.bindMeshUniforms(app.progImage);
    app.uniformsImage.setCommon(time, resX, resY, 0, 0, 0);
    app.uniformsImage.setCameraOverride(true, app.goalCamera);
    app.uniformsImage.setDetectionMode(false);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// ============================================================================
// Main
// ============================================================================
//
int main() {
    std::string shaderDir = R"(shaders/)";

    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return EXIT_FAILURE; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef APPLE
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(1280, 720,
        "Camera Hunt (SPACE=check, R=new)", nullptr, nullptr);
    if (!window) { glfwTerminate(); return EXIT_FAILURE; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwDestroyWindow(window); glfwTerminate(); return EXIT_FAILURE;
    }

    App app;
    app.window = window;
    gApp = &app;

    std::random_device rd; app.rng.seed(rd());
    glfwGetFramebufferSize(window, &app.fbW, &app.fbH);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_cb);
    glfwSetKeyCallback(window, key_cb);
    glfwSetMouseButtonCallback(window, mouse_btn_cb);

    constexpr float quadVerts[] = { -1,-1, 1,-1, -1,1, -1,1, 1,-1, 1,1 };
    glGenVertexArrays(1, &app.quadVao);
    glGenBuffers(1, &app.quadVbo);
    glBindVertexArray(app.quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, app.quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);

    app.goalThumbnail.initShader(thumbnailVS, thumbnailFS, createProgram);
    app.playerThumbnail.initShader(thumbnailVS, thumbnailFS, createProgram);
    app.textRenderer.init(textVS, textFS, createProgram);
    app.overlayRenderer.init(thumbnailVS, overlayFS, createProgram);

    std::string vsSrc = readFile(shaderDir + "fullscreen.vert");
    std::string commonSrc = readFile(shaderDir + "common.glsl");
    std::string bufAFs = readFile(shaderDir + "buffer_a.frag");
    std::string imgFs = readFile(shaderDir + "image.frag");
    if (vsSrc.empty() || commonSrc.empty() || bufAFs.empty() || imgFs.empty()) {
        std::fprintf(stderr, "Failed to load shader files\n");
        glfwDestroyWindow(window); glfwTerminate(); return EXIT_FAILURE;
    }

    app.progBufferA = createProgram(vsSrc.c_str(), bufAFs.c_str());
    app.progImage = createProgram(vsSrc.c_str(), (commonSrc + imgFs).c_str());
    if (!app.progBufferA || !app.progImage) {
        glfwDestroyWindow(window); glfwTerminate(); return EXIT_FAILURE;
    }

    app.uniformsA.init(app.progBufferA);
    app.uniformsImage.init(app.progImage);

    glUseProgram(app.progBufferA);
    if (app.uniformsA.iChannel0 >= 0) glUniform1i(app.uniformsA.iChannel0, 0);
    glUseProgram(app.progImage);
    if (app.uniformsImage.iChannel0 >= 0) glUniform1i(app.uniformsImage.iChannel0, 0);
    glUseProgram(0);

    // Load STL — crashes here if file has too many triangles
    int dummyTris = 0;
    int slicesPerRow = (int)ceilf(sqrtf((float)SDF_RES));
    app.sdfSlicesPerRow = slicesPerRow;
    app.meshTex = bakeSTLtoSDF(shaderDir + "car.stl", dummyTris);
    // right after bakeSTLtoSDF call in main:
    std::printf("SDF: res=%d slicesPerRow=%d\n", SDF_RES, app.sdfSlicesPerRow);
    app.numMeshTris = dummyTris;

    app.recreateFbos();
    app.goalGenerator.generateGoal(app.goalCamera, app.rng);

    float aspectRatio = static_cast<float>(app.fbW) / app.fbH;
    app.goalThumbnail.setTargetRect(aspectRatio, App::THUMBNAIL_SIZE, App::MARGIN, true);
    app.playerThumbnail.setTargetRect(aspectRatio, App::THUMBNAIL_SIZE, App::MARGIN, false);

    bool shouldCapture = false;
    double fpsWindowStart = glfwGetTime();
    int fpsFrameCount = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glfwGetCursorPos(window, &app.mouseX, &app.mouseY);
        const double currentTime = glfwGetTime();
        if (app.spacePressed) { shouldCapture = true; app.spacePressed = false; }

        const float time = static_cast<float>(currentTime);
        const float resX = static_cast<float>(app.fbW);
        const float resY = static_cast<float>(app.fbH);
        const float mx = static_cast<float>(app.mouseX);
        const float my = static_cast<float>(app.fbH - app.mouseY);
        const float mLeft = app.mouseLeft ? 1.f : 0.f;

        float newAspect = resX / resY;
        if (std::abs(newAspect - aspectRatio) > 0.001f) {
            aspectRatio = newAspect;
            app.goalThumbnail.setTargetRect(aspectRatio, App::THUMBNAIL_SIZE, App::MARGIN, true);
            app.playerThumbnail.setTargetRect(aspectRatio, App::THUMBNAIL_SIZE, App::MARGIN, false);
        }
        app.playerCamera.update(resX, resY, mx, my,
            app.mouseLeft,
            app.keyW, app.keyS, app.keyA, app.keyD);

        if (!app.goalCaptured) {
            bool validGoal = false;
            int attempts = 0;
            const int maxAttempts = 100;
            while (!validGoal && attempts < maxAttempts) {
                attempts++;
                for (int frame = 0; frame < 3; frame++)
                    renderGoalFrame(app, time, resX, resY);
                float coverage = checkObjectCoverage(app, time, resX, resY);
                if (coverage >= App::MIN_OBJECT_COVERAGE) {
                    validGoal = true;
                    std::printf("Goal accepted: %.1f%% (attempt %d)\n", coverage * 100.f, attempts);
                }
                else {
                    std::printf("Goal rejected: %.1f%% (need %.0f%%), retrying...\n",
                        coverage * 100.f, App::MIN_OBJECT_COVERAGE * 100.f);
                    app.goalGenerator.generateGoal(app.goalCamera, app.rng);
                    for (int i = 0; i < 2; i++) {
                        glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[i]);
                        glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
                    }
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                }
            }
            if (!validGoal)
                std::printf("Warning: using best attempt after %d tries\n", maxAttempts);

            for (int frame = 0; frame < 5; frame++)
                renderGoalFrame(app, time, resX, resY);
            app.goalThumbnail.capture(app.fbW, app.fbH);
            app.goalCaptured = true;

            for (int i = 0; i < 2; i++) {
                glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[i]);
                glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            app.playerCamera.reset();
            std::printf("Goal: pos(%.2f,%.2f,%.2f)\n",
                app.goalCamera.posX, app.goalCamera.posY, app.goalCamera.posZ);
        }

        // Buffer A pass
        glBindVertexArray(app.quadVao);
        {
            int readIdx = app.bufAIdx, writeIdx = 1 - readIdx;
            glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[writeIdx]);
            glViewport(0, 0, app.fbW, app.fbH);
            glUseProgram(app.progBufferA);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app.bufATex[readIdx]);
          //  activateMeshTexture(app);
            //app.bindMeshUniforms(app.progBufferA);
            app.uniformsA.setCommon(time, resX, resY, mx, my, mLeft);
            app.uniformsA.setKeys(app.keyW, app.keyS, app.keyA, app.keyD);
            app.uniformsA.setCameraOverride(false, app.goalCamera);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            app.bufAIdx = writeIdx;
        }

        // Image pass
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, app.fbW, app.fbH);
            glUseProgram(app.progImage);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app.bufATex[app.bufAIdx]);
            activateMeshTexture(app);
            app.bindMeshUniforms(app.progImage);
            app.uniformsImage.setCommon(time, resX, resY, mx, my, mLeft);
            app.uniformsImage.setCameraOverride(false, app.goalCamera);
            app.uniformsImage.setDetectionMode(false);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glBindVertexArray(0);

        app.liveMatch = calculateMatch(app.goalCamera, app.playerCamera);
        if (shouldCapture) {
            app.playerThumbnail.capture(app.fbW, app.fbH);
            app.playerThumbnail.startAnimation(currentTime);
            app.lastMatch = app.liveMatch;
            std::printf("CAPTURE: %.0f%%\n", app.lastMatch.total);
            if (app.lastMatch.total >= App::SUCCESS_THRESHOLD) {
                app.showingSuccess = true;
                app.successStartTime = currentTime;
            }
            shouldCapture = false;
        }

        if (app.goalThumbnail.hasContent) {
            app.goalThumbnail.render(app.quadVao, currentTime, false, 1.f, 0.8f, 0.f, true);
            app.textRenderer.draw(app.quadVao, "GOAL",
                app.goalThumbnail.targetX + app.goalThumbnail.targetW * 0.3f,
                app.goalThumbnail.targetY + app.goalThumbnail.targetH * 0.85f,
                app.goalThumbnail.targetW * 0.4f,
                app.goalThumbnail.targetH * 0.1f, 1, 1, 1, 1);
        }
        if (app.playerThumbnail.hasContent) {
            float g = app.lastMatch.total / 100.f;
            app.playerThumbnail.render(app.quadVao, currentTime, true, 1.f - g, g, 0.2f, true);
        }

        {
            float hudX = -0.98f, hudY = -0.98f, hudW = 0.6f, hudH = 0.28f;
            app.overlayRenderer.draw(app.quadVao, hudX, hudY, hudW, hudH, 0, 0, 0, 0.6f);
            char buf[64];
            float textH = 0.05f, textW = 0.55f, textX = hudX + 0.02f, c;
            c = app.liveMatch.total / 100.f;
            std::snprintf(buf, sizeof(buf), "TOTAL: %d%%", (int)app.liveMatch.total);
            app.textRenderer.draw(app.quadVao, buf, textX, hudY + 0.20f, textW, textH, 1 - c, c, 0.3f, 1);
            c = app.liveMatch.position / 100.f;
            std::snprintf(buf, sizeof(buf), "POS: %d%%", (int)app.liveMatch.position);
            app.textRenderer.draw(app.quadVao, buf, textX, hudY + 0.12f, textW, textH, 1 - c, c, 0.3f, 1);
            c = app.liveMatch.direction / 100.f;
            std::snprintf(buf, sizeof(buf), "DIR: %d%%", (int)app.liveMatch.direction);
            app.textRenderer.draw(app.quadVao, buf, textX, hudY + 0.04f, textW, textH, 1 - c, c, 0.3f, 1);
        }

        if (app.showingSuccess) {
            float elapsed = static_cast<float>(currentTime - app.successStartTime);
            if (elapsed > App::SUCCESS_DISPLAY_TIME) {
                app.showingSuccess = false;
                app.resetForNewGoal();
            }
            else {
                float alpha = 0.7f;
                if (elapsed < 0.3f) alpha *= elapsed / 0.3f;
                else if (elapsed > App::SUCCESS_DISPLAY_TIME - 0.5f)
                    alpha *= (App::SUCCESS_DISPLAY_TIME - elapsed) / 0.5f;
                app.overlayRenderer.draw(app.quadVao, -0.5f, -0.15f, 1.f, 0.3f, 0.1f, 0.5f, 0.1f, alpha);
                app.textRenderer.draw(app.quadVao, "SUCCESS!", -0.35f, -0.05f, 0.7f, 0.15f, 1, 1, 1, 1);
                if (elapsed > 1.5f) {
                    float a = std::min((elapsed - 1.5f) / 0.3f, 1.f);
                    app.textRenderer.draw(app.quadVao, "NEW GOAL", -0.35f, -0.20f, 0.7f, 0.08f, 1, 1, 0.5f, a);
                }
            }
        }

        glfwSwapBuffers(window);

        fpsFrameCount++;
        const double fpsNow = glfwGetTime();
        if (fpsNow - fpsWindowStart >= 0.5) {
            const double fps = fpsFrameCount / (fpsNow - fpsWindowStart);
            fpsFrameCount = 0; fpsWindowStart = fpsNow;
            char title[192];
            std::snprintf(title, sizeof(title),
                "Camera Hunt — %.0f FPS (SPACE=check, R=new)", fps);
            glfwSetWindowTitle(window, title);
        }
    }

    if (app.meshTex) glDeleteTextures(1, &app.meshTex);
    app.goalThumbnail.cleanup();
    app.playerThumbnail.cleanup();
    app.textRenderer.cleanup();
    app.overlayRenderer.cleanup();
    glDeleteVertexArrays(1, &app.quadVao);
    glDeleteBuffers(1, &app.quadVbo);
    for (int i = 0; i < 2; ++i) {
        glDeleteTextures(1, &app.bufATex[i]);
        glDeleteFramebuffers(1, &app.bufAFbo[i]);
    }
    glDeleteProgram(app.progBufferA);
    glDeleteProgram(app.progImage);
    gApp = nullptr;
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
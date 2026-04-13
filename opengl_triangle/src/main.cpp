#define NOMINMAX // Tell Windows we don't want min/max macros that mess with C++ std::min/max

#include <glad/glad.h>   // OpenGL function loader (talks to graphics card)
#include <GLFW/glfw3.h>  // Window creation and input handling
#include <cstdio>        // C-style printing (printf)
#include <cstdlib>       // Standard library utilities
#include <fstream>       // Reading files from disk
#include <sstream>       // String manipulation
#include <string>        // Text string handling
#include <random>        // Random number generation
#include <vector>        // Dynamic arrays
#include <algorithm>     // Useful algorithms like min/max
#include "game.h"        // Our game logic (camera, matching, UI)

// ============================================================================
// Utilities - Helper functions that do common tasks
// ============================================================================

std::string readFile(const std::string& path) { // Reads entire file from disk as string
    std::ifstream f(path, std::ios::binary | std::ios::ate); // Open in binary mode, at end for size
    if (!f) { std::fprintf(stderr, "Failed to open: %s\n", path.c_str()); return {}; }
    const auto size = f.tellg(); // Get file size (we're at end)
    f.seekg(0); // Go back to beginning
    std::string content(static_cast<size_t>(size), '\0'); // Make string big enough
    f.read(content.data(), size); // Read entire file
    return content;
}

GLuint compileShader(GLenum type, const char* src) { // Compiles a shader (program that runs on GPU)
    GLuint id = glCreateShader(type); // Create shader of specified type
    glShaderSource(id, 1, &src, nullptr); // Give OpenGL the source code
    glCompileShader(id); // Compile the shader
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok); // Check if compilation worked
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(id, sizeof(log), nullptr, log); // Get error message
        std::fprintf(stderr, "Shader compile error:\n%s\n", log);
        glDeleteShader(id);
        return 0;
    }
    return id;
}

GLuint createProgram(const char* vsSrc, const char* fsSrc) { // Creates complete shader program
    GLuint v = compileShader(GL_VERTEX_SHADER, vsSrc);   // Compile vertex shader (processes vertices)
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fsSrc); // Compile fragment shader (pixel colors)
    if (!v || !f) { if (v) glDeleteShader(v); if (f) glDeleteShader(f); return 0; }
    GLuint prog = glCreateProgram(); // Create program to hold both shaders
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog); // Link them together
    glDeleteShader(v); glDeleteShader(f); // Can delete individual shaders now
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

bool createFboTexturePair(GLuint& fbo, GLuint& tex, int w, int h) { // Creates framebuffer with texture
    glGenFramebuffers(1, &fbo);  // Create framebuffer (off-screen canvas)
    glGenTextures(1, &tex);      // Create texture (image storage)
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr); // Allocate RGBA32F
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // No blur when small
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // No blur when large
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Clamp to edges
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0); // Attach texture
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
    glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT); // Clear to black
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Switch back to screen
    return true;
}

// ============================================================================
// App State - All the data our program needs to remember
// ============================================================================

struct App {
    GLFWwindow* window = nullptr;     // Pointer to our window
    int fbW = 0, fbH = 0;             // Framebuffer width and height in pixels
    GLuint quadVao = 0, quadVbo = 0;  // Full-screen quad vertex array and buffer
    GLuint bufAFbo[2] = { 0 }, bufATex[2] = { 0 }; // Double-buffered render targets
    int bufAIdx = 0;                  // Which buffer we're reading from
    GLuint progBufferA = 0, progImage = 0; // Compiled shader programs
    ShaderUniforms uniformsA, uniformsImage; // Shader variable locations
    bool keyW = false, keyS = false, keyA = false, keyD = false; // WASD keys
    bool mouseLeft = false;           // Left mouse button
    double mouseX = 0, mouseY = 0;    // Mouse position
    Thumbnail goalThumbnail, playerThumbnail; // Small preview images
    CameraState goalCamera;           // Where the goal camera is
    PlayerCamera playerCamera;        // Player's camera (can move)
    GoalGenerator goalGenerator;      // Generates random goals
    TextRenderer textRenderer;        // Draws text on screen
    OverlayRenderer overlayRenderer;  // Draws colored rectangles
    bool spacePressed = false;        // True when space pressed this frame
    bool goalCaptured = false;        // Have we captured goal image yet?
    bool showingSuccess = false;      // Are we showing "SUCCESS!" message?
    double successStartTime = 0.0;    // When did success start showing?
    MatchResult lastMatch, liveMatch; // Match percentages
    static constexpr float SUCCESS_THRESHOLD = 75.0f;   // Need 75% match to win
    static constexpr float SUCCESS_DISPLAY_TIME = 3.0f; // Show success for 3 seconds
    static constexpr float THUMBNAIL_SIZE = 0.35f;      // Thumbnails are 35% of screen
    static constexpr float MARGIN = 0.02f;              // 2% margin from edge
    static constexpr float MIN_OBJECT_COVERAGE = 0.10f; // Need 10% 3D objects visible
    std::mt19937 rng; // Random number generator

    void recreateFbos() { // Recreates framebuffers when window resized
        for (int i = 0; i < 2; ++i) {
            if (bufATex[i]) glDeleteTextures(1, &bufATex[i]);
            if (bufAFbo[i]) glDeleteFramebuffers(1, &bufAFbo[i]);
            bufATex[i] = bufAFbo[i] = 0;
            createFboTexturePair(bufAFbo[i], bufATex[i], fbW, fbH);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void resetForNewGoal() { // Starts new round with new goal
        goalCaptured = false;
        goalGenerator.generateGoal(goalCamera, rng);
        playerThumbnail.hasContent = false;
        playerCamera.reset();
    }
};

App* gApp = nullptr; // Global pointer so callbacks can access our app

// ============================================================================
// Callbacks - Functions GLFW calls when things happen
// ============================================================================

void framebuffer_size_cb(GLFWwindow*, int, int) { // Called when window resized
    if (!gApp) return;
    glfwGetFramebufferSize(gApp->window, &gApp->fbW, &gApp->fbH);
    if (gApp->fbW <= 0 || gApp->fbH <= 0) return;
    glViewport(0, 0, gApp->fbW, gApp->fbH);
    gApp->recreateFbos();
}

void key_cb(GLFWwindow*, int key, int, int action, int) { // Called when key pressed/released
    if (!gApp) return;
    const bool down = (action == GLFW_PRESS || action == GLFW_REPEAT);
    switch (key) {
    case GLFW_KEY_W: gApp->keyW = down; break; // W for forward
    case GLFW_KEY_S: gApp->keyS = down; break; // S for backward
    case GLFW_KEY_A: gApp->keyA = down; break; // A for left
    case GLFW_KEY_D: gApp->keyD = down; break; // D for right
    case GLFW_KEY_SPACE: // Space to capture
        if (action == GLFW_PRESS && !gApp->spacePressed) gApp->spacePressed = true;
        else if (action == GLFW_RELEASE) gApp->spacePressed = false;
        break;
    case GLFW_KEY_R: // R to restart with new goal
        if (action == GLFW_PRESS) gApp->resetForNewGoal();
        break;
    case GLFW_KEY_ESCAPE: // Escape to clear player thumbnail
        if (action == GLFW_PRESS) { gApp->playerThumbnail.hasContent = false; gApp->showingSuccess = false; }
        break;
    }
}

void mouse_btn_cb(GLFWwindow*, int button, int action, int) { // Called when mouse clicked
    if (gApp && button == GLFW_MOUSE_BUTTON_LEFT)
        gApp->mouseLeft = (action == GLFW_PRESS || action == GLFW_REPEAT);
}

// ============================================================================
// Goal Validation - Makes sure goal has enough visible 3D objects
// ============================================================================

float checkObjectCoverage(App& app, float time, float resX, float resY) { // Returns % covered by 3D objects
    glBindVertexArray(app.quadVao);                                   // Use our rectangle shape (covers whole screen)
    int readIdx = app.bufAIdx, writeIdx = 1 - readIdx;                // We have 2 canvases - pick which to read/write
    glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[writeIdx]);         // Draw to off-screen canvas B
    glViewport(0, 0, app.fbW, app.fbH);                               // Set drawing area to full size
    glUseProgram(app.progBufferA);                                    // Use "Buffer A" drawing program
    glActiveTexture(GL_TEXTURE0);                                     // Pick texture slot 0
    glBindTexture(GL_TEXTURE_2D, app.bufATex[readIdx]);               // Look at canvas A (last frame's picture)
    app.uniformsA.setCommon(time, resX, resY, 0, 0, 0);               // Tell shader: current time, screen size, no mouse
    app.uniformsA.setKeys(false, false, false, false);                // Tell shader: no WASD keys pressed
    app.uniformsA.setCameraOverride(true, app.goalCamera);            // Tell shader: use goal camera, not player
    glDrawArrays(GL_TRIANGLES, 0, 6);                                 // Draw! (6 points = 2 triangles = 1 rectangle)
    app.bufAIdx = writeIdx;                                           // Swap canvases (A becomes B, B becomes A)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);                             // Now draw to actual screen (not off-screen)
    glViewport(0, 0, app.fbW, app.fbH);                               // Set drawing area again
    glUseProgram(app.progImage);                                      // Use "Image" drawing program (makes pretty colors)
    glActiveTexture(GL_TEXTURE0);                                     // Pick texture slot 0
    glBindTexture(GL_TEXTURE_2D, app.bufATex[app.bufAIdx]);           // Look at canvas we just drew
    app.uniformsImage.setCommon(time, resX, resY, 0, 0, 0);           // Tell shader: time, size, no mouse
    app.uniformsImage.setCameraOverride(true, app.goalCamera);        // Tell shader: use goal camera
    app.uniformsImage.setDetectionMode(true);                         // Special mode: color objects by type (for counting)
    glDrawArrays(GL_TRIANGLES, 0, 6);                                 // Draw final picture to screen
    glBindVertexArray(0);                                             // Done with rectangle shape
    // Read back pixels from GPU
    std::vector<unsigned char> pixels(app.fbW * app.fbH * 3);
    glReadPixels(0, 0, app.fbW, app.fbH, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    int objectPixels = 0, totalChecked = 0;
    int marginX = app.fbW / 5, marginY = app.fbH / 5; // Only check center 60%
    for (int y = marginY; y < app.fbH - marginY; y += 4) { // Loop every 4th pixel for speed
        for (int x = marginX; x < app.fbW - marginX; x += 4) {
            int idx = (y * app.fbW + x) * 3; // 3 bytes per pixel (RGB)
            unsigned char r = pixels[idx]; // Red channel has object ID in detection mode
            if (r < 64) objectPixels++; // Dark = 3D object (ID 0), gray = ground (ID 1), white = sky (ID 2)
            totalChecked++;
        }
    }
    return static_cast<float>(objectPixels) / static_cast<float>(totalChecked);
}

void renderGoalFrame(App& app, float time, float resX, float resY) { // Renders from goal camera (normal mode)
    glBindVertexArray(app.quadVao);                                   // Use full-screen rectangle mesh
    int readIdx = app.bufAIdx, writeIdx = 1 - readIdx;                // Ping-pong: read from one buffer, write to other
    glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[writeIdx]);         // Draw to off-screen buffer
    glViewport(0, 0, app.fbW, app.fbH);                               // Set drawing area to full window
    glUseProgram(app.progBufferA);                                    // Use Buffer A shader (processes 3D scene)
    glActiveTexture(GL_TEXTURE0);                                     // Activate texture slot 0
    glBindTexture(GL_TEXTURE_2D, app.bufATex[readIdx]);               // Read previous frame's data
    app.uniformsA.setCommon(time, resX, resY, 0, 0, 0);               // Pass time, resolution, no mouse input
    app.uniformsA.setKeys(false, false, false, false);                // No WASD movement
    app.uniformsA.setCameraOverride(true, app.goalCamera);            // Use goal camera position
    glDrawArrays(GL_TRIANGLES, 0, 6);                                 // Render (2 triangles = full-screen quad)
    app.bufAIdx = writeIdx;                                           // Swap read/write buffers for next frame
    glBindFramebuffer(GL_FRAMEBUFFER, 0);                             // Now draw to actual screen
    glViewport(0, 0, app.fbW, app.fbH);                               // Set drawing area again
    glUseProgram(app.progImage);                                      // Use Image shader (final colors/lighting)
    glActiveTexture(GL_TEXTURE0);                                     // Activate texture slot 0
    glBindTexture(GL_TEXTURE_2D, app.bufATex[app.bufAIdx]);           // Read buffer we just wrote
    app.uniformsImage.setCommon(time, resX, resY, 0, 0, 0);           // Pass time, resolution, no mouse
    app.uniformsImage.setCameraOverride(true, app.goalCamera);        // Use goal camera position
    app.uniformsImage.setDetectionMode(false);                        // Normal rendering (not object ID colors)
    glDrawArrays(GL_TRIANGLES, 0, 6);                                 // Render final image to screen
    glBindVertexArray(0);                                             // Unbind mesh (cleanup)
}

// ============================================================================
// Main - Program starts here
// ============================================================================

int main() {
    std::string shaderDir = SHADER_DIR; // Path to shader files
    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return EXIT_FAILURE; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Request OpenGL 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#endif
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Camera Hunt (SPACE=check, R=new)", nullptr, nullptr);
    if (!window) { glfwTerminate(); return EXIT_FAILURE; }
    glfwMakeContextCurrent(window); // Make OpenGL context current
    glfwSwapInterval(1); // Enable vsync
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) { // Load OpenGL functions
        glfwDestroyWindow(window); glfwTerminate(); return EXIT_FAILURE;
    }
    App app;
    app.window = window;
    gApp = &app; // Set global pointer for callbacks
    std::random_device rd; app.rng.seed(rd()); // Seed random generator
    glfwGetFramebufferSize(window, &app.fbW, &app.fbH); // Get initial window size
    glfwSetFramebufferSizeCallback(window, framebuffer_size_cb); // Register callbacks
    glfwSetKeyCallback(window, key_cb);
    glfwSetMouseButtonCallback(window, mouse_btn_cb);
    // Create full-screen quad (two triangles covering -1 to +1)
    constexpr float quadVerts[] = { -1,-1, 1,-1, -1,1, -1,1, 1,-1, 1,1 };
    glGenVertexArrays(1, &app.quadVao);
    glGenBuffers(1, &app.quadVbo);
    glBindVertexArray(app.quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, app.quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW); // Upload to GPU
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr); // 2 floats per vertex
    glBindVertexArray(0);
    // Initialize UI renderers
    app.goalThumbnail.initShader(thumbnailVS, thumbnailFS, createProgram);
    app.playerThumbnail.initShader(thumbnailVS, thumbnailFS, createProgram);
    app.textRenderer.init(textVS, textFS, createProgram);
    app.overlayRenderer.init(thumbnailVS, overlayFS, createProgram);
    // Load shader source code from files
    std::string vsSrc = readFile(shaderDir + "fullscreen.vert");
    std::string commonSrc = readFile(shaderDir + "common.glsl");
    std::string bufAFs = readFile(shaderDir + "buffer_a.frag");
    std::string imgFs = readFile(shaderDir + "image.frag");
    if (vsSrc.empty() || commonSrc.empty() || bufAFs.empty() || imgFs.empty()) {
        std::fprintf(stderr, "Failed to load shader files\n");
        glfwDestroyWindow(window); glfwTerminate(); return EXIT_FAILURE;
    }
    // Compile shaders
    app.progBufferA = createProgram(vsSrc.c_str(), bufAFs.c_str());
    app.progImage = createProgram(vsSrc.c_str(), (commonSrc + imgFs).c_str());
    if (!app.progBufferA || !app.progImage) { glfwDestroyWindow(window); glfwTerminate(); return EXIT_FAILURE; }
    app.uniformsA.init(app.progBufferA); // Find uniform locations
    app.uniformsImage.init(app.progImage);
    glUseProgram(app.progBufferA);
    if (app.uniformsA.iChannel0 >= 0) glUniform1i(app.uniformsA.iChannel0, 0); // Texture slot 0
    glUseProgram(app.progImage);
    if (app.uniformsImage.iChannel0 >= 0) glUniform1i(app.uniformsImage.iChannel0, 0);
    glUseProgram(0);
    app.recreateFbos(); // Create framebuffers at initial size
    app.goalGenerator.generateGoal(app.goalCamera, app.rng); // Generate first goal
    float aspectRatio = static_cast<float>(app.fbW) / app.fbH;
    app.goalThumbnail.setTargetRect(aspectRatio, App::THUMBNAIL_SIZE, App::MARGIN, true);   // Top-right
    app.playerThumbnail.setTargetRect(aspectRatio, App::THUMBNAIL_SIZE, App::MARGIN, false); // Top-left
    bool shouldCapture = false; // Flag for when to capture player's view
    double fpsWindowStart = glfwGetTime();
    int fpsFrameCount = 0;

    // === MAIN LOOP - Runs every frame until window closed ===
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); // Process window events
        glfwGetCursorPos(window, &app.mouseX, &app.mouseY); // Get mouse position
        const double currentTime = glfwGetTime(); // Time since program started
        if (app.spacePressed) { shouldCapture = true; app.spacePressed = false; }
        const float time = static_cast<float>(currentTime);
        const float resX = static_cast<float>(app.fbW), resY = static_cast<float>(app.fbH);
        const float mx = static_cast<float>(app.mouseX);
        const float my = static_cast<float>(app.fbH - app.mouseY); // Flip Y (OpenGL is bottom-up)
        const float mLeft = app.mouseLeft ? 1.f : 0.f;
        float newAspect = resX / resY; // Update aspect ratio if window resized
        if (std::abs(newAspect - aspectRatio) > 0.001f) {
            aspectRatio = newAspect;
            app.goalThumbnail.setTargetRect(aspectRatio, App::THUMBNAIL_SIZE, App::MARGIN, true);
            app.playerThumbnail.setTargetRect(aspectRatio, App::THUMBNAIL_SIZE, App::MARGIN, false);
        }
        app.playerCamera.update(resX, resY, mx, my, app.mouseLeft, app.keyW, app.keyS, app.keyA, app.keyD);

        // Goal capture (only runs once per goal)
        if (!app.goalCaptured) {
            bool validGoal = false;
            int attempts = 0;
            const int maxAttempts = 100;
            while (!validGoal && attempts < maxAttempts) { // Keep trying until enough 3D objects visible
                attempts++;
                for (int frame = 0; frame < 3; frame++) renderGoalFrame(app, time, resX, resY); // Warm up
                float coverage = checkObjectCoverage(app, time, resX, resY);
                if (coverage >= App::MIN_OBJECT_COVERAGE) {
                    validGoal = true;
                    std::printf("Goal accepted: %.1f%% 3D objects (attempt %d)\n", coverage * 100.0f, attempts);
                }
                else {
                    std::printf("Goal rejected: %.1f%% 3D objects (need %.0f%%), retrying...\n",
                        coverage * 100.0f, App::MIN_OBJECT_COVERAGE * 100.0f);
                    app.goalGenerator.generateGoal(app.goalCamera, app.rng); // Try new position
                    for (int i = 0; i < 2; i++) { // Clear buffers for fresh start
                        glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[i]);
                        glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
                    }
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                }
            }
            if (!validGoal) std::printf("Warning: Using best attempt after %d tries\n", maxAttempts);
            for (int frame = 0; frame < 5; frame++) renderGoalFrame(app, time, resX, resY); // Stabilize
            app.goalThumbnail.capture(app.fbW, app.fbH); // Capture goal thumbnail
            app.goalCaptured = true;
            for (int i = 0; i < 2; i++) { // Clear buffers so player starts fresh
                glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[i]);
                glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            app.playerCamera.reset(); // Reset player to starting position
            std::printf("Goal: pos(%.2f, %.2f, %.2f)\n", app.goalCamera.posX, app.goalCamera.posY, app.goalCamera.posZ);
        }

        // Player render
        glBindVertexArray(app.quadVao);
        { // Buffer A pass (processes scene data)
            int readIdx = app.bufAIdx, writeIdx = 1 - readIdx;
            glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[writeIdx]); // Render to off-screen buffer
            glViewport(0, 0, app.fbW, app.fbH);
            glUseProgram(app.progBufferA);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app.bufATex[readIdx]); // Previous frame as input
            app.uniformsA.setCommon(time, resX, resY, mx, my, mLeft);
            app.uniformsA.setKeys(app.keyW, app.keyS, app.keyA, app.keyD);
            app.uniformsA.setCameraOverride(false, app.goalCamera); // Use player camera
            glDrawArrays(GL_TRIANGLES, 0, 6);
            app.bufAIdx = writeIdx; // Swap buffers
        }
        { // Image pass (final render to screen)
            glBindFramebuffer(GL_FRAMEBUFFER, 0); // Render to screen
            glViewport(0, 0, app.fbW, app.fbH);
            glUseProgram(app.progImage);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app.bufATex[app.bufAIdx]); // Use buffer we just created
            app.uniformsImage.setCommon(time, resX, resY, mx, my, mLeft);
            app.uniformsImage.setCameraOverride(false, app.goalCamera);
            app.uniformsImage.setDetectionMode(false); // Normal rendering
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glBindVertexArray(0);

        // Match calculation - see how close player is to goal
        app.liveMatch = calculateMatch(app.goalCamera, app.playerCamera);
        if (shouldCapture) { // Player pressed space
            app.playerThumbnail.capture(app.fbW, app.fbH); // Save current screen
            app.playerThumbnail.startAnimation(currentTime); // Start appear animation
            app.lastMatch = app.liveMatch; // Save match result
            std::printf("CAPTURE: %.0f%%\n", app.lastMatch.total);
            if (app.lastMatch.total >= App::SUCCESS_THRESHOLD) { // Good enough match
                app.showingSuccess = true;
                app.successStartTime = currentTime;
            }
            shouldCapture = false;
        }

        // UI rendering
        if (app.goalThumbnail.hasContent) { // Goal thumbnail (top-right)
            app.goalThumbnail.render(app.quadVao, currentTime, false, 1.0f, 0.8f, 0.0f, true); // Yellow border
            app.textRenderer.draw(app.quadVao, "GOAL",
                app.goalThumbnail.targetX + app.goalThumbnail.targetW * 0.3f,
                app.goalThumbnail.targetY + app.goalThumbnail.targetH * 0.85f,
                app.goalThumbnail.targetW * 0.4f, app.goalThumbnail.targetH * 0.1f, 1, 1, 1, 1); // White text
        }
        if (app.playerThumbnail.hasContent) { // Player thumbnail (top-left)
            float g = app.lastMatch.total / 100.0f; // Green based on match quality
            app.playerThumbnail.render(app.quadVao, currentTime, true, 1.0f - g, g, 0.2f, true); // Red-green border
        }
        { // HUD (score display at bottom-left)
            float hudX = -0.98f, hudY = -0.98f, hudW = 0.6f, hudH = 0.28f;
            app.overlayRenderer.draw(app.quadVao, hudX, hudY, hudW, hudH, 0, 0, 0, 0.6f); // Semi-transparent black
            char buf[64];
            float textH = 0.05f, textW = 0.55f, textX = hudX + 0.02f;
            float c = app.liveMatch.total / 100.0f; // Color based on quality (red text  to green text)
            std::snprintf(buf, sizeof(buf), "TOTAL: %d%%", (int)app.liveMatch.total);
            app.textRenderer.draw(app.quadVao, buf, textX, hudY + 0.20f, textW, textH, 1 - c, c, 0.3f, 1);
            c = app.liveMatch.position / 100.0f;
            std::snprintf(buf, sizeof(buf), "POS: %d%%", (int)app.liveMatch.position);
            app.textRenderer.draw(app.quadVao, buf, textX, hudY + 0.12f, textW, textH, 1 - c, c, 0.3f, 1);
            c = app.liveMatch.direction / 100.0f;
            std::snprintf(buf, sizeof(buf), "DIR: %d%%", (int)app.liveMatch.direction);
            app.textRenderer.draw(app.quadVao, buf, textX, hudY + 0.04f, textW, textH, 1 - c, c, 0.3f, 1);
        }
        if (app.showingSuccess) { // Success overlay (appears when match > 75%)
            float elapsed = static_cast<float>(currentTime - app.successStartTime);
            if (elapsed > App::SUCCESS_DISPLAY_TIME) { // After 3 seconds, start new goal
                app.showingSuccess = false;
                app.resetForNewGoal();
            }
            else {
                float alpha = 0.7f; // Fade in and out
                if (elapsed < 0.3f) alpha *= elapsed / 0.3f; // Fade in
                else if (elapsed > App::SUCCESS_DISPLAY_TIME - 0.5f) alpha *= (App::SUCCESS_DISPLAY_TIME - elapsed) / 0.5f; // Fade out
                app.overlayRenderer.draw(app.quadVao, -0.5f, -0.15f, 1.0f, 0.3f, 0.1f, 0.5f, 0.1f, alpha); // Green bg
                app.textRenderer.draw(app.quadVao, "SUCCESS!", -0.35f, -0.05f, 0.7f, 0.15f, 1, 1, 1, 1);
                if (elapsed > 1.5f) { // Show "NEW GOAL" after 1.5 seconds
                    float a = std::min((elapsed - 1.5f) / 0.3f, 1.0f); // Fade in
                    app.textRenderer.draw(app.quadVao, "NEW GOAL", -0.35f, -0.20f, 0.7f, 0.08f, 1, 1, 0.5f, a);
                }
            }
        }
        glfwSwapBuffers(window); // Display everything we drew
        fpsFrameCount++;
        const double fpsNow = glfwGetTime();
        if (fpsNow - fpsWindowStart >= 0.5) {
            const double fps = static_cast<double>(fpsFrameCount) / (fpsNow - fpsWindowStart);
            fpsFrameCount = 0;
            fpsWindowStart = fpsNow;
            char title[192];
            std::snprintf(title, sizeof(title), "Camera Hunt — %.0f FPS (SPACE=check, R=new)", fps);
            glfwSetWindowTitle(window, title);
        }
    }

    // Cleanup - free all resources before exiting
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
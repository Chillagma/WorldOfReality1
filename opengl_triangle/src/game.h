#pragma once
#define NOMINMAX

#include <glad/glad.h>
#include <random>
#include <cmath>
#include <algorithm>
#include <cstring>

constexpr float PI = 3.14159265359f;

// ============================================================================
// Camera & Matching
// ============================================================================

// The "goal" camera position the player needs to match
struct CameraState {
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;  // where camera is
    float dirX = 0.0f, dirY = 0.0f, dirZ = 1.0f;  // where camera looks
    float pitch = 0.0f, yaw = 0.0f;                // up/down and left/right angles

    // convert pitch/yaw angles into a direction vector
    void setFromAngles(float p, float y) {
        pitch = p; yaw = y;
        dirX = std::cos(pitch) * std::sin(yaw);   // trig math to get direction
        dirY = std::sin(pitch);
        dirZ = std::cos(pitch) * std::cos(yaw);
        float len = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);  // normalize
        if (len > 0.0001f) { dirX /= len; dirY /= len; dirZ /= len; }
    }

    // pick a random position and direction
    void randomize(std::mt19937& rng) {
        std::uniform_real_distribution<float> posDist(-8.0f, 8.0f);
        std::uniform_real_distribution<float> yawDist(-PI, PI);
        std::uniform_real_distribution<float> pitchDist(-0.5f, 0.5f);
        posX = posDist(rng);
        posY = posDist(rng) * 0.2f;   // Y is smaller so camera stays near ground
        posZ = posDist(rng);
        setFromAngles(pitchDist(rng), yawDist(rng));
    }
};

// the camera the player controls with mouse/keyboard
struct PlayerCamera {
    float mouseX = 0.0f, mouseY = 0.0f, mouseDown = 0.0f;
    float dirX = 0.0f, dirY = 0.0f, dirZ = 1.0f;
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;

    // put camera back at start
    void reset() {
        mouseX = mouseY = mouseDown = 0.0f;
        dirX = 0.0f; dirY = 0.0f; dirZ = 1.0f;
        posX = posY = posZ = 0.0f;
    }

    // called every frame to handle input
    void update(float resX, float resY, float curMX, float curMY, bool mouseDownNow,
        bool keyW, bool keyS, bool keyA, bool keyD) {
        // if dragging mouse, rotate camera
        if (mouseDownNow && mouseDown > 0.0f) {
            float dx = (curMX - mouseX) * 8.0f / resY;  // how much mouse moved
            float dy = (curMY - mouseY) * 8.0f / resY;

            float pitch = std::asin(std::clamp(dirY, -1.0f, 1.0f));
            pitch = std::clamp(pitch - dy, -1.56f, 1.56f);  // limit so cant flip upside down

            float yawDelta = -dx;
            float cosY = std::cos(yawDelta), sinY = std::sin(yawDelta);
            float newX = dirX * cosY - dirZ * sinY;   // rotation math
            float newZ = dirX * sinY + dirZ * cosY;

            float l = std::cos(pitch);
            float lenXZ = std::sqrt(newX * newX + newZ * newZ);
            if (lenXZ > 0.0001f) {
                dirX = (newX / lenXZ) * l;
                dirZ = (newZ / lenXZ) * l;
            }
            else {
                dirX = 0.0f; dirZ = l;
            }
            dirY = std::sin(pitch);
        }

        // remember mouse pos for next frame
        mouseX = curMX; mouseY = curMY;
        mouseDown = mouseDownNow ? 1.0f : 0.0f;

        // WASD movement
        float speed = 0.4f;
        float rightX = -dirZ, rightZ = dirX;  // "right" direction for strafing
        float rightLen = std::sqrt(rightX * rightX + rightZ * rightZ);
        if (rightLen > 0.0001f) { rightX /= rightLen; rightZ /= rightLen; }

        posX += (keyW - keyS) * speed * dirX;  // W/S = forward/back
        posY += (keyW - keyS) * speed * dirY;
        posZ += (keyW - keyS) * speed * dirZ;
        posX += (keyD - keyA) * speed * rightX * -1.0f;  // A/D = strafe
        posZ += (keyD - keyA) * speed * rightZ * -1.0f;
    }
};

// how well player matched the goal
struct MatchResult {
    float position = 0.0f;   // position score 0-100
    float direction = 0.0f;  // direction score 0-100
    float total = 0.0f;      // combined score
    float posDist = 0.0f;    // actual distance in units
    float dirDist = 0.0f;    // actual angle difference
};

// converts distance to score (closer = higher, max 100)
inline float smoothMatch(float distance, float halfDist) {
    return 100.0f / (1.0f + (distance * distance) / (halfDist * halfDist));
}

// compare player camera to goal camera
inline MatchResult calculateMatch(const CameraState& goal, const PlayerCamera& player) {
    MatchResult m;
    // position distance (3D pythagorean theorem)
    m.posDist = std::sqrt(std::pow(goal.posX - player.posX, 2.0f) +
        std::pow(goal.posY - player.posY, 2.0f) +
        std::pow(goal.posZ - player.posZ, 2.0f));
    m.position = smoothMatch(m.posDist, 10.0f);

    // direction difference (dot product -> angle)
    float dot = goal.dirX * player.dirX + goal.dirY * player.dirY + goal.dirZ * player.dirZ;
    m.dirDist = std::acos(std::clamp(dot, -1.0f, 1.0f));
    m.direction = smoothMatch(m.dirDist, 0.5f);

    m.total = m.position * 0.5f + m.direction * 0.5f;  // average both scores
    return m;
}

// ============================================================================
// Goal Generation - MODIFY THIS FOR DIFFERENT GOAL LOGIC
// ============================================================================

// makes sure goals are looking at something interesting
struct GoalGenerator {
    // Same ring as common.glsl: 5 houses around spawn (0,16,-64) at radius 40
    struct Vec3 { float x, y, z; };
    std::vector<Vec3> objects = [] {
        std::vector<Vec3> v;
        constexpr float ax = 0.f, ay = 16.f, az = -64.f;
        constexpr float r = 40.f;
        constexpr int n = 5;
        for (int i = 0; i < n; ++i) {
            float a = static_cast<float>(i) * 6.2831853f / static_cast<float>(n);
            v.push_back({ ax + std::cos(a) * r, ay, az + std::sin(a) * r });
        }
        return v;
    }();

    // check if camera can see any objects
    bool isValidGoalPosition(const CameraState& cam) {
        for (auto& obj : objects) {
            // vector from camera to object
            float toObjX = obj.x - cam.posX;
            float toObjY = obj.y - cam.posY;
            float toObjZ = obj.z - cam.posZ;

            // distance to object
            float dist = std::sqrt(toObjX * toObjX + toObjY * toObjY + toObjZ * toObjZ);
            if (dist < 0.1f) continue; // too close, skip

            // normalize
            toObjX /= dist; toObjY /= dist; toObjZ /= dist;

            // dot product = 1 means looking directly at it, 0 = perpendicular
            float dot = toObjX * cam.dirX + toObjY * cam.dirY + toObjZ * cam.dirZ;

            // if looking toward object (dot > 0.7) and close enough
            if (dot > 0.7f && dist < 55.0f) {
                return true;  // good goal!
            }
        }
        return false;  // cant see anything, bad goal
    }

    // keep trying random positions until one works
    void generateGoal(CameraState& goal, std::mt19937& rng, int maxAttempts = 100) {
        for (int i = 0; i < maxAttempts; i++) {
            goal.randomize(rng);
            if (isValidGoalPosition(goal)) return;
        }
    }
};

// ============================================================================
// UI Rendering
// ============================================================================

// stores locations of shader variables (uniforms)
struct ShaderUniforms {
    // These are "addresses" to variables inside the GPU shader
    // -1 means "doesn't exist" or "shader doesn't use this"
    GLint iChannel0 = -1, iTime = -1, iResolution = -1, iMouse = -1;  // basic stuff
    GLint uKeyW = -1, uKeyS = -1, uKeyA = -1, uKeyD = -1;              // keyboard keys
    GLint uCameraOverride = -1, uCameraPosOverride = -1, uCameraRotOverride = -1;  // camera stuff
    GLint uDetectionMode = -1;  // highlight mode

    // Find where each variable lives in the shader (like looking up phone numbers)
    void init(GLuint program) {
        iChannel0 = glGetUniformLocation(program, "iChannel0");      // texture input
        iTime = glGetUniformLocation(program, "iTime");              // clock/stopwatch
        iResolution = glGetUniformLocation(program, "iResolution");  // screen size
        iMouse = glGetUniformLocation(program, "iMouse");            // mouse info

        uKeyW = glGetUniformLocation(program, "uKeyW");  // W key
        uKeyS = glGetUniformLocation(program, "uKeyS");  // S key
        uKeyA = glGetUniformLocation(program, "uKeyA");  // A key
        uKeyD = glGetUniformLocation(program, "uKeyD");  // D key

        uCameraOverride = glGetUniformLocation(program, "uCameraOverride");      // switch: use player camera or goal camera?
        uCameraPosOverride = glGetUniformLocation(program, "uCameraPosOverride"); // goal camera position
        uCameraRotOverride = glGetUniformLocation(program, "uCameraRotOverride"); // goal camera direction

        uDetectionMode = glGetUniformLocation(program, "uDetectionMode");  // "highlight objects?" flag
    }

    // Tell shader: current time, screen size, and mouse position
    void setCommon(float time, float resX, float resY, float mx, float my, float mLeft) {
        if (iTime >= 0) glUniform1f(iTime, time);                      // how many seconds passed
        if (iResolution >= 0) glUniform2f(iResolution, resX, resY);    // window width & height
        if (iMouse >= 0) glUniform4f(iMouse, mx, my, mLeft, 0.f);      // mouse x, y, left button
    }

    // Tell shader: which WASD keys are pressed (1.0 = yes, 0.0 = no)
    void setKeys(bool w, bool s, bool a, bool d) {
        if (uKeyW >= 0) glUniform1f(uKeyW, w ? 1.f : 0.f);  // forward
        if (uKeyS >= 0) glUniform1f(uKeyS, s ? 1.f : 0.f);  // backward
        if (uKeyA >= 0) glUniform1f(uKeyA, a ? 1.f : 0.f);  // left
        if (uKeyD >= 0) glUniform1f(uKeyD, d ? 1.f : 0.f);  // right
    }

    // Tell shader: render from the GOAL camera instead of the PLAYER camera
    // (Used to show "what the goal looks like" in the thumbnail)
    void setCameraOverride(bool enable, const CameraState& cam) {
        // enable=true means: "render from goal camera", false means: "render from player camera"
        if (uCameraOverride >= 0) glUniform1i(uCameraOverride, enable ? 1 : 0);

        if (enable) {
            // Send the goal camera's position (where it is in 3D space)
            if (uCameraPosOverride >= 0) glUniform3f(uCameraPosOverride, cam.posX, cam.posY, cam.posZ);
            // Send the goal camera's rotation (which direction it's looking)
            if (uCameraRotOverride >= 0) glUniform2f(uCameraRotOverride, cam.pitch, cam.yaw);
        }
    }

    // Tell shader: "make important objects glow" on/off
    void setDetectionMode(bool enable) {
        if (uDetectionMode >= 0) glUniform1i(uDetectionMode, enable ? 1 : 0);
    }
};

// small preview image (picture-in-picture)
struct Thumbnail {
    GLuint texture = 0, program = 0;
    GLint uRect = -1, uTexture = -1, uAlpha = -1, uBorderSize = -1, uBorderColor = -1, uShowLabel = -1, uLabelHeight = -1;
    int texWidth = 0, texHeight = 0;
    bool hasContent = false, animating = false;
    double animStartTime = 0.0;
    float animDuration = 0.6f;
    float targetX = 0.0f, targetY = 0.0f, targetW = 0.0f, targetH = 0.0f;

    // load thumbnail shader
    void initShader(const char* vs, const char* fs, GLuint(*createProg)(const char*, const char*)) {
        // 1. Create and link shader program from vertex and fragment shaders
        program = createProg(vs, fs);

        // 2. Only look for uniform locations if shaders compiled successfully
        if (program) {
            // Find where in the GPU program we can access each uniform variable
            uRect = glGetUniformLocation(program, "uRect");          // Position/size of thumbnail
            uTexture = glGetUniformLocation(program, "uTexture");    // Texture to display
            uAlpha = glGetUniformLocation(program, "uAlpha");        // Transparency
            uBorderSize = glGetUniformLocation(program, "uBorderSize"); // Border thickness
            uBorderColor = glGetUniformLocation(program, "uBorderColor"); // Border color        uShowLabel = glGetUniformLocation(program, "uShowLabel");     // Show text label
            uLabelHeight = glGetUniformLocation(program, "uLabelHeight"); // Text size

            // Store these "handles" so we can set these values later
            // (without knowing where they are in the shader)
        }
    }

    // make empty texture to store screenshot
    void createTexture(int w, int h) {
        // if we don't have a texture ID yet, ask OpenGL for one
        if (!texture) glGenTextures(1, &texture);

        // remember the size
        texWidth = w; texHeight = h;

        // tell OpenGL "we're working with this texture now"
        glBindTexture(GL_TEXTURE_2D, texture);

        // create empty texture memory on GPU (w x h pixels, RGB format, no data yet)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

        // when zooming out, blend between pixels smoothly (not blocky)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        // when zooming in, blend between pixels smoothly (not blocky)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // if UV goes outside 0-1 range horizontally, clamp to edge (don't wrap/repeat)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

        // if UV goes outside 0-1 range vertically, clamp to edge (don't wrap/repeat)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // done configuring, unbind texture
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // copy whats on screen into texture
    void capture(int w, int h) {
        // if texture doesn't exist yet OR size changed, make a new one
        if (!texture || texWidth != w || texHeight != h) createTexture(w, h);

        // tell OpenGL "we're working with this texture now"
        glBindTexture(GL_TEXTURE_2D, texture);

        // copy what's currently on screen into the texture
        // (from screen position 0,0 to texture position 0,0, copy w x h pixels)
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);

        // done, unbind texture
        glBindTexture(GL_TEXTURE_2D, 0);

        // remember that we now have a screenshot stored
        hasContent = true;
    }

    // start shrink animation
    void startAnimation(double t) { animating = true; animStartTime = t; }

    // bouncy animation curve
    float easeOutBack(float t) {
        const float c1 = 1.70158f, c3 = c1 + 1.0f;
        return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
    }

    // draw the thumbnail
    void render(GLuint quadVao, double currentTime, bool animate, float borderR, float borderG, float borderB, bool showLabel) {
        if (!hasContent || !program || !texture) return;
        float rectX, rectY, rectW, rectH, alpha = 1.0f;
        if (animate && animating) {
            // animate from fullscreen to corner
            float elapsed = static_cast<float>(currentTime - animStartTime);
            float t = std::min(elapsed / animDuration, 1.0f);
            if (t >= 1.0f) animating = false;
            float easedT = easeOutBack(t);
            rectX = -1.0f + (targetX - (-1.0f)) * easedT;
            rectY = -1.0f + (targetY - (-1.0f)) * easedT;
            rectW = 2.0f + (targetW - 2.0f) * easedT;
            rectH = 2.0f + (targetH - 2.0f) * easedT;
        }
        else {
            rectX = targetX; rectY = targetY; rectW = targetW; rectH = targetH;
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(program);
        if (uRect >= 0) glUniform4f(uRect, rectX, rectY, rectW, rectH);
        if (uAlpha >= 0) glUniform1f(uAlpha, alpha);
        if (uBorderSize >= 0) glUniform1f(uBorderSize, 0.03f);
        if (uBorderColor >= 0) glUniform3f(uBorderColor, borderR, borderG, borderB);
        if (uShowLabel >= 0) glUniform1i(uShowLabel, showLabel ? 1 : 0);
        if (uLabelHeight >= 0) glUniform1f(uLabelHeight, 0.08f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        if (uTexture >= 0) glUniform1i(uTexture, 0);
        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
    }

    // free gpu memory
    void cleanup() {
        if (texture) { glDeleteTextures(1, &texture); texture = 0; }
        if (program) { glDeleteProgram(program); program = 0; }
    }

    // calculate corner position based on screen size
    void setTargetRect(float aspectRatio, float size, float margin, bool topRight) {
        targetW = size * 2.0f;
        targetH = targetW / aspectRatio;
        if (targetH > size * 1.5f) { targetH = size * 1.5f; targetW = targetH * aspectRatio; }
        if (topRight) { targetX = 1.0f - targetW - margin; targetY = 1.0f - targetH - margin; }
        else { targetX = 1.0f - targetW - margin; targetY = -1.0f + margin; }
    }
};

// draws pixel art text
struct TextRenderer {
    GLuint program = 0;
    GLint uRect = -1, uColor = -1, uAlpha = -1, uCharCount = -1, uChars = -1;

    void init(const char* vs, const char* fs, GLuint(*createProg)(const char*, const char*)) {
        program = createProg(vs, fs);
        if (program) {
            uRect = glGetUniformLocation(program, "uRect");
            uColor = glGetUniformLocation(program, "uColor");
            uAlpha = glGetUniformLocation(program, "uAlpha");
            uCharCount = glGetUniformLocation(program, "uCharCount");
            uChars = glGetUniformLocation(program, "uChars");
        }
    }

    // draw text string at position
    void draw(GLuint quadVao, const char* text, float x, float y, float w, float h, float r, float g, float b, float a) {
        if (!program) return;
        int len = static_cast<int>(strlen(text));
        if (len == 0 || len > 32) return;
        int chars[32] = { 0 };
        for (int i = 0; i < len; i++) chars[i] = static_cast<int>(text[i]);  // convert to ascii codes
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(program);
        if (uRect >= 0) glUniform4f(uRect, x, y, w, h);
        if (uColor >= 0) glUniform3f(uColor, r, g, b);
        if (uAlpha >= 0) glUniform1f(uAlpha, a);
        if (uCharCount >= 0) glUniform1i(uCharCount, len);
        if (uChars >= 0) glUniform1iv(uChars, 32, chars);
        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
    }

    void cleanup() { if (program) { glDeleteProgram(program); program = 0; } }
};

// draws solid color rectangles (for backgrounds, bars, etc)
struct OverlayRenderer {
    GLuint program = 0;
    GLint uRect = -1, uColor = -1, uAlpha = -1;

    void init(const char* vs, const char* fs, GLuint(*createProg)(const char*, const char*)) {
        program = createProg(vs, fs);
        if (program) {
            uRect = glGetUniformLocation(program, "uRect");
            uColor = glGetUniformLocation(program, "uColor");
            uAlpha = glGetUniformLocation(program, "uAlpha");
        }
    }

    void draw(GLuint quadVao, float x, float y, float w, float h, float r, float g, float b, float a) {
        if (!program) return;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(program);
        if (uRect >= 0) glUniform4f(uRect, x, y, w, h);
        if (uColor >= 0) glUniform3f(uColor, r, g, b);
        if (uAlpha >= 0) glUniform1f(uAlpha, a);
        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
    }

    void cleanup() { if (program) { glDeleteProgram(program); program = 0; } }
};

// ============================================================================
// Inline Shader Sources (UI only) - these run on the GPU
// ============================================================================

// vertex shader for thumbnail - positions the quad on screen
inline const char* thumbnailVS = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
uniform vec4 uRect;
void main() {
    vec2 normalPos = aPos * 0.5 + 0.5;
    vec2 finalPos = uRect.xy + normalPos * uRect.zw;
    gl_Position = vec4(finalPos, 0.0, 1.0);
    vUV = vec2(normalPos.x, normalPos.y);
}
)";

// fragment shader for thumbnail - draws texture with border
inline const char* thumbnailFS = R"(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTexture;
uniform float uAlpha;
uniform float uBorderSize;
uniform vec3 uBorderColor;
uniform bool uShowLabel;
uniform float uLabelHeight;
void main() {
    vec2 borderSize = vec2(uBorderSize);
    bool inBorder = vUV.x < borderSize.x || vUV.x > 1.0 - borderSize.x ||
                    vUV.y < borderSize.y || vUV.y > 1.0 - borderSize.y;
    bool inLabel = uShowLabel && vUV.y > (1.0 - borderSize.y - uLabelHeight) && 
                   vUV.y < (1.0 - borderSize.y * 0.3) &&
                   vUV.x > borderSize.x && vUV.x < (1.0 - borderSize.x);
    if (inBorder) {
        fragColor = vec4(uBorderColor, uAlpha);
    } else if (inLabel) {
        fragColor = vec4(0.0, 0.0, 0.0, uAlpha * 0.7);
    } else {
        vec2 innerUV = (vUV - borderSize) / (1.0 - 2.0 * borderSize);
        vec4 color = texture(uTexture, innerUV);
        fragColor = vec4(color.rgb, uAlpha);
    }
}
)";

// vertex shader for text
inline const char* textVS = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
uniform vec4 uRect;
void main() {
    vec2 normalPos = aPos * 0.5 + 0.5;
    vec2 finalPos = uRect.xy + normalPos * uRect.zw;
    gl_Position = vec4(finalPos, 0.0, 1.0);
    vUV = normalPos;
}
)";

// fragment shader for text - has bitmap font data baked in
inline const char* textFS = R"(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform vec3 uColor;
uniform float uAlpha;
uniform int uCharCount;
uniform int uChars[32];

// each letter is a 5x7 pixel bitmap encoded as hex numbers
int getPixel(int charCode, int x, int y) {
    if (charCode == 71) { int p[7] = int[7](0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; } // G
    if (charCode == 79) { int p[7] = int[7](0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; } // O
    if (charCode == 65) { int p[7] = int[7](0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11); return (p[6-y] >> (4-x)) & 1; } // A
    if (charCode == 76) { int p[7] = int[7](0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F); return (p[6-y] >> (4-x)) & 1; } // L
    if (charCode == 89) { int p[7] = int[7](0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04); return (p[6-y] >> (4-x)) & 1; } // Y
    if (charCode == 85) { int p[7] = int[7](0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; } // U
    if (charCode == 82) { int p[7] = int[7](0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11); return (p[6-y] >> (4-x)) & 1; } // R
    if (charCode == 83) { int p[7] = int[7](0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; } // S
    if (charCode == 67) { int p[7] = int[7](0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; } // C
    if (charCode == 69) { int p[7] = int[7](0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F); return (p[6-y] >> (4-x)) & 1; } // E
    if (charCode == 33) { int p[7] = int[7](0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04); return (p[6-y] >> (4-x)) & 1; } // !
    if (charCode == 37) { int p[7] = int[7](0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03); return (p[6-y] >> (4-x)) & 1; } // %
    if (charCode == 48) { int p[7] = int[7](0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; } // 0
    if (charCode == 49) { int p[7] = int[7](0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E); return (p[6-y] >> (4-x)) & 1; } // 1
    if (charCode == 50) { int p[7] = int[7](0x0E, 0x11, 0x01, 0x0E, 0x10, 0x10, 0x1F); return (p[6-y] >> (4-x)) & 1; } // 2
    if (charCode == 51) { int p[7] = int[7](0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; } // 3
    if (charCode == 52) { int p[7] = int[7](0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02); return (p[6-y] >> (4-x)) & 1; } // 4
    if (charCode == 53) { int p[7] = int[7](0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; } // 5
    if (charCode == 54) { int p[7] = int[7](0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; } // 6
    if (charCode == 55) { int p[7] = int[7](0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08); return (p[6-y] >> (4-x)) & 1; } // 7
    if (charCode == 56) { int p[7] = int[7](0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; } // 8
    if (charCode == 57) { int p[7] = int[7](0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E); return (p[6-y] >> (4-x)) & 1; } // 9
    if (charCode == 77) { int p[7] = int[7](0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11); return (p[6-y] >> (4-x)) & 1; } // M
    if (charCode == 84) { int p[7] = int[7](0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04); return (p[6-y] >> (4-x)) & 1; } // T
    if (charCode == 72) { int p[7] = int[7](0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11); return (p[6-y] >> (4-x)) & 1; } // H
    if (charCode == 58) { int p[7] = int[7](0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00); return (p[6-y] >> (4-x)) & 1; } // :
    if (charCode == 78) { int p[7] = int[7](0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11); return (p[6-y] >> (4-x)) & 1; } // N
    if (charCode == 87) { int p[7] = int[7](0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11); return (p[6-y] >> (4-x)) & 1; } // W
    if (charCode == 80) { int p[7] = int[7](0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10); return (p[6-y] >> (4-x)) & 1; } // P
    if (charCode == 73) { int p[7] = int[7](0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E); return (p[6-y] >> (4-x)) & 1; } // I
    if (charCode == 88) { int p[7] = int[7](0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11); return (p[6-y] >> (4-x)) & 1; } // X
    if (charCode == 90) { int p[7] = int[7](0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F); return (p[6-y] >> (4-x)) & 1; } // Z
    if (charCode == 46) { int p[7] = int[7](0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04); return (p[6-y] >> (4-x)) & 1; } // .
    if (charCode == 45) { int p[7] = int[7](0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00); return (p[6-y] >> (4-x)) & 1; } // -
    if (charCode == 68) { int p[7] = int[7](0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C); return (p[6-y] >> (4-x)) & 1; } // D
    if (charCode == 32) return 0; // space
    return 0;
}

void main() {
    int charIndex = int(vUV.x * float(uCharCount));
    charIndex = clamp(charIndex, 0, uCharCount - 1);
    float localX = fract(vUV.x * float(uCharCount));
    float localY = vUV.y;
    int px = clamp(int(localX * 5.0), 0, 4);
    int py = clamp(int(localY * 7.0), 0, 6);
    int charCode = uChars[charIndex];
    int pixel = getPixel(charCode, px, py);
    if (pixel == 1) fragColor = vec4(uColor, uAlpha);
    else discard;
}
)";

// fragment shader for overlay - just solid color
inline const char* overlayFS = R"(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform vec3 uColor;
uniform float uAlpha;
void main() { fragColor = vec4(uColor, uAlpha); }
)";
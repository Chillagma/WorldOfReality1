#define NOMINMAX

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>
#include <vector>

namespace {

    constexpr float PI = 3.14159265359f;

    std::string readFile(const std::string& path) {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) {
            std::fprintf(stderr, "Failed to open: %s\n", path.c_str());
            return {};
        }
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
        if (!v || !f) {
            if (v) glDeleteShader(v);
            if (f) glDeleteShader(f);
            return 0;
        }
        GLuint prog = glCreateProgram();
        glAttachShader(prog, v);
        glAttachShader(prog, f);
        glLinkProgram(prog);
        glDeleteShader(v);
        glDeleteShader(f);
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

    const char* thumbnailVS = R"(
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

    const char* thumbnailFS = R"(
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

    const char* textVS = R"(
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

    const char* textFS = R"(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform vec3 uColor;
uniform float uAlpha;
uniform int uCharCount;
uniform int uChars[32];

int getPixel(int charCode, int x, int y) {
    if (charCode == 71) { int p[7] = int[7](0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 79) { int p[7] = int[7](0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 65) { int p[7] = int[7](0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 76) { int p[7] = int[7](0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 89) { int p[7] = int[7](0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 85) { int p[7] = int[7](0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 82) { int p[7] = int[7](0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 83) { int p[7] = int[7](0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 67) { int p[7] = int[7](0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 69) { int p[7] = int[7](0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 33) { int p[7] = int[7](0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 37) { int p[7] = int[7](0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 48) { int p[7] = int[7](0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 49) { int p[7] = int[7](0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 50) { int p[7] = int[7](0x0E, 0x11, 0x01, 0x0E, 0x10, 0x10, 0x1F); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 51) { int p[7] = int[7](0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 52) { int p[7] = int[7](0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 53) { int p[7] = int[7](0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 54) { int p[7] = int[7](0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 55) { int p[7] = int[7](0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 56) { int p[7] = int[7](0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 57) { int p[7] = int[7](0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 77) { int p[7] = int[7](0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 84) { int p[7] = int[7](0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 72) { int p[7] = int[7](0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 58) { int p[7] = int[7](0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 78) { int p[7] = int[7](0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 87) { int p[7] = int[7](0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 80) { int p[7] = int[7](0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 73) { int p[7] = int[7](0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 88) { int p[7] = int[7](0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 90) { int p[7] = int[7](0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 46) { int p[7] = int[7](0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 45) { int p[7] = int[7](0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 68) { int p[7] = int[7](0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C); return (p[6-y] >> (4-x)) & 1; }
    if (charCode == 32) return 0;
    return 0;
}

void main() {
    int charIndex = int(vUV.x * float(uCharCount));
    charIndex = clamp(charIndex, 0, uCharCount - 1);
    float localX = fract(vUV.x * float(uCharCount));
    float localY = vUV.y;
    int px = int(localX * 5.0);
    int py = int(localY * 7.0);
    px = clamp(px, 0, 4);
    py = clamp(py, 0, 6);
    int charCode = uChars[charIndex];
    int pixel = getPixel(charCode, px, py);
    if (pixel == 1) fragColor = vec4(uColor, uAlpha);
    else discard;
}
)";

    const char* overlayFS = R"(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform vec3 uColor;
uniform float uAlpha;
void main() { fragColor = vec4(uColor, uAlpha); }
)";

    struct ShaderUniforms {
        GLint iChannel0 = -1, iTime = -1, iResolution = -1, iMouse = -1;
        GLint uKeyW = -1, uKeyS = -1, uKeyA = -1, uKeyD = -1;
        GLint uCameraOverride = -1, uCameraPosOverride = -1, uCameraRotOverride = -1;

        void init(GLuint program) {
            iChannel0 = glGetUniformLocation(program, "iChannel0");
            iTime = glGetUniformLocation(program, "iTime");
            iResolution = glGetUniformLocation(program, "iResolution");
            iMouse = glGetUniformLocation(program, "iMouse");
            uKeyW = glGetUniformLocation(program, "uKeyW");
            uKeyS = glGetUniformLocation(program, "uKeyS");
            uKeyA = glGetUniformLocation(program, "uKeyA");
            uKeyD = glGetUniformLocation(program, "uKeyD");
            uCameraOverride = glGetUniformLocation(program, "uCameraOverride");
            uCameraPosOverride = glGetUniformLocation(program, "uCameraPosOverride");
            uCameraRotOverride = glGetUniformLocation(program, "uCameraRotOverride");
        }
    };

    struct CameraState {
        float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
        float dirX = 0.0f, dirY = 0.0f, dirZ = 1.0f;
        float pitch = 0.0f, yaw = 0.0f;

        void setFromAngles(float p, float y) {
            pitch = p; yaw = y;
            dirX = std::cos(pitch) * std::sin(yaw);
            dirY = std::sin(pitch);
            dirZ = std::cos(pitch) * std::cos(yaw);
            float len = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
            if (len > 0.0001f) { dirX /= len; dirY /= len; dirZ /= len; }
        }

        void randomize(std::mt19937& rng) {
            std::uniform_real_distribution<float> posDist(-8.0f, 8.0f);
            std::uniform_real_distribution<float> yawDist(-PI, PI);
            std::uniform_real_distribution<float> pitchDist(-0.5f, 0.5f);
            posX = posDist(rng); posY = posDist(rng) * 0.2f; posZ = posDist(rng);
            setFromAngles(pitchDist(rng), yawDist(rng));
        }
    };

    struct PlayerCameraTracker {
        float mouseX = 0.0f, mouseY = 0.0f, mouseDown = 0.0f;
        float dirX = 0.0f, dirY = 0.0f, dirZ = 1.0f;
        float posX = 0.0f, posY = 0.0f, posZ = 0.0f;

        void reset() {
            mouseX = mouseY = mouseDown = 0.0f;
            dirX = 0.0f; dirY = 0.0f; dirZ = 1.0f;
            posX = posY = posZ = 0.0f;
        }

        void update(float resX, float resY, float curMX, float curMY, bool mouseDownNow,
            bool keyW, bool keyS, bool keyA, bool keyD) {
            if (mouseDownNow && mouseDown > 0.0f) {
                float dx = (curMX - mouseX) * 8.0f / resY;
                float dy = (curMY - mouseY) * 8.0f / resY;

                float pitch = std::asin(std::clamp(dirY, -1.0f, 1.0f));
                pitch = pitch - dy;
                pitch = std::clamp(pitch, -1.56f, 1.56f);

                float yawDelta = -dx;
                float cosY = std::cos(yawDelta);
                float sinY = std::sin(yawDelta);
                float newX = dirX * cosY - dirZ * sinY;
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

            mouseX = curMX; mouseY = curMY; mouseDown = mouseDownNow ? 1.0f : 0.0f;

            float speed = 0.4f;
            float rightX = -dirZ, rightZ = dirX;
            float rightLen = std::sqrt(rightX * rightX + rightZ * rightZ);
            if (rightLen > 0.0001f) { rightX /= rightLen; rightZ /= rightLen; }

            posX += (keyW - keyS) * speed * dirX;
            posY += (keyW - keyS) * speed * dirY;
            posZ += (keyW - keyS) * speed * dirZ;
            posX += (keyD - keyA) * speed * rightX * -1.0f;
            posZ += (keyD - keyA) * speed * rightZ * -1.0f;
        }
    };

    float smoothMatch(float distance, float halfDist) {
        return 100.0f / (1.0f + (distance * distance) / (halfDist * halfDist));
    }

    struct MatchComponents {
        float position = 0.0f, direction = 0.0f, total = 0.0f;
        float posDist = 0.0f, dirDist = 0.0f;
    };

    MatchComponents getMatchComponents(const CameraState& goal, const PlayerCameraTracker& player) {
        MatchComponents m;
        m.posDist = std::sqrt(std::pow(goal.posX - player.posX, 2.0f) +
            std::pow(goal.posY - player.posY, 2.0f) +
            std::pow(goal.posZ - player.posZ, 2.0f));
        m.position = smoothMatch(m.posDist, 10.0f);

        float dot = goal.dirX * player.dirX + goal.dirY * player.dirY + goal.dirZ * player.dirZ;
        dot = std::clamp(dot, -1.0f, 1.0f);
        m.dirDist = std::acos(dot);
        m.direction = smoothMatch(m.dirDist, 0.5f);

        m.total = m.position * 0.5f + m.direction * 0.5f;
        return m;
    }

    struct Thumbnail {
        GLuint texture = 0, program = 0;
        GLint uRect = -1, uTexture = -1, uAlpha = -1, uBorderSize = -1, uBorderColor = -1, uShowLabel = -1, uLabelHeight = -1;
        int texWidth = 0, texHeight = 0;
        bool hasContent = false, animating = false;
        double animStartTime = 0.0;
        float animDuration = 0.6f;
        float targetX = 0.0f, targetY = 0.0f, targetW = 0.0f, targetH = 0.0f;

        void initShader() {
            program = createProgram(thumbnailVS, thumbnailFS);
            if (program) {
                uRect = glGetUniformLocation(program, "uRect");
                uTexture = glGetUniformLocation(program, "uTexture");
                uAlpha = glGetUniformLocation(program, "uAlpha");
                uBorderSize = glGetUniformLocation(program, "uBorderSize");
                uBorderColor = glGetUniformLocation(program, "uBorderColor");
                uShowLabel = glGetUniformLocation(program, "uShowLabel");
                uLabelHeight = glGetUniformLocation(program, "uLabelHeight");
            }
        }
        void createTexture(int w, int h) {
            if (!texture) glGenTextures(1, &texture);
            texWidth = w; texHeight = h;
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        void captureFramebuffer(int w, int h) {
            if (!texture || texWidth != w || texHeight != h) createTexture(w, h);
            glBindTexture(GL_TEXTURE_2D, texture);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
            glBindTexture(GL_TEXTURE_2D, 0);
            hasContent = true;
        }
        void startAnimation(double currentTime) { animating = true; animStartTime = currentTime; }
        float easeOutBack(float t) {
            const float c1 = 1.70158f, c3 = c1 + 1.0f;
            return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
        }
        void render(GLuint quadVao, double currentTime, bool animate, float borderR, float borderG, float borderB, bool showLabel) {
            if (!hasContent || !program || !texture) return;
            float rectX, rectY, rectW, rectH, alpha = 1.0f;
            if (animate && animating) {
                float elapsed = static_cast<float>(currentTime - animStartTime);
                float t = std::min(elapsed / animDuration, 1.0f);
                if (t >= 1.0f) animating = false;
                float easedT = easeOutBack(t);
                rectX = -1.0f + (targetX - (-1.0f)) * easedT;
                rectY = -1.0f + (targetY - (-1.0f)) * easedT;
                rectW = 2.0f + (targetW - 2.0f) * easedT;
                rectH = 2.0f + (targetH - 2.0f) * easedT;
            }
            else { rectX = targetX; rectY = targetY; rectW = targetW; rectH = targetH; }

            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(program);
            if (uRect >= 0) glUniform4f(uRect, rectX, rectY, rectW, rectH);
            if (uAlpha >= 0) glUniform1f(uAlpha, alpha);
            if (uBorderSize >= 0) glUniform1f(uBorderSize, 0.03f);
            if (uBorderColor >= 0) glUniform3f(uBorderColor, borderR, borderG, borderB);
            if (uShowLabel >= 0) glUniform1i(uShowLabel, showLabel ? 1 : 0);
            if (uLabelHeight >= 0) glUniform1f(uLabelHeight, 0.08f);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture);
            if (uTexture >= 0) glUniform1i(uTexture, 0);
            glBindVertexArray(quadVao); glDrawArrays(GL_TRIANGLES, 0, 6); glBindVertexArray(0);
            glDisable(GL_BLEND);
        }
        void cleanup() {
            if (texture) { glDeleteTextures(1, &texture); texture = 0; }
            if (program) { glDeleteProgram(program); program = 0; }
        }
    };

    struct TextRenderer {
        GLuint program = 0;
        GLint uRect = -1, uColor = -1, uAlpha = -1, uCharCount = -1, uChars = -1;
        void init() {
            program = createProgram(textVS, textFS);
            if (program) {
                uRect = glGetUniformLocation(program, "uRect");
                uColor = glGetUniformLocation(program, "uColor");
                uAlpha = glGetUniformLocation(program, "uAlpha");
                uCharCount = glGetUniformLocation(program, "uCharCount");
                uChars = glGetUniformLocation(program, "uChars");
            }
        }
        void renderText(GLuint quadVao, const char* text, float x, float y, float w, float h, float r, float g, float b, float alpha) {
            if (!program) return;
            int len = static_cast<int>(strlen(text));
            if (len == 0 || len > 32) return;
            int chars[32] = { 0 };
            for (int i = 0; i < len; i++) chars[i] = static_cast<int>(text[i]);
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(program);
            if (uRect >= 0) glUniform4f(uRect, x, y, w, h);
            if (uColor >= 0) glUniform3f(uColor, r, g, b);
            if (uAlpha >= 0) glUniform1f(uAlpha, alpha);
            if (uCharCount >= 0) glUniform1i(uCharCount, len);
            if (uChars >= 0) glUniform1iv(uChars, 32, chars);
            glBindVertexArray(quadVao); glDrawArrays(GL_TRIANGLES, 0, 6); glBindVertexArray(0);
            glDisable(GL_BLEND);
        }
        void cleanup() { if (program) { glDeleteProgram(program); program = 0; } }
    };

    struct OverlayRenderer {
        GLuint program = 0;
        GLint uRect = -1, uColor = -1, uAlpha = -1;
        void init() {
            program = createProgram(thumbnailVS, overlayFS);
            if (program) {
                uRect = glGetUniformLocation(program, "uRect");
                uColor = glGetUniformLocation(program, "uColor");
                uAlpha = glGetUniformLocation(program, "uAlpha");
            }
        }
        void renderOverlay(GLuint quadVao, float x, float y, float w, float h, float r, float g, float b, float alpha) {
            if (!program) return;
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(program);
            if (uRect >= 0) glUniform4f(uRect, x, y, w, h);
            if (uColor >= 0) glUniform3f(uColor, r, g, b);
            if (uAlpha >= 0) glUniform1f(uAlpha, alpha);
            glBindVertexArray(quadVao); glDrawArrays(GL_TRIANGLES, 0, 6); glBindVertexArray(0);
            glDisable(GL_BLEND);
        }
        void cleanup() { if (program) { glDeleteProgram(program); program = 0; } }
    };

    struct App {
        GLFWwindow* window = nullptr;
        int fbW = 0, fbH = 0;
        GLuint quadVao = 0, quadVbo = 0;

        // Only Buffer A (for camera state)
        GLuint bufAFbo[2] = { 0 }, bufATex[2] = { 0 };
        int bufAIdx = 0;

        GLuint progBufferA = 0, progImage = 0;
        ShaderUniforms uniformsA, uniformsImage;

        bool keyW = false, keyS = false, keyA = false, keyD = false, mouseLeft = false;
        double mouseX = 0, mouseY = 0;

        Thumbnail goalThumbnail, playerThumbnail;
        CameraState goalCamera;
        PlayerCameraTracker playerCamera;
        TextRenderer textRenderer;
        OverlayRenderer overlayRenderer;

        bool spacePressed = false, goalCaptured = false, showingSuccess = false;
        double successStartTime = 0.0;
        MatchComponents lastMatch, liveMatch;
        double lastFrameTime = 0.0;

        static constexpr float SUCCESS_THRESHOLD = 75.0f;
        static constexpr float SUCCESS_DISPLAY_TIME = 3.0f;
        static constexpr float THUMBNAIL_SIZE = 0.35f;
        static constexpr float MARGIN = 0.02f;

        std::mt19937 rng;
    };

    App* gApp = nullptr;

    bool createFboTexturePair(GLuint& fbo, GLuint& tex, int width, int height) {
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "FBO incomplete: 0x%x\n", status);
            return false;
        }
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    void recreateBufferFbos(App& app) {
        for (int i = 0; i < 2; ++i) {
            if (app.bufATex[i]) glDeleteTextures(1, &app.bufATex[i]);
            if (app.bufAFbo[i]) glDeleteFramebuffers(1, &app.bufAFbo[i]);
            app.bufATex[i] = app.bufAFbo[i] = 0;
        }
        for (int i = 0; i < 2; ++i) {
            createFboTexturePair(app.bufAFbo[i], app.bufATex[i], app.fbW, app.fbH);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void framebuffer_size_cb(GLFWwindow*, int, int) {
        if (!gApp) return;
        glfwGetFramebufferSize(gApp->window, &gApp->fbW, &gApp->fbH);
        if (gApp->fbW <= 0 || gApp->fbH <= 0) return;
        glViewport(0, 0, gApp->fbW, gApp->fbH);
        recreateBufferFbos(*gApp);
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
            if (action == GLFW_PRESS) {
                gApp->goalCaptured = false;
                gApp->goalCamera.randomize(gApp->rng);
                gApp->playerThumbnail.hasContent = false;
                gApp->playerCamera.reset();
            }
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

    inline void setCommonUniforms(const ShaderUniforms& u, float time, float resX, float resY, float mouseX, float mouseY, float mouseLeft) {
        if (u.iTime >= 0) glUniform1f(u.iTime, time);
        if (u.iResolution >= 0) glUniform2f(u.iResolution, resX, resY);
        if (u.iMouse >= 0) glUniform4f(u.iMouse, mouseX, mouseY, mouseLeft, 0.f);
    }

    inline void setKeyUniforms(const ShaderUniforms& u, bool w, bool s, bool a, bool d) {
        if (u.uKeyW >= 0) glUniform1f(u.uKeyW, w ? 1.f : 0.f);
        if (u.uKeyS >= 0) glUniform1f(u.uKeyS, s ? 1.f : 0.f);
        if (u.uKeyA >= 0) glUniform1f(u.uKeyA, a ? 1.f : 0.f);
        if (u.uKeyD >= 0) glUniform1f(u.uKeyD, d ? 1.f : 0.f);
    }

    inline void setCameraOverride(const ShaderUniforms& u, bool enable, const CameraState& cam) {
        if (u.uCameraOverride >= 0) glUniform1i(u.uCameraOverride, enable ? 1 : 0);
        if (enable) {
            if (u.uCameraPosOverride >= 0) glUniform3f(u.uCameraPosOverride, cam.posX, cam.posY, cam.posZ);
            if (u.uCameraRotOverride >= 0) glUniform2f(u.uCameraRotOverride, cam.pitch, cam.yaw);
        }
    }

    void calculateThumbnailRect(float aspectRatio, float size, float margin, bool topRight, float& x, float& y, float& w, float& h) {
        w = size * 2.0f;
        h = w / aspectRatio;
        if (h > size * 1.5f) { h = size * 1.5f; w = h * aspectRatio; }
        if (topRight) { x = 1.0f - w - margin; y = 1.0f - h - margin; }
        else { x = 1.0f - w - margin; y = -1.0f + margin; }
    }

}

int main() {
    std::string shaderDir = SHADER_DIR;
    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return EXIT_FAILURE; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Camera Hunt (SPACE=check, R=new)", nullptr, nullptr);
    if (!window) { std::fprintf(stderr, "glfwCreateWindow failed\n"); glfwTerminate(); return EXIT_FAILURE; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "gladLoadGLLoader failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    App app;
    app.window = window;
    gApp = &app;
    std::random_device rd;
    app.rng.seed(rd());

    glfwGetFramebufferSize(window, &app.fbW, &app.fbH);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_cb);
    glfwSetKeyCallback(window, key_cb);
    glfwSetMouseButtonCallback(window, mouse_btn_cb);

    constexpr float quadVerts[] = { -1.f, -1.f, 1.f, -1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f, 1.f };
    glGenVertexArrays(1, &app.quadVao);
    glGenBuffers(1, &app.quadVbo);
    glBindVertexArray(app.quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, app.quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);

    app.goalThumbnail.initShader();
    app.playerThumbnail.initShader();
    app.textRenderer.init();
    app.overlayRenderer.init();

    std::string vsSrc = readFile(shaderDir + "fullscreen.vert");
    std::string commonSrc = readFile(shaderDir + "common.glsl");
    std::string bufAFs = readFile(shaderDir + "buffer_a.frag");
    std::string imgFs = readFile(shaderDir + "image.frag");

    if (vsSrc.empty() || commonSrc.empty() || bufAFs.empty() || imgFs.empty()) {
        std::fprintf(stderr, "Failed to load shader files\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::string fullImgFs = commonSrc + imgFs;
    app.progBufferA = createProgram(vsSrc.c_str(), bufAFs.c_str());
    app.progImage = createProgram(vsSrc.c_str(), fullImgFs.c_str());

    if (!app.progBufferA || !app.progImage) {
        std::fprintf(stderr, "Failed to create shader programs\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    app.uniformsA.init(app.progBufferA);
    app.uniformsImage.init(app.progImage);

    // Debug print uniform locations
    std::printf("Image shader uniforms: override=%d, pos=%d, rot=%d\n",
        app.uniformsImage.uCameraOverride,
        app.uniformsImage.uCameraPosOverride,
        app.uniformsImage.uCameraRotOverride);

    glUseProgram(app.progBufferA);
    if (app.uniformsA.iChannel0 >= 0) glUniform1i(app.uniformsA.iChannel0, 0);
    glUseProgram(app.progImage);
    if (app.uniformsImage.iChannel0 >= 0) glUniform1i(app.uniformsImage.iChannel0, 0);
    glUseProgram(0);

    recreateBufferFbos(app);
    app.goalCamera.randomize(app.rng);

    float aspectRatio = static_cast<float>(app.fbW) / static_cast<float>(app.fbH);
    calculateThumbnailRect(aspectRatio, App::THUMBNAIL_SIZE, App::MARGIN, true,
        app.goalThumbnail.targetX, app.goalThumbnail.targetY,
        app.goalThumbnail.targetW, app.goalThumbnail.targetH);
    calculateThumbnailRect(aspectRatio, App::THUMBNAIL_SIZE, App::MARGIN, false,
        app.playerThumbnail.targetX, app.playerThumbnail.targetY,
        app.playerThumbnail.targetW, app.playerThumbnail.targetH);

    bool shouldCapture = false;
    app.lastFrameTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glfwGetCursorPos(window, &app.mouseX, &app.mouseY);

        const double currentTime = glfwGetTime();
        app.lastFrameTime = currentTime;

        if (app.spacePressed) { shouldCapture = true; app.spacePressed = false; }

        const float time = static_cast<float>(currentTime);
        const float resX = static_cast<float>(app.fbW);
        const float resY = static_cast<float>(app.fbH);
        const float mouseX = static_cast<float>(app.mouseX);
        const float mouseY = static_cast<float>(app.fbH - app.mouseY);
        const float mouseLeftF = app.mouseLeft ? 1.f : 0.f;

        float newAspect = resX / resY;
        if (std::abs(newAspect - aspectRatio) > 0.001f) {
            aspectRatio = newAspect;
            calculateThumbnailRect(aspectRatio, App::THUMBNAIL_SIZE, App::MARGIN, true,
                app.goalThumbnail.targetX, app.goalThumbnail.targetY,
                app.goalThumbnail.targetW, app.goalThumbnail.targetH);
            calculateThumbnailRect(aspectRatio, App::THUMBNAIL_SIZE, App::MARGIN, false,
                app.playerThumbnail.targetX, app.playerThumbnail.targetY,
                app.playerThumbnail.targetW, app.playerThumbnail.targetH);
        }

        // Update C++ camera tracker
        app.playerCamera.update(resX, resY, mouseX, mouseY, app.mouseLeft, app.keyW, app.keyS, app.keyA, app.keyD);

        // Capture goal with multiple frames to stabilize
        if (!app.goalCaptured) {
            for (int frame = 0; frame < 5; frame++) {
                glBindVertexArray(app.quadVao);

                // Buffer A pass
                {
                    const int readIdx = app.bufAIdx, writeIdx = 1 - readIdx;
                    glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[writeIdx]);
                    glViewport(0, 0, app.fbW, app.fbH);
                    glUseProgram(app.progBufferA);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, app.bufATex[readIdx]);
                    setCommonUniforms(app.uniformsA, time, resX, resY, 0, 0, 0);
                    setKeyUniforms(app.uniformsA, false, false, false, false);
                    setCameraOverride(app.uniformsA, true, app.goalCamera);
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
                    setCommonUniforms(app.uniformsImage, time, resX, resY, 0, 0, 0);
                    setCameraOverride(app.uniformsImage, true, app.goalCamera);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }

                glBindVertexArray(0);
            }

            app.goalThumbnail.captureFramebuffer(app.fbW, app.fbH);
            app.goalCaptured = true;

            // Clear buffer A for fresh player start
            for (int i = 0; i < 2; i++) {
                glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[i]);
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            app.playerCamera.reset();

            std::printf("Goal captured: pos(%.2f, %.2f, %.2f) dir(%.2f, %.2f, %.2f)\n",
                app.goalCamera.posX, app.goalCamera.posY, app.goalCamera.posZ,
                app.goalCamera.dirX, app.goalCamera.dirY, app.goalCamera.dirZ);
        }

        // Normal player render
        glBindVertexArray(app.quadVao);

        // Buffer A pass
        {
            const int readIdx = app.bufAIdx, writeIdx = 1 - readIdx;
            glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[writeIdx]);
            glViewport(0, 0, app.fbW, app.fbH);
            glUseProgram(app.progBufferA);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app.bufATex[readIdx]);
            setCommonUniforms(app.uniformsA, time, resX, resY, mouseX, mouseY, mouseLeftF);
            setKeyUniforms(app.uniformsA, app.keyW, app.keyS, app.keyA, app.keyD);
            setCameraOverride(app.uniformsA, false, app.goalCamera);
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
            setCommonUniforms(app.uniformsImage, time, resX, resY, mouseX, mouseY, mouseLeftF);
            setCameraOverride(app.uniformsImage, false, app.goalCamera);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glBindVertexArray(0);

        // Live match calculation
        app.liveMatch = getMatchComponents(app.goalCamera, app.playerCamera);

        if (shouldCapture) {
            app.playerThumbnail.captureFramebuffer(app.fbW, app.fbH);
            app.playerThumbnail.startAnimation(currentTime);
            app.lastMatch = app.liveMatch;
            std::printf("CAPTURE: Pos=%.0f%% Dir=%.0f%% Total=%.0f%%\n",
                app.lastMatch.position, app.lastMatch.direction, app.lastMatch.total);
            if (app.lastMatch.total >= App::SUCCESS_THRESHOLD) {
                app.showingSuccess = true;
                app.successStartTime = currentTime;
                std::printf("SUCCESS!\n");
            }
            shouldCapture = false;
        }

        // UI
        if (app.goalThumbnail.hasContent) {
            app.goalThumbnail.render(app.quadVao, currentTime, false, 1.0f, 0.8f, 0.0f, true);
            app.textRenderer.renderText(app.quadVao, "GOAL",
                app.goalThumbnail.targetX + app.goalThumbnail.targetW * 0.3f,
                app.goalThumbnail.targetY + app.goalThumbnail.targetH * 0.85f,
                app.goalThumbnail.targetW * 0.4f, app.goalThumbnail.targetH * 0.1f, 1, 1, 1, 1);
        }

        if (app.playerThumbnail.hasContent) {
            app.playerThumbnail.render(app.quadVao, currentTime, true,
                1.0f - app.lastMatch.total / 100.0f, app.lastMatch.total / 100.0f, 0.2f, true);
        }

        // HUD
        {
            float hudX = -0.98f, hudY = -0.98f, hudW = 0.6f, hudH = 0.28f;
            app.overlayRenderer.renderOverlay(app.quadVao, hudX, hudY, hudW, hudH, 0.0f, 0.0f, 0.0f, 0.6f);
            char buf[64];
            float textH = 0.05f, textW = 0.55f, textX = hudX + 0.02f;

            float totalColor = app.liveMatch.total / 100.0f;
            std::snprintf(buf, sizeof(buf), "TOTAL: %d%%", static_cast<int>(app.liveMatch.total));
            app.textRenderer.renderText(app.quadVao, buf, textX, hudY + 0.20f, textW, textH, 1.0f - totalColor, totalColor, 0.3f, 1.0f);

            float posColor = app.liveMatch.position / 100.0f;
            std::snprintf(buf, sizeof(buf), "POS: %d%%", static_cast<int>(app.liveMatch.position));
            app.textRenderer.renderText(app.quadVao, buf, textX, hudY + 0.12f, textW, textH, 1.0f - posColor, posColor, 0.3f, 1.0f);

            float dirColor = app.liveMatch.direction / 100.0f;
            std::snprintf(buf, sizeof(buf), "DIR: %d%%", static_cast<int>(app.liveMatch.direction));
            app.textRenderer.renderText(app.quadVao, buf, textX, hudY + 0.04f, textW, textH, 1.0f - dirColor, dirColor, 0.3f, 1.0f);
        }

        // Success
        if (app.showingSuccess) {
            float elapsed = static_cast<float>(currentTime - app.successStartTime);
            if (elapsed > App::SUCCESS_DISPLAY_TIME) {
                app.showingSuccess = false;
                app.goalCaptured = false;
                app.goalCamera.randomize(app.rng);
                app.playerThumbnail.hasContent = false;
                app.playerCamera.reset();
            }
            else {
                float alpha = 0.7f;
                if (elapsed < 0.3f) alpha *= elapsed / 0.3f;
                else if (elapsed > App::SUCCESS_DISPLAY_TIME - 0.5f)
                    alpha *= (App::SUCCESS_DISPLAY_TIME - elapsed) / 0.5f;
                app.overlayRenderer.renderOverlay(app.quadVao, -0.5f, -0.15f, 1.0f, 0.3f, 0.1f, 0.5f, 0.1f, alpha);
                app.textRenderer.renderText(app.quadVao, "SUCCESS!", -0.35f, -0.05f, 0.7f, 0.15f, 1, 1, 1, 1);
                if (elapsed > 1.5f) {
                    float textAlpha = std::min((elapsed - 1.5f) / 0.3f, 1.0f);
                    app.textRenderer.renderText(app.quadVao, "NEW GOAL", -0.35f, -0.20f, 0.7f, 0.08f, 1, 1, 0.5f, textAlpha);
                }
            }
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
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
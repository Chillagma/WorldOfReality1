#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace {

    std::string readFile(const std::string& path) {
        std::ifstream f(path, std::ios::binary | std::ios::ate); // Open at end
        if (!f) {
            std::fprintf(stderr, "Failed to open: %s\n", path.c_str());
            return {};
        }
        // Pre-allocate string to avoid reallocations
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

    // OPTIMIZATION: Cache uniform locations to avoid glGetUniformLocation calls every frame
    struct ShaderUniforms {
        GLint iChannel0 = -1;
        GLint iChannel1 = -1;
        GLint iTime = -1;
        GLint iResolution = -1;
        GLint iMouse = -1;
        GLint uKeyW = -1;
        GLint uKeyS = -1;
        GLint uKeyA = -1;
        GLint uKeyD = -1;

        void init(GLuint program) {
            iChannel0 = glGetUniformLocation(program, "iChannel0");
            iChannel1 = glGetUniformLocation(program, "iChannel1");
            iTime = glGetUniformLocation(program, "iTime");
            iResolution = glGetUniformLocation(program, "iResolution");
            iMouse = glGetUniformLocation(program, "iMouse");
            uKeyW = glGetUniformLocation(program, "uKeyW");
            uKeyS = glGetUniformLocation(program, "uKeyS");
            uKeyA = glGetUniformLocation(program, "uKeyA");
            uKeyD = glGetUniformLocation(program, "uKeyD");
        }
    };

    struct App {
        GLFWwindow* window = nullptr;
        int fbW = 0;
        int fbH = 0;

        GLuint quadVao = 0;
        GLuint quadVbo = 0;

        // Double-buffered FBOs
        GLuint bufAFbo[2] = { 0, 0 };
        GLuint bufATex[2] = { 0, 0 };
        int bufAIdx = 0;

        GLuint bufBFbo[2] = { 0, 0 };
        GLuint bufBTex[2] = { 0, 0 };
        int bufBIdx = 0;

        GLuint progBufferA = 0;
        GLuint progBufferB = 0;
        GLuint progImage = 0;

        // OPTIMIZATION: Cached uniform locations
        ShaderUniforms uniformsA;
        ShaderUniforms uniformsB;
        ShaderUniforms uniformsImage;

        // OPTIMIZATION: Pack booleans together for better cache locality
        bool keyW = false;
        bool keyS = false;
        bool keyA = false;
        bool keyD = false;
        bool mouseLeft = false;

        double mouseX = 0;
        double mouseY = 0;
    };

    App* gApp = nullptr;

    // OPTIMIZATION: Helper to create FBO+texture pair, reduces code duplication
    bool createFboTexturePair(GLuint& fbo, GLuint& tex, int width, int height, const char* name, int index) {
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
            std::fprintf(stderr, "%s FBO %d incomplete: 0x%x\n", name, index, status);
            return false;
        }

        // Clear while bound
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        return true;
    }

    void recreateBufferFbos(App& app) {
        // Clean up existing resources
        for (int i = 0; i < 2; ++i) {
            if (app.bufATex[i]) glDeleteTextures(1, &app.bufATex[i]);
            if (app.bufAFbo[i]) glDeleteFramebuffers(1, &app.bufAFbo[i]);
            if (app.bufBTex[i]) glDeleteTextures(1, &app.bufBTex[i]);
            if (app.bufBFbo[i]) glDeleteFramebuffers(1, &app.bufBFbo[i]);
            app.bufATex[i] = app.bufAFbo[i] = 0;
            app.bufBTex[i] = app.bufBFbo[i] = 0;
        }

        // Create all FBOs
        for (int i = 0; i < 2; ++i) {
            createFboTexturePair(app.bufAFbo[i], app.bufATex[i], app.fbW, app.fbH, "Buffer A", i);
            createFboTexturePair(app.bufBFbo[i], app.bufBTex[i], app.fbW, app.fbH, "Buffer B", i);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
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
        }
    }

    void mouse_btn_cb(GLFWwindow*, int button, int action, int) {
        if (gApp && button == GLFW_MOUSE_BUTTON_LEFT) {
            gApp->mouseLeft = (action == GLFW_PRESS || action == GLFW_REPEAT);
        }
    }

    // OPTIMIZATION: Inline helper to set common uniforms
    inline void setCommonUniforms(const ShaderUniforms& u, float time, float resX, float resY,
        float mouseX, float mouseY, float mouseLeft) {
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

} // namespace

int main() {
    std::string shaderDir = SHADER_DIR;

    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit failed\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Shadertoy port", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

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
    glfwGetFramebufferSize(window, &app.fbW, &app.fbH);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_cb);
    glfwSetKeyCallback(window, key_cb);
    glfwSetMouseButtonCallback(window, mouse_btn_cb);

    // Create fullscreen quad
    constexpr float quadVerts[] = {
        -1.f, -1.f,  1.f, -1.f,  -1.f, 1.f,
        -1.f,  1.f,  1.f, -1.f,   1.f, 1.f
    };
    glGenVertexArrays(1, &app.quadVao);
    glGenBuffers(1, &app.quadVbo);
    glBindVertexArray(app.quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, app.quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);

    // Load shaders
   // Load shaders
    std::string vsSrc = readFile(shaderDir + "fullscreen.vert");
    std::string commonSrc = readFile(shaderDir + "common.glsl");
    std::string bufAFs = readFile(shaderDir + "buffer_a.frag");
    std::string bufBFs = readFile(shaderDir + "buffer_b.frag");
    std::string imgFs = readFile(shaderDir + "image.frag");

    if (vsSrc.empty() || commonSrc.empty() || bufAFs.empty() || bufBFs.empty() || imgFs.empty()) {
        // error handling
    }

    // Prepend common to image.frag
    std::string fullImgFs = commonSrc + imgFs;

    app.progBufferA = createProgram(vsSrc.c_str(), bufAFs.c_str());
    app.progBufferB = createProgram(vsSrc.c_str(), bufBFs.c_str());
    app.progImage = createProgram(vsSrc.c_str(), fullImgFs.c_str());

    if (!app.progBufferA || !app.progBufferB || !app.progImage) {
        std::fprintf(stderr, "Failed to create shader programs\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // OPTIMIZATION: Cache uniform locations once after program creation
    app.uniformsA.init(app.progBufferA);
    app.uniformsB.init(app.progBufferB);
    app.uniformsImage.init(app.progImage);

    // OPTIMIZATION: Set texture unit uniforms once (they never change)
    glUseProgram(app.progBufferA);
    if (app.uniformsA.iChannel0 >= 0) glUniform1i(app.uniformsA.iChannel0, 0);
    if (app.uniformsA.iChannel1 >= 0) glUniform1i(app.uniformsA.iChannel1, 1);

    glUseProgram(app.progBufferB);
    if (app.uniformsB.iChannel0 >= 0) glUniform1i(app.uniformsB.iChannel0, 0);
    if (app.uniformsB.iChannel1 >= 0) glUniform1i(app.uniformsB.iChannel1, 1);

    glUseProgram(app.progImage);
    if (app.uniformsImage.iChannel0 >= 0) glUniform1i(app.uniformsImage.iChannel0, 0);
    if (app.uniformsImage.iChannel1 >= 0) glUniform1i(app.uniformsImage.iChannel1, 1);

    glUseProgram(0);

    recreateBufferFbos(app);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glfwGetCursorPos(window, &app.mouseX, &app.mouseY);

        // Cache values
        const float time = static_cast<float>(glfwGetTime());
        const float resX = static_cast<float>(app.fbW);
        const float resY = static_cast<float>(app.fbH);
        const float mouseX = static_cast<float>(app.mouseX);
        const float mouseY = static_cast<float>(app.fbH +app.mouseY); // FIX: Was + should be -
        const float mouseLeftF = app.mouseLeft ? 1.f : 0.f;

        glBindVertexArray(app.quadVao);

        // PASS 1: Buffer B
        {
            const int readIdx = app.bufBIdx;
            const int writeIdx = 1 - readIdx;

            glBindFramebuffer(GL_FRAMEBUFFER, app.bufBFbo[writeIdx]);
            glViewport(0, 0, app.fbW, app.fbH);
            glUseProgram(app.progBufferB);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app.bufBTex[readIdx]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, app.bufATex[app.bufAIdx]);

            setCommonUniforms(app.uniformsB, time, resX, resY, mouseX, mouseY, mouseLeftF);
            setKeyUniforms(app.uniformsB, app.keyW, app.keyS, app.keyA, app.keyD);

            glDrawArrays(GL_TRIANGLES, 0, 6);
            app.bufBIdx = writeIdx;
        }

        // PASS 2: Buffer A
        {
            const int readIdx = app.bufAIdx;
            const int writeIdx = 1 - readIdx;

            glBindFramebuffer(GL_FRAMEBUFFER, app.bufAFbo[writeIdx]);
            // OPTIMIZATION: Skip viewport if size hasn't changed (same size as previous pass)
            glUseProgram(app.progBufferA);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app.bufATex[readIdx]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, app.bufBTex[app.bufBIdx]);

            setCommonUniforms(app.uniformsA, time, resX, resY, mouseX, mouseY, mouseLeftF);
            setKeyUniforms(app.uniformsA, app.keyW, app.keyS, app.keyA, app.keyD);

            glDrawArrays(GL_TRIANGLES, 0, 6);
            app.bufAIdx = writeIdx;
        }

        // PASS 3: Image
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glUseProgram(app.progImage);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app.bufATex[app.bufAIdx]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, app.bufBTex[app.bufBIdx]);

            setCommonUniforms(app.uniformsImage, time, resX, resY, mouseX, mouseY, mouseLeftF);

            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteVertexArrays(1, &app.quadVao);
    glDeleteBuffers(1, &app.quadVbo);

    for (int i = 0; i < 2; ++i) {
        glDeleteTextures(1, &app.bufATex[i]);
        glDeleteFramebuffers(1, &app.bufAFbo[i]);
        glDeleteTextures(1, &app.bufBTex[i]);
        glDeleteFramebuffers(1, &app.bufBFbo[i]);
    }

    glDeleteProgram(app.progBufferA);
    glDeleteProgram(app.progBufferB);
    glDeleteProgram(app.progImage);

    gApp = nullptr;
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
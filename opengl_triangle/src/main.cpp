#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "Failed to open: %s\n", path.c_str());
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
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

struct App {
    GLFWwindow* window = nullptr;
    int fbW = 0;
    int fbH = 0;

    GLuint quadVao = 0;
    GLuint quadVbo = 0;

    GLuint bufFbo[2] = {0, 0};
    GLuint bufTex[2] = {0, 0};
    int bufIdx = 0;

    GLuint progBufferA = 0;
    GLuint progImage = 0;

    bool keyW = false, keyS = false, keyA = false, keyD = false;
    double mouseX = 0, mouseY = 0;
    bool mouseLeft = false;
};

App* gApp = nullptr;

void recreateBufferFbo(App& app) {
    for (int i = 0; i < 2; ++i) {
        if (app.bufTex[i]) {
            glDeleteTextures(1, &app.bufTex[i]);
            app.bufTex[i] = 0;
        }
        if (app.bufFbo[i]) {
            glDeleteFramebuffers(1, &app.bufFbo[i]);
            app.bufFbo[i] = 0;
        }
    }

    for (int i = 0; i < 2; ++i) {
        glGenFramebuffers(1, &app.bufFbo[i]);
        glGenTextures(1, &app.bufTex[i]);
        glBindTexture(GL_TEXTURE_2D, app.bufTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, app.fbW, app.fbH, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, app.bufFbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, app.bufTex[i], 0);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "FBO incomplete: 0x%x\n", status);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, app.bufFbo[i]);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void framebuffer_size_cb(GLFWwindow* w, int width, int height) {
    (void)w;
    if (gApp) {
        glfwGetFramebufferSize(gApp->window, &gApp->fbW, &gApp->fbH);
        if (gApp->fbW <= 0 || gApp->fbH <= 0) {
            return;
        }
        glViewport(0, 0, gApp->fbW, gApp->fbH);
        recreateBufferFbo(*gApp);
    }
}

void key_cb(GLFWwindow*, int key, int, int action, int) {
    if (!gApp) {
        return;
    }
    const bool down = (action == GLFW_PRESS || action == GLFW_REPEAT);
    if (key == GLFW_KEY_W) {
        gApp->keyW = down;
    }
    if (key == GLFW_KEY_S) {
        gApp->keyS = down;
    }
    if (key == GLFW_KEY_A) {
        gApp->keyA = down;
    }
    if (key == GLFW_KEY_D) {
        gApp->keyD = down;
    }
}

void mouse_btn_cb(GLFWwindow*, int button, int action, int) {
    if (!gApp || button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    gApp->mouseLeft = (action == GLFW_PRESS || action == GLFW_REPEAT);
}

}  // namespace

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

    const float quadVerts[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f, 1.f};
    glGenVertexArrays(1, &app.quadVao);
    glGenBuffers(1, &app.quadVbo);
    glBindVertexArray(app.quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, app.quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);

    const std::string vsPath = shaderDir + "fullscreen.vert";
    const std::string vsSrc = readFile(vsPath);
    if (vsSrc.empty()) {
        return EXIT_FAILURE;
    }
    const std::string bufPath = shaderDir + "buffer_a.frag";
    const std::string imgPath = shaderDir + "image.frag";
    const std::string bufFs = readFile(bufPath);
    const std::string imgFs = readFile(imgPath);
    if (bufFs.empty() || imgFs.empty()) {
        return EXIT_FAILURE;
    }

    app.progBufferA = createProgram(vsSrc.c_str(), bufFs.c_str());
    app.progImage = createProgram(vsSrc.c_str(), imgFs.c_str());
    if (!app.progBufferA || !app.progImage) {
        return EXIT_FAILURE;
    }

    recreateBufferFbo(app);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glfwGetCursorPos(window, &app.mouseX, &app.mouseY);

        const int readIdx = app.bufIdx;
        const int writeIdx = 1 - app.bufIdx;

        glBindFramebuffer(GL_FRAMEBUFFER, app.bufFbo[writeIdx]);
        glViewport(0, 0, app.fbW, app.fbH);
        glUseProgram(app.progBufferA);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, app.bufTex[readIdx]);
        glUniform1i(glGetUniformLocation(app.progBufferA, "uChannel0"), 0);
        glUniform2f(glGetUniformLocation(app.progBufferA, "iResolution"),
                      static_cast<float>(app.fbW), static_cast<float>(app.fbH));
        glUniform4f(glGetUniformLocation(app.progBufferA, "iMouse"),
                    static_cast<float>(app.mouseX), static_cast<float>(app.mouseY),
                    app.mouseLeft ? 1.f : 0.f, 0.f);
        glUniform1f(glGetUniformLocation(app.progBufferA, "uKeyW"), app.keyW ? 1.f : 0.f);
        glUniform1f(glGetUniformLocation(app.progBufferA, "uKeyS"), app.keyS ? 1.f : 0.f);
        glUniform1f(glGetUniformLocation(app.progBufferA, "uKeyA"), app.keyA ? 1.f : 0.f);
        glUniform1f(glGetUniformLocation(app.progBufferA, "uKeyD"), app.keyD ? 1.f : 0.f);

        glBindVertexArray(app.quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        app.bufIdx = writeIdx;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, app.fbW, app.fbH);
        glUseProgram(app.progImage);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, app.bufTex[app.bufIdx]);
        glUniform1i(glGetUniformLocation(app.progImage, "uChannel0"), 0);
        glUniform1f(glGetUniformLocation(app.progImage, "iTime"), static_cast<float>(glfwGetTime()));
        glUniform2f(glGetUniformLocation(app.progImage, "iResolution"),
                    static_cast<float>(app.fbW), static_cast<float>(app.fbH));
        glUniform4f(glGetUniformLocation(app.progImage, "iMouse"),
                    static_cast<float>(app.mouseX), static_cast<float>(app.mouseY),
                    app.mouseLeft ? 1.f : 0.f, 0.f);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &app.quadVao);
    glDeleteBuffers(1, &app.quadVbo);
    for (int i = 0; i < 2; ++i) {
        glDeleteTextures(1, &app.bufTex[i]);
        glDeleteFramebuffers(1, &app.bufFbo[i]);
    }
    glDeleteProgram(app.progBufferA);
    glDeleteProgram(app.progImage);

    gApp = nullptr;
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}

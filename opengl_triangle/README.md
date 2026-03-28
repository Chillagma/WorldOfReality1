# opengl_triangle

OpenGL 3.3 (Core) demo: fullscreen quad with a Shadertoy-style multi-pass setup. Buffer A stores camera state; Image presents the final frame.

## Requirements

- CMake 3.16+
- C++17 compiler
- OpenGL 3.3 drivers
- Internet on first configure (CMake fetches GLFW 3.4)

## Build (Windows, Visual Studio)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Shaders load from the `shaders/` folder next to the sources (path set at compile time); a copy is also placed next to the built executable.

## Controls

| Input | Action |
|--------|--------|
| **W A S D** | Move |
| **Mouse + left button** | Look around |

## Layout

| Path | Role |
|------|------|
| `src/main.cpp` | Window, uniforms, render loop |
| `shaders/` | `fullscreen.vert`, `buffer_a.frag`, `image.frag` |
| `third_party/glad/` | OpenGL loader |

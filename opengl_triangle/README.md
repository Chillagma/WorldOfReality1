# opengl_triangle

check

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

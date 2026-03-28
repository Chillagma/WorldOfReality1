# WorldOfReality
# check testing testing 123
OpenGL 3.3 demo: a Shadertoy-style fullscreen shader runner with camera movement (WASD + mouse look).

## Contents

- **`opengl_triangle/`** — CMake project, source, and shaders. See [`opengl_triangle/README.md`](opengl_triangle/README.md) for build steps and controls.

## Quick build

```powershell
cd opengl_triangle
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Requires CMake 3.16+, C++17, and OpenGL 3.3. GLFW is fetched automatically on first configure.

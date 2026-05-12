# World of Reality

A 3D OpenGL camera hunting game where players must match a goal camera position to photograph specific objects.

## Prerequisites

- **Windows 10/11** (developed with Visual Studio)
- **Visual Studio 2022** or newer
- **CMake 3.15+**
- **Git** (optional, for cloning)

## Setup

### 1. Clone the Repository

```bash
git clone <your-repo-url>
cd WorldOfReality
```

### 2. Open in Visual Studio

1. Open the `WorldOfReality` folder
2. Look for `CMakeLists.txt` at the root or in subdirectories
3. Visual Studio should automatically detect the CMake project
4. Or use **File > Open > Folder** and select `WorldOfReality`

### 3. Build with CMake

```bash
# Navigate to the OpenGL project
cd opengl_triangle

# Create build directory
mkdir build
cd build

# Generate project files
cmake .. -G "Visual Studio 17 2022" -A x64

# Build
cmake --build . --config Release
```

### 4. Run the Executable

After building, find the executable in:
```
opengl_triangle\build\Release\opengl_triangle.exe
```

Or run directly from Visual Studio by pressing **F5**.

## Controls

| Input | Action |
|-------|--------|
| **W / S** | Move forward/backward |
| **A / D** | Strafe left/right |
| **Mouse + Left Click** | Look around (hold) |
| **Space** | Capture and check match |
| **R** | Generate new goal |

## How It Works

1. **Goal**: A random camera position and target is generated
2. **Gameplay**: Navigate using WASD and mouse to match the goal camera
3. **Capture**: Press Space to check your match percentage
4. **Score**: Match above 75% to score and get a new goal

## Troubleshooting

### "Cannot find GLFW" or "Cannot find GLAD"
- Ensure third_party dependencies are present
- Some versions require running CMake from the Visual Studio Developer Command Prompt

### Build Errors
- Make sure Visual Studio is up to date
- Verify CMake is in your PATH: `cmake --version`

### Black Screen or Crash
- Check that shaders folder is next to the executable
- Verify your GPU supports OpenGL 3.3+

## License

MIT License
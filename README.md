# Ray Tracing Harmonic Functions

Intro to Computer Graphics final project by Zachary Taylor, Aubrey Koch and Yu-Cheng Liu.

We are implementing **Harnack tracing** from *Ray Tracing Harmonic Functions* (Gillespie, Yang, Botsch, Crane, SIGGRAPH 2024) in C++ and OpenGL. All ray tracing runs in a GLSL fragment shader drawn on a full-screen quad. We compare it against a fixed-step ray marching baseline.

## Scope

- Two scenes:
  1. **Spherical harmonics:** level sets of the harmonic polynomial `x²y − yz²` inside the unit ball.
  2. **Nonplanar polygons:** the smooth surface spanning a closed 3D loop, rendered as the 2π level set of the loop's solid angle.
- Two tracers:
  - **Harnack tracing:** the paper's Algorithm 1, plus Algorithm 2 for angle-valued functions like the solid angle.
  - **Fixed-step ray marching:** the baseline.
- A comparison of the two on frame time, average steps per pixel and missing surface (holes), checked against the authors' released shaders.

## Planned features

- Switch between Harnack tracing and fixed-step marching with a key.
- Adjust the fixed step size with keys, to show the trade-off between speed and holes.
- Orbit camera: drag to rotate, scroll to zoom.
- Lambert shading from the function's gradient. Polygons use Biot–Savart normals.
- A heatmap mode that colors each pixel by its step count.
- On-GPU measurements: frame time from timer queries, and average steps per pixel.
- Stretch goals: over-stepping acceleration, a comparison of stopping conditions, and more of the paper's scenes (polygons with holes, knots, point clouds, gyroid).

## Steps

1. **Setup:** full-screen quad, camera uniforms, one ray per pixel.
2. **Baseline:** a fixed-step marcher with bisection, tested on a sphere.
3. **Harnack tracing:** Algorithm 1 on the spherical harmonic scene.
4. **Nonplanar polygons:** solid angle, Algorithm 2, Biot–Savart normals.
5. **Measurements:** timings, step counts, heatmap, comparison with the authors' shaders.
6. **Write-up and presentation.**

## Building

You need CMake and a C++ compiler. GLFW, GLM and GLAD are included in the repo.

```bash
mkdir build
cd build
cmake ..
```

- **Linux / macOS:** run `make` in the `build` directory. Some Linux distributions need extra packages for GLFW; CMake lists them during the step above.
- **Windows:** open the generated Visual Studio solution (`.sln` or `.slnx`) in `build`, right-click `learnply2` in Solution Explorer and choose **Build**.

Shaders are embedded in the executable when the project is configured. If you add a shader, add it to `RESOURCES` in `CMakeLists.txt` and re-run `cmake ..`. If you add a source file, add it to `add_executable` in the same file.

## Running

From the `build` directory:

```bash
./learnply2
```

In Visual Studio, right-click `learnply2`, choose **Set as Startup Project**, then press F5.

The program needs OpenGL 3.3 or newer. Right now it draws a full-screen quad in a solid color; the ray tracer will go in `shaders/raytrace.frag`.

### Controls

- **Left-drag:** rotate the camera
- **Ctrl + left-drag:** pan
- **Scroll:** zoom
- **P:** switch between perspective and orthographic
- **R:** reset the camera
- **Esc:** quit

## Project structure

- `src/main.cpp`: window, input and the draw call for the full-screen quad
- `src/camera.*`: orbit camera
- `src/shader.*`: compiles and links shaders
- `shaders/quad.vert`: vertex shader for the full-screen quad
- `shaders/raytrace.frag`: fragment shader that will do the ray tracing
- `glfw-3.5.1/`, `glm/`, `glad/`: libraries
- `cmake/`: scripts that embed the shaders in the executable

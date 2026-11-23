# learnply2

## Building

You will need an installation of CMake and a C++ compiler to build the project. Install CMake from their [official website](https://cmake.org/download/), or use your system's package manager. For example, on Debian distributions, you can install CMake with:

```bash
sudo apt-get update
sudo apt-get install cmake
```

After installing CMake, you can build the project by first creating a build directory and navigating into it:

```bash
mkdir build
cd build
```

Then, run CMake to configure the project and generate the build files:

```bash
cmake ..
```

**Note**: Some Linux distributions do not have all the required libraries out-of-box to build GLFW. GLFW will report such libraries at this step for which you will need to install as-needed.

Depending on your operating system, the generated build files will differ. On Linux and macOS, CMake will generate Makefiles by default, while on Windows, it will generate Visual Studio solution files.

### Linux and macOS

To build the project on Linux or macOS, you can use the following command from the build directory:

```bash
make
```

### Windows

Open the generated Visual Studio solution file (`.sln` or `.slnx`) in the build directory. Build by right-clicking on `learnply2` in the Solution Explorer and selecting "Build".

## Running the Program

The program takes a single command-line argument, which is the path to a PLY file. You can run the program from the command line as follows:

```bash
./learnply2 path/to/your/file.ply
```

You may also run the program in an IDE of your choice. Be sure to set command line arguments there.

**Note**: In Visual Studio, you can set the command-line arguments by right-clicking on the project in the Solution Explorer, selecting "Properties", and then navigating to "Debugging" to set the "Command Arguments". To set the project to be run, right-click on `learnply2` and click "Set as Startup Project". You can then start the program by pressing F5.

### Controls

Below are the built-in controls for the program.

#### Viewport

The viewport can be adjusted with the following controls:

- **Left-click** and drag to rotate view.
- **Control + Left-click** and drag to pan view.
- **Scroll** to zoom in and out
- **P** to toggle between orthographic/perspective projection.
- **Number keys** to switch between different rendering modes.
- **R** to reset the view.

#### Mesh Editing/Deformation

Some of the controls follow Blender's controls, so if you are familar with Blender some controls may feel similar.

- Hold **Alt** to pick an actuator vertex, when holding **Alt**:
    - **Left-lick** to confirm and select the vertex. Constraints will be generated using the actuation distance. The actuation vertex is displayed as a *red* sphere.
    - Release **Alt** to make no changes.
- When a vertex is selected, press **G** to grab the vertex to move its position, the vertex will move to your mouse. When in this mode:
    - Press **X**, **Y**, or **Z** to lock the translation to the X, Y, or Z axis, respectively.
    - Press **Shift+X**, **Shift+Y**, or **Shift+Z** to lock the translation to the Y/Z plane, X/Z plane, or X/Y plane.
    - Press **C** to remove any axis/plane locks.
    - **Right-click** to cancel the operation.
    - **Control + Left-click** to confirm the operation.
- **Control + scroll** to increase/decrease the actuation distance
    - **Shift** to have finer control over the distance
- **H** to toggle rendering of vertex constraints. Constraints are displayed as *pink* spheres.
- **L** to toggle between uniform/cord weights.
- **Control + Enter** to start deforming. The deformation will automatically stop after convergence. After the deformation is over, you may do another deformation operation. While the mesh is deforming you may use:
    - **Control + Enter** to accept the current view as the deformed mesh (stops deformation)
    - **Escape** to cancel the operation, returns the mesh to its pre-deformed state

## Project Structure

The project is organized into the following directories:

- `src/`: Contains the source code files for the project.
- `models/`: Contains the PLY model files used for testing and demonstration.
- `glfw-3.5.1/`: Contains the GLFW library used for creating windows and handling input.
- `glm`: Contains the GLM library used for mathematics operations in graphics programming.
- `glad`: Containts the GLAD library used as an OpenGL function loader.
- `cmake`: Contains extra CMake build scripts and unconfigured source files.
- `shaders`: Contains the GLSL source code for shaders in the program.
- `res`: Other resources, including textures.

## Resource Embedding

Resources are automatically embedded in the executable. This is done when the project is configured. If you choose to add resources (e.g. textures), edit the `RESOURCES` variable in the `CMakeLists.txt` file and reconfigure. You may need to delete `CMakeCache.txt` before reconfiguring.

To access the resources for the `learnply2` project, including `<learnply2_resources.h>` gives access to the function `learnply2_find_resource` for which you may request the data by name.

**Note**: If there are issues with resource embedding, you may also load from a file directly, although this is not implemented for you. Note that paths are relative to the working directory, so ensure that you run from the correct directory. Absolute paths hardcoded into your source are not recommended as this is not portable for other users.

## Homework 4

The `Deformation` class has been introduced, which implements the deformation algorithm laid out in [As-Rigid-As-Possible Surface Modelling](https://igl.ethz.ch/projects/ARAP/arap_web.pdf). This deformation algorithm is split into two passes per iteration:

1. Compute a per-vertex rotation matrix that best rotates a cell of undeformed vertices to their deformed positions.
2. Solve for the vertex positions that best fit the rotation matrices.

Understanding the basic idea of the algorithm will be **very** helpful. The As-Rigid-As-Possible energy is implemented in `Deformation::compute_energy`, it may be helpful to correspond the C++ code there with the energy formulation in the paper (equation 2). The full solver is implemented in `Deformation::solve`, with the aformentioned passes in (1) `Deformation::compute_rotation_matrices` and (2) `Deformation::solve_vertex_positions`. Parameters controlling the convergence of the solver are defined as constants in `deformation.h`. Feel free to change these if you want, they may not be the best for every mesh.

You will need to implement the TODOs in the following places:
- `Mesh::compute_distances` in `mesh.cpp` (completing this will allow you to see vertex constraints)
- `Deformation::compute_rotation_matrix` and `Deformation::solve_vertex_positions` in `deformation.cpp`

**Note**: When submitting your project, do not include the `build` directory in your submission. If you choose to add files, make sure you add them to the project by modifying the `CMakeLists.txt` file in the root directory of the project. This will ensure that CMake is aware of the new files and can include them in the build process. Be sure to re-run `cmake ..` from the build directory to add them to the build process.

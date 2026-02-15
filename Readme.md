# STL Analysis & Visualization Tool

A high-performance **C++ STL Viewer + Geometric Analyzer**.  
This application loads **binary STL files**, visualizes them using **Modern OpenGL (3.3 Core Profile)**, and performs **geometric comparisons** between:

- **Exact analytic geometry** (raw STL floating-point data)
- **Grid-discretized / voxel-snapped approximations** (SDF-style quantization)

---

## 🚀 Key Features

### ✅ Binary STL Loading
Efficient parsing of binary STL files, including:
- Bounding box computation
- Center of mass estimation
- Unique vertex count
- Mesh statistics

### 🎨 Dual-Mode Visualization
Toggle between two render modes:

- **Mesh View**  
  Standard shaded 3D mesh rendering.

- **Voxel View**  
  Displays the model as a point cloud based on a grid (default: **512³**, adjustable).

### 🎥 Dynamic Camera
- Orbit camera (mouse drag)
- Zoom (scroll)
- Supports:
  - **Perspective projection**
  - **Orthographic projection**

### 📤 Geometric Export Reports
Exports detailed triangle-by-triangle analysis to text files, including:

- Edge lengths: **L₁, L₂, L₃**
- Interior angles (degrees): **θ₁, θ₂, θ₃**
- Detection of **degenerate triangles**

---

## 🛠 Prerequisites & Dependencies

To build and run this project, install:

- **GLFW** (windowing + input)
- **GLM** (math library, header-only)
- **GLEW** (OpenGL extension loader)
- **OpenGL 3.3+ (Core Profile)**

---

## 🍎 macOS Notes

This project includes:

- `#define GL_SILENCE_DEPRECATION`
- macOS-friendly OpenGL headers

to ensure compatibility with modern macOS OpenGL implementations.

---

## 🎮 Controls & Shortcuts

| Key / Input | Action |
|------------|--------|
| **V** | Toggle Mesh ↔ Voxel visualization |
| **O** | Toggle Perspective ↔ Orthographic projection |
| **E** | Export **Exact Analysis** (`geometry_exact.txt`) |
| **F** | Export **SDF/Grid Analysis** (`geometry_sdf.txt`) |
| **Mouse Drag** | Rotate / orbit model |
| **Scroll Wheel** | Zoom in / out |
| **ESC** | Exit application |

---

## 🔬 Technical Explanation: Analysis Modes

The core uniqueness of this tool is comparing:

### 1) Exact Euclidean Mode (**E**)
- Uses raw floating-point coordinates directly from the STL
- Provides the **ground truth** geometry

Output:
- `geometry_exact.txt`

---

### 2) SDF / Grid Approximation Mode (**F**)
Simulates what happens when geometry is converted into:

- a **Signed Distance Field (SDF)**, or
- a **Voxel Grid**

#### How it works:
- The model space is divided into a grid (defined by `GRID_RESOLUTION`)
- Every vertex is **snapped to the nearest voxel center**
- Geometry calculations are performed on these snapped vertices

#### Why it matters:
This helps measure **step loss / quantization error**.

If the grid resolution is too low, many triangles may collapse and become:
- **degenerate**
- **flat**
- **zero-area**

Output:
- `geometry_sdf.txt`

---

## 📦 Installation (macOS)

```bash
# Install the core dependencies
brew install glfw glm glew

# Verify they are linked correctly
brew link glfw glm glew


---

## 📁 Prepare Your Files

Make sure your STL model is:

- Named: `model.stl`
- Located in the same folder as: `main.cpp`

Example structure:

```txt
project-root/
├── main.cpp
├── model.stl
├── CMakeLists.txt
└── ...


---

## ▶️ Build & Run

```bash
mkdir build
cd build
cmake ..

# Compile
make

# Run
./STLViewer

---

## ✅ Output Files

After using the export keys:

- Press **E** → generates `geometry_exact.txt`
- Press **F** → generates `geometry_sdf.txt`

These files contain **triangle-by-triangle measurements**, including:

- Edge lengths  
- Interior angles  
- Degenerate triangle detection  

---

## 📌 Summary

This tool is ideal for:

- Visual STL inspection  
- Mesh statistics extraction  
- Studying voxel/SDF quantization artifacts  
- Detecting degenerate triangles  
- Comparing exact vs discretized geometry  

---

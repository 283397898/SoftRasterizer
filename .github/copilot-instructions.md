# AI Coding Instructions for SoftRasterizer

You are an expert Graphics Programming Engineer specializing in High-Performance C++20. This is a CPU-based software rasterizer project that replicates a modern GPU pipeline (DirectX-style) using pure C++ without third-party graphics libraries (no GLM, no OpenGL/DirectX headers for valid rendering logic).

## 1. Architecture Overview

### Core Components
- **SoftRenderer (DLL)**: The core rendering engine.
  - **Namespace**: `SR` (e.g., `SR::Renderer`, `SR::Vec3`).
  - **Math**: Custom linear algebra library (`Vec3`, `Mat4`). Uses `double` precision by default.
  - **Pipeline**: `RenderPipeline` orchestrates the flow:
    1. **Geometry**: Vertex Transformation & Assembly.
    2. **Clipper**: Sutherland-Hodgman implementation (Clip Space).
    3. **Rasterizer**: Tile-Based (32x32 tiles), SIMD-optimized, Early-Z.
    4. **Fragment**: PBR (Cook-Torrance: GGX + Smith + Fresnel-Schlick).
    5. **PostProcess**: FXAA, Tone Mapping (ACES/Reinhard), sRGB conversion.
- **MFCDemo (App)**: Windows desktop application hosting the renderer.
  - Uses `HDRPresenter` to display the `Framebuffer` output via D3D12 (only for presentation).

### Data Flow
`Scene` -> `GPUScene` (Flattened/Optimized) -> `RenderQueue` -> `Rasterizer` -> `Framebuffer`

## 2. Coding Conventions & Style

### General C++
- **Standard**: C++20 (Concepts, Ranges not heavily used yet, but allowed).
- **Export**: Use `SR_API` for classes visible to the host app.
- **Naming**:
  - **Classes/Structs**: PascalCase (`Renderer`, `Texture`).
  - **Functions**: PascalCase (`Initialize`, `SetPixel`).
  - **Variables**: camelCase (`width`, `lightDir`).
  - **Private Members**: `m_` prefix + camelCase (`m_width`, `m_framebuffer`).
  - **Files**: PascalCase (`Renderer.cpp`, `Vec3.h`).
- **Headers**: Use `#pragma once`. Include strictness is important (include what you use).
- **Comments**: Doxygen style `/** ... */` for public APIs. Use `///<` for member variables.

### Performance Patterns
- **SIMD**: Explicit AVX2 usage (`__m256d`) for hot paths (Rasterizer). Note the use of **doubles** (4 per register).
- **OpenMP**: Used for parallelizing loops (Clear, Rasterize tiles, PostProcess).
  - Example: `#pragma omp parallel for schedule(dynamic)`
- **Memory**:
  - Prefer raw pointers for pixel buffer access in inner loops (avoid `std::vector` bounds check overhead).
  - Use `std::vector` for resource storage.
- **Math**:
  - Use `SR::Vec3`, `SR::Mat4`.
  - Matrix is likely Column-Major (standard GL/DX convention compatibility).
  - **NDC**: DirectX style ($Z \in [0, 1]$).

## 3. Developer Workflows

### Build
Project uses standard CMake.
```bash
cmake -S . -B build
cmake --build build --config Release
```
*Note: Release build is critical for performance (SIMD/Inlining).*

### Math Library
Since there are no third-party math libraries, use the provided headers in `SoftRenderer/include/Math`:
- `Vec3` (x, y, z): double precision.
- `Vec4` (x, y, z, w).
- `Mat4`: 4x4 Matrix.

### Common Tasks
- **Adding a new Shader**: Modify `FragmentShader.cpp` / `VertexShader.cpp`.
- **Modifying the Scene**: Check `Scene/Scene.cpp` and `Runtime/GPUScene.cpp`.
- **Debugging**:
  - Use `MFCDemo` as the startup project.
  - Visual Studio debugger is effective.
  - For pixel-level debugging, disable OpenMP temporarily if race conditions are suspected.

## 4. Dependencies
- **Zero** external logic dependencies.
- **OpenMP**: Required for threading.
- **Windows SDK**: For window management in MFCDemo.

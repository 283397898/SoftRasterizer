# Claude Code Configuration for SoftRasterizer

You are an expert Graphics Programming Engineer specializing in High-Performance C++20. This is a CPU-based software rasterizer project that replicates a modern GPU pipeline (DirectX-style) using pure C++ without third-party graphics libraries.

## Project Structure

```
SoftRasterizer/
├── SoftRenderer/          # Core rendering engine (DLL)
│   ├── include/
│   │   ├── Math/          # Vec3, Vec4, Mat4 (double precision)
│   │   ├── Renderer/      # Core rendering classes
│   │   ├── Pipeline/      # Geometry, Clipper, Rasterizer, Fragment, PostProcess
│   │   └── Scene/         # Scene management
│   └── src/
├── MFCDemo/               # Windows desktop app (uses D3D12 for display only)
└── example/               # Test assets (glb models, env maps)
```

## Coding Conventions

### Naming
| Type | Convention | Example |
|------|------------|---------|
| Classes/Structs | PascalCase | `Renderer`, `Texture` |
| Functions | PascalCase | `Initialize`, `SetPixel` |
| Variables | camelCase | `width`, `lightDir` |
| Private Members | `m_` + camelCase | `m_width`, `m_framebuffer` |
| Files | PascalCase | `Renderer.cpp`, `Vec3.h` |

### C++ Standards
- **Standard**: C++20 (Concepts, Ranges, Coroutines allowed but not heavily used)
- **Headers**: Use `#pragma once`
- **Namespace**: `SR` for core rendering engine (`SR::Renderer`, `SR::Vec3`)
- **Export**: Use `SR_API` macro for classes visible to host app
- **Comments**: Doxygen style `/** ... */` for public APIs, `///<` for members

### Performance Guidelines
- **SIMD**: Explicit AVX2 (`__m256d`) for hot paths - uses doubles (4 per register)
- **OpenMP**: Used for parallelizing loops:
  ```cpp
  #pragma omp parallel for schedule(dynamic)
  ```
- **Memory**:
  - Prefer raw pointers for pixel buffer access in inner loops (avoid `std::vector` bounds check overhead)
  - Use `std::vector` for resource storage
- **Math**:
  - Use `SR::Vec3`, `SR::Vec4`, `SR::Mat4` from `SoftRenderer/include/Math`
  - Matrix is Column-Major (standard GL/DX convention)
  - Double precision by default
- **NDC**: DirectX style (Z ∈ [0, 1])

## Rendering Pipeline

1. **Geometry**: Vertex Transformation & Assembly
2. **Clipper**: Sutherland-Hodgman implementation (Clip Space)
3. **Rasterizer**: Tile-Based (32x32 tiles), SIMD-optimized, Early-Z
4. **Fragment**: PBR (Cook-Torrance: GGX + Smith + Fresnel-Schlick)
5. **PostProcess**: FXAA, Tone Mapping (ACES/Reinhard), sRGB conversion

## Data Flow

`Scene` → `GPUScene` (Flattened/Optimized) → `RenderQueue` → `Rasterizer` → `Framebuffer`

## Dependencies

- **Zero** external logic dependencies (no GLM, no GL/DX headers for rendering)
- **OpenMP**: Required for threading
- **Windows SDK**: For window management in MFCDemo
- **D3D12**: Used only in MFCDemo's `HDRPresenter` for display, not for rendering

## Build System

```bash
cmake -S . -B build
cmake --build build --config Release
```

**Important**: Release build is critical for performance (SIMD/Inlining).

## Common Tasks

| Task | Location |
|------|----------|
| Add new shader | `FragmentShader.cpp` / `VertexShader.cpp` |
| Modify scene | `Scene/Scene.cpp`, `Runtime/GPUScene.cpp` |
| Modify rasterizer | `Pipeline/Rasterizer.cpp` |
| Add new post-process | `Pipeline/PostProcess.cpp` |
| Demo app changes | `MFCDemo/src/` |

## Debugging

- Use `MFCDemo` as the startup project
- Visual Studio debugger is effective
- For pixel-level debugging, disable OpenMP temporarily if race conditions are suspected
- Use `OutputDebugStringA()` for debug output

## File Organization Guidelines

- Place new render pipeline components in `SoftRenderer/include/Pipeline/` and `SoftRenderer/src/Pipeline/`
- Math helpers go in `SoftRenderer/include/Math/` and `SoftRenderer/src/Math/`
- Scene-related code goes in `SoftRenderer/include/Scene/` and `SoftRenderer/src/Scene/`
- MFCDemo-specific code stays in `MFCDemo/src/`

## Available Skills

| Skill | Purpose | When to Use |
|-------|---------|-------------|
| **cpp-pro** | Modern C++20/23, templates, SIMD | When implementing high-performance algorithms, template metaprogramming, memory optimization |
| **general-purpose** | Complex multi-step tasks | When working on multiple components or exploring the codebase |
| **explore** | Codebase analysis | When searching for files or understanding existing code patterns |
| **plan** | Architectural design | Before implementing major features or refactoring |
| **claude-code-guide** | Claude Code features | For questions about CLI commands, hooks, settings |

## When to Ask

- Before making architectural changes to the rendering pipeline
- When adding external dependencies (project aims to stay dependency-free for core rendering)
- Before changing math precision or coordinate systems
- When modifying the OpenMP parallelization strategy
- When unsure which skill to use for a task
- Before implementing new features that require significant architectural changes

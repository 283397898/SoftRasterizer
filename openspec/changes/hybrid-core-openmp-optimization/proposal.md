## Why

当前项目运行在 Intel 大小核（P+E core）混合架构 CPU 上，使用 ICX (IntelLLVM) 编译器。光栅化管线中 **大量热路径硬编码 `schedule(static)`**，将迭代空间等分给所有线程——P 核先完成后空等 E 核，导致帧时间被最慢核拖尾。需要全面优化 OpenMP 调度策略，使 P 核和 E 核都充分利用，最大化全核吞吐量。

## What Changes

- **将所有硬编码 `schedule(static)` 热路径改为 `schedule(guided)`**：涵盖 clip 阶段、bin counting、bin fill、bin sort、Framebuffer Clear、DepthBuffer Clear、FXAA、ToneMapping 等，guided 调度先分大块后分小块，P 核抢大块、E 核收小尾，barrier 等待最小化
- **新增 Intel OpenMP 运行时亲和性初始化**：在渲染器初始化阶段通过条件编译（`#if SR_INTEL_OMP`）设置 `KMP_AFFINITY` 和 `KMP_HW_SUBSET` 环境变量，确保线程均匀分散到所有物理核心（P+E）
- **扩展 `OpenMPTuningOptions` 配置**：将 clip 阶段、bin 阶段的调度策略也纳入可配置范围，与现有 tile raster / draw build 配置对齐
- **新增编译期检测宏 `SR_INTEL_OMP`**：通过条件编译隔离 Intel OpenMP 专用优化路径（KMP 亲和性、guided 默认策略），非 Intel 编译器回退到通用 OpenMP 行为
- **新增 per-thread 耗时采样**：在 profiling 模式下记录每个线程在 tile raster 阶段的实际工作时间，用于验证大小核负载均衡效果

## Capabilities

### New Capabilities
- `hybrid-affinity`: Intel 大小核 OpenMP 运行时亲和性初始化与条件编译隔离（`SR_INTEL_OMP` 宏检测、KMP 环境变量设置、编译器专用路径）

### Modified Capabilities
- `openmp-tuning`: 扩展可配置阶段（新增 clip/bin 调度选项），将所有硬编码 `static` 替换为 `guided`，新增 per-thread 耗时采样

## Impact

- **修改文件**：
  - `Pipeline/FrameContext.h` — `OpenMPTuningOptions` 新增 clip/bin 调度配置字段
  - `Pipeline/Rasterizer.cpp` — clip/bin/sort 阶段调度策略替换 + per-thread 计时
  - `Pipeline/OpaquePass.cpp` — 已有 runtime 调度路径无需大改
  - `Pipeline/EnvironmentMap.cpp` — SH/prefilter 循环调度替换
  - `Core/Framebuffer.cpp` — Clear/FXAA/ToneMap 调度替换
  - `Core/DepthBuffer.cpp` — Clear 调度替换
  - `Render/RendererConfig.cpp` — Sanitize 扩展新字段
  - `CMakeLists.txt`（根目录 + SoftRenderer）— 新增 `SR_INTEL_OMP` 编译宏定义
  - 新增 `Utils/IntelOMPInit.h/.cpp`（或内联在 Renderer 初始化中）— KMP 亲和性设置
- **API 影响**：`OpenMPTuningOptions` 新增字段（二进制兼容，不破坏已有调用方）
- **依赖**：无新外部依赖，仅利用 ICX 自带的 Intel OpenMP 运行时特性

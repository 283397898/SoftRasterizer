## Context

SoftRasterizer 运行在 Intel 12/13/14 代（Alder Lake / Raptor Lake）大小核混合架构 CPU 上，使用 Intel ICX (IntelLLVM) 编译器配合 `/Qiopenmp` 链接 Intel OpenMP 运行时（libiomp5）。

当前管线中存在 **11 处硬编码 `schedule(static)` 的 OpenMP 并行循环**，分布在：
- `Rasterizer.cpp`：clip 阶段（1 处）、bin counting（2 处）、bin fill（1 处）、bin sort（1 处）
- `Framebuffer.cpp`：Clear（2 处）、ClearLinear（1 处）、FXAA（1 处）、ResolveToSRGB（1 处）
- `DepthBuffer.cpp`：Clear（1 处）
- `EnvironmentMap.cpp`：SH9（1 处）、Prefilter mip0（1 处）

`static` 调度将迭代空间等分给所有线程。在大小核架构下，P 核（Golden Cove / Raptor Cove, ~5.0GHz）比 E 核（Gracemont, ~3.6GHz）快约 40-60%，导致 P 核做完后在 barrier 处空等 E 核，帧尾部出现 ~30% 时间浪费。

现有 `OpenMPTuningOptions` 已对 tile raster 和 draw build 两个阶段实现了 `schedule(runtime)` 可配置化（默认 `dynamic,1`），但其余阶段未覆盖。

## Goals / Non-Goals

**Goals:**
- 使 P 核和 E 核的总计算能力都被充分利用，最大化全核吞吐量
- 将所有 OpenMP 热路径的调度策略从硬编码 `static` 改为异构友好的 `guided`
- 通过条件编译宏 `SR_INTEL_OMP` 隔离 Intel OpenMP 专有优化（KMP 亲和性），保证 MSVC/GCC 编译兼容
- 扩展 `OpenMPTuningOptions`，将 clip/bin 阶段也纳入可配置范围
- 在 profiling 模式下提供 per-thread 耗时，验证负载均衡效果

**Non-Goals:**
- 不引入 TBB 或其他第三方并行库
- 不修改 SIMD (AVX2) 内核代码
- 不改变渲染结果的正确性（色彩/深度位一致）
- 不做 NUMA 感知优化（当前目标为单 socket 桌面平台）

## Decisions

### D1: `guided` 作为默认调度策略替代 `static`

**选择**: 将所有硬编码 `schedule(static)` 替换为 `schedule(guided)`

**替代方案对比**:

| 方案 | 优点 | 缺点 |
|------|------|------|
| `static`（现状） | 零调度开销，均匀负载最优 | 大小核拖尾严重，barrier 空等 ~30% |
| `dynamic,1` | 完美负载均衡 | 调度开销高（每次仅分 1 个迭代），在大迭代次数的 clear 循环中开销约 ~0.5μs/迭代 |
| `dynamic,N` | 开销可控 | 需要手动调 chunk，场景依赖 |
| **`guided`** | **自适应分块（先大后小），P 核自然抢到大块，E 核收小尾，调度开销介于 static 和 dynamic 之间** | 首次分配可能过大，极端不均匀场景不如 dynamic |
| TBB work-stealing | 最强负载均衡 | 引入外部依赖，重构并行框架，ROI 低 |

**理由**: `guided` 是大小核场景下最佳折中——调度开销仅比 `static` 高约 5-10%（约 50ns/分配），但能消除 30% 的 barrier 空等。对 clear/FXAA 等均匀负载场景，`guided` 的首批大块与 `static` 行为接近，尾部小块自适应异构差异。

### D2: `SR_INTEL_OMP` 条件编译宏

**选择**: CMake 中检测 `CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM"` 时自动定义 `SR_INTEL_OMP`

**条件编译作用范围**:
1. **KMP 亲和性初始化**：仅在 `SR_INTEL_OMP` 下调用 `kmp_set_defaults()` 或 `_putenv_s("KMP_...")`
2. **默认调度策略**：`SR_INTEL_OMP` 下默认 `guided`，其他编译器默认 `dynamic`（MSVC OpenMP 的 guided 实现质量较低）
3. **per-thread 计时**：使用 `omp_get_wtime()`（所有 OpenMP 实现都支持，不需条件编译）

**理由**: Intel OpenMP 运行时对 `guided` 调度和 `KMP_AFFINITY` 有专门优化，而 MSVC 的 OpenMP 实现仅支持 OpenMP 2.0 子集，`guided` 表现不稳定。条件编译保证两条路径都能正确编译运行。

### D3: KMP 亲和性策略

**选择**: `KMP_AFFINITY=granularity=core,balanced`

**替代方案**:

| 策略 | 行为 | 适用场景 |
|------|------|----------|
| `compact` | 线程紧密排列在连续核上 | 纯 P 核利用，浪费 E 核 |
| `scatter` | 线程尽量分散到不同 socket/核 | 多 socket NUMA |
| **`balanced`** | **Intel 混合架构专用，感知 P/E 核拓扑，均衡分配** | **大小核桌面 CPU** |
| `none` | 不绑定，OS 自由调度 | 可能被 OS 迁移线程到随机核 |

**理由**: `balanced` 是 Intel OneAPI 2023+ 为混合架构引入的策略，运行时会自动识别 P/E 核拓扑并做最优分配。配合 `KMP_HW_SUBSET=1t`（每核一线程）避免超线程 P 核上两个线程竞争 ALU，保证每个物理核独占一个 OpenMP 线程。

### D4: 扩展 `OpenMPTuningOptions` 新增字段

在 `FrameContext.h` 的 `OpenMPTuningOptions` 中新增：

```
clipSchedule        : OpenMPSchedulePolicy  (默认 Guided)
clipChunk           : int                   (默认 64)
binCountSchedule    : OpenMPSchedulePolicy  (默认 Guided)
binCountChunk       : int                   (默认 64)
clearSchedule       : OpenMPSchedulePolicy  (默认 Guided)
clearChunk          : int                   (默认 4096)
postProcessSchedule : OpenMPSchedulePolicy  (默认 Guided)
postProcessChunk    : int                   (默认 1)
```

**chunk 默认值估算**:
- **clip (64)**: 每个三角形 clip 耗时 ~0.5-2μs，64 个一组 = ~32-128μs/批次，guided 首批覆盖 P 核大吞吐
- **bin count (64)**: 类似 clip，per-tri 操作轻量
- **clear (4096)**: 每像素操作 ~1ns，4096 像素/批次 = ~4μs，调度开销（~50ns）占比 ~1%，可接受
- **postProcess (1)**: FXAA 按行并行，每行耗时差异不大，chunk=1 让 guided 自适应

### D5: per-thread 耗时采样实现

在 `#pragma omp parallel` 块内使用 `omp_get_wtime()` 记录每个线程的工作时间：

```
threadWorkMs[tid] = (endTime - startTime) * 1000.0
```

在 profiling 输出中报告 `maxThreadMs - minThreadMs` 作为**负载不均衡指标**（imbalance ratio）。目标：优化后 imbalance < 15%（优化前通常 30-50%）。

线程安全保证：`omp_get_wtime()` 是线程安全的；`threadWorkMs` 按 tid 索引的独立数组元素，无竞争。

## Risks / Trade-offs

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| `guided` 在小迭代次数时首批过大，退化为单线程 | Clear 操作在小分辨率（<640x480）下可能无加速 | 最小 chunk 保底：`guided` 的 chunk 参数定义最小粒度，不会退化 |
| KMP 环境变量设置时机：必须在首个并行区之前 | 若 DLL 加载时已有其他 OMP 并行区，设置无效 | 在 `Renderer::Initialize()` 最早调用点设置，或在 DLL 入口 `DllMain` 中设置 |
| MSVC 编译路径下 `guided` 行为不一致 | MSVC OpenMP 2.0 的 guided 实现可能不如 Intel | 条件编译：MSVC 下默认 `dynamic` 而非 `guided` |
| per-thread 计时增加约 ~200ns/并行区（`omp_get_wtime()` 调用） | 仅在 `enableProfiling=true` 时激活，Release 默认关闭 | 条件分支 + 编译器预测，热路径零开销 |
| 新增配置字段增加 `Sanitize()` 复杂度 | 新字段忘记 clamp 可能导致 chunk=0 崩溃 | `Sanitize()` 统一处理所有 chunk 字段，添加断言 |

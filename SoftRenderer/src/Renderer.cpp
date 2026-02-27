#include "SoftRenderer.h"

#include "Scene/Scene.h"
#include "Scene/ObjectGroup.h"
#include "Scene/Transform.h"
#include "Scene/RenderQueue.h"
#include "Render/FrameContextBuilder.h"
#include "Render/RenderQueueBuilder.h"
#include "Render/GPUSceneRenderQueueBuilder.h"
#include "Render/PassContext.h"
#include "Render/RenderPipeline.h"
#include "Runtime/GPUScene.h"
#include "Utils/DebugLog.h"

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <omp.h>

#if defined(SR_INTEL_OMP) && defined(_WIN32)
#include <windows.h>
// CRT lib 段静态初始化：确保在 DLL 的任何 OpenMP 并行区（包括全局构造）之前设置 KMP 环境变量。
// init_seg(lib) 在 C++ 全局构造之前执行，保证 libiomp5 首次 fork 时能读到正确的环境变量。
#pragma init_seg(lib)
static struct IntelOMPEarlyInit {
    IntelOMPEarlyInit() {
        // balanced 类型是 Intel OpenMP 为混合架构（P+E core）专门优化的亲和策略，
        // 会自动检测核心拓扑并均匀分配线程到 P 核和 E 核。
        _putenv_s("KMP_AFFINITY", "granularity=core,balanced");
        // 不设置 KMP_HW_SUBSET —— 让运行时自动检测所有逻辑处理器。
        // 在 i9-14900HX 等混合架构上，1t 限制可能导致 E 核线程不足。
    }
} s_intelOMPEarlyInit;
#endif

namespace SR {

namespace {

const char* ScheduleName(OpenMPSchedulePolicy policy) {
    switch (policy) {
    case OpenMPSchedulePolicy::Static:  return "static";
    case OpenMPSchedulePolicy::Guided:  return "guided";
    case OpenMPSchedulePolicy::Dynamic:
    default:                            return "dynamic";
    }
}

void LogOpenMPDiagnostics() {
#if defined(_WIN32)
    char buf[256];
    int maxThreads = omp_get_max_threads();
    int numProcs = omp_get_num_procs();
    std::snprintf(buf, sizeof(buf),
        "[SR] OpenMP: max_threads=%d num_procs=%d"
#if defined(SR_INTEL_OMP)
        " runtime=Intel(libiomp5) affinity=balanced"
#else
        " runtime=default"
#endif
        "\n", maxThreads, numProcs);
    OutputDebugStringA(buf);
#endif
}

} // namespace

/**
 * @brief 清除帧缓冲与深度缓冲
 *
 * HDR 模式下跳过 SDR 颜色清除（线性 HDR 始终清除）。
 * 深度缓冲初始化为 1.0（最大深度，即远平面值）。
 */
void Renderer::ClearBuffers() {
    if (!m_useHDR) {
        Color clearColor{16, 16, 16, 255};
        m_framebuffer.Clear(clearColor);
    }
    m_framebuffer.ClearLinear(Vec3{0.0, 0.0, 0.0});
    m_depthBuffer.Clear(1.0);
}

/**
 * @brief 从帧上下文构建渲染 Pass 上下文
 *
 * 将 FrameContext 与帧缓冲、后处理配置打包成 PassContext，
 * 供 RenderPipeline 执行所有 Pass 时使用。
 * HDR 模式下禁用色调映射（由外部呈现层处理）。
 */
PassContext Renderer::BuildPassContext(const FrameContext& frame) {
    PassContext passContext{};
    passContext.frame = frame;
    passContext.framebuffer = &m_framebuffer;
    passContext.depthBuffer = &m_depthBuffer;
    passContext.enableFXAA = m_config.enableFXAA;
    passContext.enableToneMap = m_config.enableToneMap && !m_useHDR;
    passContext.exposure = m_config.exposure;
    return passContext;
}

/**
 * @brief 向调试输出输出帧性能统计信息
 *
 * 通过 SR_PERF_LOG 输出，Release/Debug 均可见（OutputDebugStringA）。
 * 包含各阶段耗时、三角形/像素统计和 OMP 调度参数。
 */
void Renderer::LogFrameStats(const RenderStats& stats, double clearMs, double setupMs, double totalMs, const char* label, size_t itemCount) const {
    char buffer[256];
    if (itemCount > 0) {
        std::snprintf(
            buffer, sizeof(buffer),
            "%s Frame(ms): clear=%.3f setup=%.3f build=%.3f rast=%.3f total=%.3f | items=%zu tri: build=%llu clip=%llu rast=%llu | pix: test=%llu shade=%llu\n",
            label, clearMs, setupMs, stats.buildMs, stats.rastMs, totalMs, itemCount,
            static_cast<unsigned long long>(stats.trianglesBuilt),
            static_cast<unsigned long long>(stats.trianglesClipped),
            static_cast<unsigned long long>(stats.trianglesRaster),
            static_cast<unsigned long long>(stats.pixelsTested),
            static_cast<unsigned long long>(stats.pixelsShaded));
    } else {
        std::snprintf(
            buffer, sizeof(buffer),
            "%s Frame(ms): clear=%.3f setup=%.3f build=%.3f rast=%.3f total=%.3f | tri: build=%llu clip=%llu rast=%llu | pix: test=%llu shade=%llu\n",
            label, clearMs, setupMs, stats.buildMs, stats.rastMs, totalMs,
            static_cast<unsigned long long>(stats.trianglesBuilt),
            static_cast<unsigned long long>(stats.trianglesClipped),
            static_cast<unsigned long long>(stats.trianglesRaster),
            static_cast<unsigned long long>(stats.pixelsTested),
            static_cast<unsigned long long>(stats.pixelsShaded));
    }
    SR_PERF_LOG(buffer);

    const char* clipSched = ScheduleName(m_config.openmp.clipSchedule);
    const char* binSched = ScheduleName(m_config.openmp.binCountSchedule);
    const char* clearSched = ScheduleName(m_config.openmp.clearSchedule);
    const char* postSched = ScheduleName(m_config.openmp.postProcessSchedule);
    const char* rasterSched = ScheduleName(m_config.openmp.rasterTileSchedule);
    const char* buildSched = ScheduleName(m_config.openmp.drawItemBuildSchedule);

    char ompBuffer[512];
    std::snprintf(
        ompBuffer, sizeof(ompBuffer),
        "%s OMP: clip=%s,%d bin=%s,%d clear=%s,%d post=%s,%d build=%s,%d raster=%s,%d legacyBin=%d profiling=%d\n",
        label,
        clipSched, m_config.openmp.clipChunk,
        binSched, m_config.openmp.binCountChunk,
        clearSched, m_config.openmp.clearChunk,
        postSched, m_config.openmp.postProcessChunk,
        buildSched, m_config.openmp.drawItemBuildChunk,
        rasterSched, m_config.openmp.rasterTileChunk,
        m_config.openmp.enableLegacyBinReduction ? 1 : 0,
        m_config.openmp.enableProfiling ? 1 : 0);
    SR_PERF_LOG(ompBuffer);
}

/**
 * @brief 初始化渲染器
 * @param width 画布宽度
 * @param height 画布高度
 */
void Renderer::Initialize(int width, int height) {
    LogOpenMPDiagnostics();
    m_width = width;
    m_height = height;
    m_framebuffer.Resize(width, height);
    m_depthBuffer.Resize(width, height);
}

/**
 * @brief 设置是否启用 HDR 输出
 * @param enabled true 为启用
 */
void Renderer::SetHDR(bool enabled) {
    m_useHDR = enabled;
}

/**
 * @brief 设置帧上下文选项
 * @param options 配置参数
 */
void Renderer::SetFrameContextOptions(const FrameContextOptions& options) {
    m_config.frameContext = options;
}

/**
 * @brief 设置后处理参数
 * @param enableFXAA 是否启用 FXAA 抗锯齿
 * @param enableToneMap 是否启用色调映射
 * @param exposure 曝光值
 */
void Renderer::SetPostProcess(bool enableFXAA, bool enableToneMap, double exposure) {
    m_config.enableFXAA = enableFXAA;
    m_config.enableToneMap = enableToneMap;
    m_config.exposure = exposure;
}

/**
 * @brief 设置渲染器整体配置
 * @param config 配置对象
 */
void Renderer::SetConfig(const RendererConfig& config) {
    m_config = config;
    m_config.Sanitize();
}

/**
 * @brief 获取渲染器配置
 */
const RendererConfig& Renderer::GetConfig() const {
    return m_config;
}

/**
 * @brief 渲染传统场景
 * @param scene 场景对象
 */
void Renderer::Render(const Scene& scene) {
    using Clock = std::chrono::high_resolution_clock;
    auto frameStart = Clock::now();

    ClearBuffers();
    auto clearEnd = Clock::now();

    const ObjectGroup* objects = scene.GetObjectGroup();
    if (!objects) {
        return;
    }

    FrameContextBuilder frameContextBuilder;
    FrameContext frameContext = frameContextBuilder.Build(scene, m_width, m_height, m_config.frameContext);
    frameContext.environmentMap = m_config.environmentMap;
    frameContext.openmp = m_config.openmp;
    auto setupEnd = Clock::now();

    RenderQueue renderQueue;
    RenderQueueBuilder renderQueueBuilder;
    renderQueueBuilder.Build(*objects, renderQueue);

    PassContext passContext = BuildPassContext(frameContext);

    RenderPipeline pipeline;
    RenderStats stats = pipeline.Render(renderQueue, passContext);

    auto frameEnd = Clock::now();

    double clearMs = std::chrono::duration<double, std::milli>(clearEnd - frameStart).count();
    double setupMs = std::chrono::duration<double, std::milli>(setupEnd - clearEnd).count();
    double totalMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();

    LogFrameStats(stats, clearMs, setupMs, totalMs, "Scene");
}

/**
 * @brief 渲染 GPUScene (加速结构场景)
 * @param scene GPUScene 引用
 */
void Renderer::Render(const GPUScene& scene) {
    using Clock = std::chrono::high_resolution_clock;
    auto frameStart = Clock::now();

    ClearBuffers();
    auto clearEnd = Clock::now();

    FrameContext frameContext{};
    const FrameContextOptions& options = m_config.frameContext;
    frameContext.view = m_config.useViewOverride ? m_config.viewOverride : Mat4::Identity();
    frameContext.cameraPos = m_config.useCameraPosOverride ? m_config.cameraPosOverride : options.defaultCameraPos;
    double aspect = (m_height > 0) ? (static_cast<double>(m_width) / static_cast<double>(m_height)) : 1.0;
    frameContext.projection = Mat4::Perspective(options.fovYRadians, aspect, options.zNear, options.zFar);
    frameContext.ambientColor = options.ambientColor;
    frameContext.environmentMap = m_config.environmentMap;
    frameContext.openmp = m_config.openmp;
    frameContext.images = &scene.GetImages();
    frameContext.samplers = &scene.GetSamplers();
    DirectionalLight defaultLight;
    defaultLight.direction = options.defaultLightDirection;
    defaultLight.color = options.defaultLightColor;
    defaultLight.intensity = options.defaultLightIntensity;
    frameContext.lights.push_back(defaultLight);
    RenderQueue renderQueue;
    GPUSceneRenderQueueBuilder queueBuilder;
    queueBuilder.Build(scene, renderQueue, m_config.debugOnlyMaterialIndex);
    auto setupEnd = Clock::now();

    PassContext passContext = BuildPassContext(frameContext);

    RenderPipeline pipeline;
    SR_DEBUG_LOG("GPUScene Render: before pipeline\n");
    auto pipelineStart = Clock::now();
    RenderStats stats = pipeline.Render(renderQueue, passContext);
    auto pipelineEnd = Clock::now();
    SR_DEBUG_LOG("GPUScene Render: after pipeline\n");
    auto frameEnd = Clock::now();

    double clearMs = std::chrono::duration<double, std::milli>(clearEnd - frameStart).count();
    double setupMs = std::chrono::duration<double, std::milli>(setupEnd - clearEnd).count();
    double pipelineMs = std::chrono::duration<double, std::milli>(pipelineEnd - pipelineStart).count();
    double totalMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
    double unmeasuredMs = totalMs - clearMs - setupMs;
    double gapMs = unmeasuredMs - stats.buildMs - stats.rastMs;

    char gapBuf[256];
    std::snprintf(gapBuf, sizeof(gapBuf),
        "[SR-PERF] Frame breakdown: pipeline=%.3fms measured(build+rast)=%.3fms gap=%.3fms (postproc+sky+clear2+other)\n",
        pipelineMs, stats.buildMs + stats.rastMs, gapMs);
    SR_PERF_LOG(gapBuf);

    LogFrameStats(stats, clearMs, setupMs, totalMs, "GPUScene", renderQueue.GetItems().size());
}

/**
 * @brief 获取 SDR 帧缓冲像素数据
 * @return 指向像素数组的指针
 */
const uint32_t* Renderer::GetFramebuffer() const {
    return m_framebuffer.GetPixels();
}

/**
 * @brief 获取线性 HDR 帧缓冲像素数据
 * @return 指向线性颜色数组的指针
 */
const Vec3* Renderer::GetFramebufferLinear() const {
    return m_framebuffer.GetLinearPixels();
}

/**
 * @brief 获取渲染器宽度
 */
int Renderer::GetWidth() const {
    return m_width;
}

/**
 * @brief 获取渲染器高度
 */
int Renderer::GetHeight() const {
    return m_height;
}

} // namespace SR

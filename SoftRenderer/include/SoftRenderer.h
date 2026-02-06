#pragma once

#include <cstdint>

#include "Core/DepthBuffer.h"
#include "Core/Framebuffer.h"
#include "SoftRendererExport.h"
#include "Math/Vec3.h"
#include "Render/FrameContextBuilder.h"
#include "Render/RendererConfig.h"

namespace SR {

class Scene;
class GPUScene;

/**
 * @brief 渲染器主类，负责整个渲染流程的管理
 */
class SR_API Renderer {
public:
    /** @brief 初始化渲染器分辨率 */
    void Initialize(int width, int height);
    /** @brief 切换 HDR/SDR 渲染模式 */
    void SetHDR(bool enabled);
    /** @brief 设置帧上下文生成选项 */
    void SetFrameContextOptions(const FrameContextOptions& options);
    /** @brief 设置后处理效果开关与参数 */
    void SetPostProcess(bool enableFXAA, bool enableToneMap, double exposure = 1.0);
    /** @brief 应用完整的渲染器配置 */
    void SetConfig(const RendererConfig& config);
    /** @brief 获取当前配置 (只读) */
    const RendererConfig& GetConfig() const;
    /** @brief 渲染传统层级的场景 */
    void Render(const Scene& scene);
    /** @brief 渲染扁平化加速结构的 GPUScene */
    void Render(const GPUScene& scene);
    /** @brief 获取最终输出的 BGRA8 像素缓冲区 */
    const uint32_t* GetFramebuffer() const;
    /** @brief 获取线性空间 HDR 像素缓冲区 */
    const Vec3* GetFramebufferLinear() const;
    /** @brief 获取渲染目标宽度 */
    int GetWidth() const;
    /** @brief 获取渲染目标高度 */
    int GetHeight() const;

private:
    int m_width = 0;
    int m_height = 0;
    bool m_useHDR = false;
    Framebuffer m_framebuffer;
    DepthBuffer m_depthBuffer;
    RendererConfig m_config{};
};

} // namespace SR

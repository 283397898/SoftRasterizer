#pragma once

#include "Pipeline/FrameContext.h"
#include "Core/Framebuffer.h"
#include "Core/DepthBuffer.h"

namespace SR {

/**
 * @brief 渲染 Pass 上下文，包含单次渲染所需的所有资源和配置
 */
struct PassContext {
    FrameContext frame{};            ///< 帧级全局数据 (变换、光照)
    Framebuffer* framebuffer = nullptr; ///< 颜色缓冲目标
    DepthBuffer* depthBuffer = nullptr; ///< 深度缓冲目标
    bool enableFXAA = false;          ///< 是否开启 FXAA 抗锯齿
    bool enableToneMap = true;       ///< 是否开启色调映射 (HDR -> sRGB)
    double exposure = 1.0;            ///< 曝光度
};

} // namespace SR

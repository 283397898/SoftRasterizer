#pragma once

#include <vector>
#include <string>
#include <memory>

namespace SR {

// 前向声明
class Framebuffer;
class DepthBuffer;
class RenderQueue;
struct FrameContext;
struct RenderStats;
struct Triangle;
class MaterialTable;

/**
 * @brief 渲染上下文，包含 Pass 执行所需的所有数据
 */
struct RenderContext {
    Framebuffer* framebuffer = nullptr;
    DepthBuffer* depthBuffer = nullptr;
    const RenderQueue* renderQueue = nullptr;
    const FrameContext* frameContext = nullptr;
    std::vector<Triangle>* deferredBlendTriangles = nullptr;
    MaterialTable* materialTable = nullptr;

    /// 当前 Pass 名称（调试用）
    std::string passName;
};

/**
 * @brief 渲染 Pass 统计数据
 */
struct PassStats {
    double buildMs = 0.0;           ///< 几何构建耗时 (毫秒)
    double rastMs = 0.0;            ///< 光栅化耗时 (毫秒)
    double executeMs = 0.0;         ///< 总执行耗时 (毫秒)
    uint64_t trianglesBuilt = 0;    ///< 构建出的总三角形数
    uint64_t trianglesClipped = 0;  ///< 裁剪后的总三角形数
    uint64_t trianglesRendered = 0; ///< 渲染的三角形数
    uint64_t pixelsTested = 0;      ///< 深度测试总像素数
    uint64_t pixelsShaded = 0;      ///< 最终着色的总像素数
};

/**
 * @brief 渲染 Pass 抽象基类
 *
 * 使用策略模式实现可配置的渲染管线。
 * 每个 Pass 负责渲染管线的一个阶段。
 */
class RenderPass {
public:
    virtual ~RenderPass() = default;

    /**
     * @brief 执行 Pass
     * @param context 渲染上下文
     * @return Pass 统计数据
     */
    virtual PassStats Execute(RenderContext& context) = 0;

    /**
     * @brief 判断 Pass 是否应该执行
     * @param context 渲染上下文
     * @return true 如果 Pass 应该执行
     */
    virtual bool ShouldExecute(const RenderContext& context) const {
        (void)context;
        return true;
    }

    /**
     * @brief 获取 Pass 名称
     * @return Pass 名称字符串
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief 获取 Pass 优先级（用于排序）
     * @return 优先级值（越小越先执行）
     */
    virtual int GetPriority() const { return 100; }

    /**
     * @brief 判断 Pass 是否启用
     * @return true 如果启用
     */
    virtual bool IsEnabled() const { return m_enabled; }

    /**
     * @brief 设置 Pass 启用状态
     * @param enabled 启用状态
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }

protected:
    bool m_enabled = true;
};

} // namespace SR

#pragma once

#include "Pipeline/RenderPass.h"

namespace SR {

/**
 * @brief 不透明几何体渲染 Pass
 *
 * 渲染所有 alphaMode 为 Opaque 或 Mask 的几何体。
 * 使用 Early-Z 优化。
 */
class OpaquePass : public RenderPass {
public:
    PassStats Execute(RenderContext& context) override;

    bool ShouldExecute(const RenderContext& context) const override {
        return context.renderQueue != nullptr;
    }

    std::string GetName() const override {
        return "OpaquePass";
    }

    int GetPriority() const override {
        return 100; // 不透明物体先渲染
    }
};

/**
 * @brief 透明几何体渲染 Pass
 *
 * 渲染所有 alphaMode 为 Blend 的几何体。
 * 使用从后到前的排序进行正确的 alpha 混合。
 */
class TransparentPass : public RenderPass {
public:
    PassStats Execute(RenderContext& context) override;

    bool ShouldExecute(const RenderContext& context) const override {
        return context.renderQueue != nullptr;
    }

    std::string GetName() const override {
        return "TransparentPass";
    }

    int GetPriority() const override {
        return 300; // 透明物体在天空盒之后渲染
    }
};

/**
 * @brief 天空盒渲染 Pass
 *
 * 使用环境贴图渲染天空盒背景。
 * 只渲染深度为远平面的像素。
 */
class SkyboxPass : public RenderPass {
public:
    PassStats Execute(RenderContext& context) override;

    bool ShouldExecute(const RenderContext& context) const override;

    std::string GetName() const override {
        return "SkyboxPass";
    }

    int GetPriority() const override {
        return 200; // 天空盒在不透明物体之后
    }
};

/**
 * @brief 后处理 Pass
 *
 * 执行 FXAA 抗锯齿和色调映射。
 */
class PostProcessPass : public RenderPass {
public:
    PassStats Execute(RenderContext& context) override;

    std::string GetName() const override {
        return "PostProcessPass";
    }

    int GetPriority() const override {
        return 400; // 后处理最后执行
    }

    void SetFXAAEnabled(bool enabled) { m_fxaaEnabled = enabled; }
    void SetToneMappingEnabled(bool enabled) { m_toneMappingEnabled = enabled; }
    void SetExposure(double exposure) { m_exposure = exposure; }

private:
    bool m_fxaaEnabled = true;
    bool m_toneMappingEnabled = true;
    double m_exposure = 1.0;
};

} // namespace SR

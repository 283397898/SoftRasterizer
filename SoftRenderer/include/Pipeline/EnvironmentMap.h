#pragma once

#include <string>
#include <vector>

#include "Core/HDRImage.h"
#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "SoftRendererExport.h"

namespace SR {

/**
 * @brief 环境贴图类，提供 IBL（基于图像的光照）支持
 *
 * 加载 EXR 等距柱形投影环境贴图后，预计算：
 *   - SH L=2 漫反射辐照度系数（9 × 3 通道）
 *   - Split-Sum 预过滤镜面反射 mip 链（6 级）
 *   - BRDF 积分 LUT（128×128）
 */
class SR_API EnvironmentMap {
public:
    /**
     * @brief 从 EXR 文件加载环境贴图并预计算所有 IBL 数据
     * @return 成功返回 true
     */
    bool LoadFromEXR(const std::string& path);

    /** @brief 是否已成功加载 */
    bool IsLoaded() const { return m_loaded; }

    /** @brief 按方向采样原始环境贴图（天空盒背景用） */
    Vec3 SampleDirection(const Vec3& dir) const;

    /** @brief 使用 SH 系数评估漫反射辐照度 */
    Vec3 EvalDiffuseSH(const Vec3& normal) const;

    /** @brief 按反射方向和粗糙度采样预过滤镜面反射贴图 */
    Vec3 SampleSpecular(const Vec3& R, double roughness) const;

    /** @brief 查询 BRDF 积分 LUT，返回 (scale, bias) */
    Vec2 LookupBRDF(double NdotV, double roughness) const;

    /** @brief 获取最近一次错误信息 */
    const std::string& GetLastError() const { return m_lastError; }

private:
    // 等距柱形投影双线性采样
    Vec3 SampleEquirectBilinear(const HDRImage& img, const Vec3& dir) const;

    // 预计算步骤
    void ComputeSH9();
    void ComputePrefilteredSpecular();
    void ComputeBRDFLUT();

    bool m_loaded = false;
    std::string m_lastError;

    // 原始环境贴图
    HDRImage m_envMap;

    // 漫反射 SH L=2 系数（9 个 Vec3）
    Vec3 m_sh[9]{};

    // 预过滤镜面反射 mip 链
    static constexpr int kSpecularMipCount = 6;
    static constexpr double kMipRoughness[kSpecularMipCount] = {0.0, 0.2, 0.4, 0.6, 0.8, 1.0};
    HDRImage m_specularMips[kSpecularMipCount];

    // BRDF 积分 LUT
    static constexpr int kBRDFLutSize = 128;
    std::vector<float> m_brdfLUT; // 128×128，每条目 2 floats (scale, bias)
};

} // namespace SR

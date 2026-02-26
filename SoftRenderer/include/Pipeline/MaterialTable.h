#pragma once

#include <vector>
#include <cstdint>
#include <deque>
#include "Material/PBRMaterial.h"

namespace SR {

/**
 * @brief 材质句柄类型
 */
using MaterialHandle = uint32_t;

/**
 * @brief 无效材质句柄常量
 */
constexpr MaterialHandle InvalidMaterialHandle = UINT32_MAX;

/**
 * @brief 材质参数结构，用于创建新材质
 */
struct MaterialParams {
    // PBR 属性
    Vec3 albedo{1.0, 1.0, 1.0};
    double metallic = 0.0;
    double roughness = 0.5;
    bool doubleSided = false;
    double alpha = 1.0;
    double transmissionFactor = 0.0;
    GLTFAlphaMode alphaMode = GLTFAlphaMode::Opaque;
    double alphaCutoff = 0.5;
    Vec3 emissiveFactor{0.0, 0.0, 0.0};
    double ior = 1.5;
    double specularFactor = 1.0;
    Vec3 specularColorFactor{1.0, 1.0, 1.0};

    // 纹理索引
    int32_t baseColorTextureIndex = -1;
    int32_t metallicRoughnessTextureIndex = -1;
    int32_t normalTextureIndex = -1;
    int32_t occlusionTextureIndex = -1;
    int32_t emissiveTextureIndex = -1;
    int32_t transmissionTextureIndex = -1;

    // 图像索引
    int32_t baseColorImageIndex = -1;
    int32_t metallicRoughnessImageIndex = -1;
    int32_t normalImageIndex = -1;
    int32_t occlusionImageIndex = -1;
    int32_t emissiveImageIndex = -1;
    int32_t transmissionImageIndex = -1;

    // 采样器索引
    int32_t baseColorSamplerIndex = -1;
    int32_t metallicRoughnessSamplerIndex = -1;
    int32_t normalSamplerIndex = -1;
    int32_t occlusionSamplerIndex = -1;
    int32_t emissiveSamplerIndex = -1;
    int32_t transmissionSamplerIndex = -1;

    // 纹理坐标集
    int32_t baseColorTexCoordSet = 0;
    int32_t metallicRoughnessTexCoordSet = 0;
    int32_t normalTexCoordSet = 0;
    int32_t occlusionTexCoordSet = 0;
    int32_t emissiveTexCoordSet = 0;
    int32_t transmissionTexCoordSet = 0;

    // 索引引用
    int32_t meshIndex = -1;
    int32_t materialIndex = -1;
    int32_t primitiveIndex = -1;
    int32_t nodeIndex = -1;
};

/**
 * @brief SOA 布局的材质表 (高性能渲染)
 *
 * 使用 Structure of Arrays (SOA) 布局存储材质数据，优化缓存局部性。
 * 每个 Triangle 仅存储一个 MaterialHandle (uint32_t) 而非完整的材质属性，
 * 大幅减少 Triangle 结构体大小 (从 ~45 字段降至 ~15 字段)。
 *
 * ## 与 MaterialPool 的关系
 * - **MaterialTable**: 渲染时的只读 SOA 存储，提供 O(1) 属性访问
 * - **MaterialPool**: 场景级别的 AOS 存储，管理完整的 PBRMaterial 对象
 *
 * ## 数据流
 * 1. GeometryProcessor 从 PBRMaterial 提取属性，调用 AddMaterial()
 * 2. Triangle.materialId 存储返回的 MaterialHandle
 * 3. FragmentShader 通过 GetXxx(materialId) 方法获取着色数据
 *
 * ## 生命周期
 * MaterialTable 必须在整个渲染帧期间保持有效 (包括延迟的透明物体渲染)。
 * 通常由 RenderPipeline 在 Render() 调用时创建并管理。
 */
class MaterialTable {
public:
    MaterialTable();
    ~MaterialTable() = default;

    // 禁止拷贝
    MaterialTable(const MaterialTable&) = delete;
    MaterialTable& operator=(const MaterialTable&) = delete;

    // 允许移动
    MaterialTable(MaterialTable&&) noexcept = default;
    MaterialTable& operator=(MaterialTable&&) noexcept = default;

    /**
     * @brief 添加新材质
     * @param params 材质参数
     * @return 材质句柄
     */
    MaterialHandle AddMaterial(const MaterialParams& params);

    /**
     * @brief 移除材质
     * @param handle 材质句柄
     */
    void RemoveMaterial(MaterialHandle handle);

    /**
     * @brief 检查材质句柄是否有效
     * @param handle 材质句柄
     * @return 是否有效
     */
    bool IsValid(MaterialHandle handle) const;

    /**
     * @brief 获取默认材质句柄
     * @return 默认材质句柄 (始终为 0)
     */
    static constexpr MaterialHandle GetDefaultMaterialHandle() { return 0; }

    /**
     * @brief 获取材质数量
     */
    size_t GetMaterialCount() const { return m_count - m_freeSlots.size(); }

    // ========== PBR 属性访问 ==========

    const Vec3& GetAlbedo(MaterialHandle handle) const;
    double GetMetallic(MaterialHandle handle) const;
    double GetRoughness(MaterialHandle handle) const;
    bool GetDoubleSided(MaterialHandle handle) const;
    double GetAlpha(MaterialHandle handle) const;
    double GetTransmissionFactor(MaterialHandle handle) const;
    GLTFAlphaMode GetAlphaMode(MaterialHandle handle) const;
    double GetAlphaCutoff(MaterialHandle handle) const;
    const Vec3& GetEmissiveFactor(MaterialHandle handle) const;
    double GetIOR(MaterialHandle handle) const;
    double GetSpecularFactor(MaterialHandle handle) const;
    const Vec3& GetSpecularColorFactor(MaterialHandle handle) const;

    // ========== 纹理索引访问 ==========

    int32_t GetBaseColorTextureIndex(MaterialHandle handle) const;
    int32_t GetMetallicRoughnessTextureIndex(MaterialHandle handle) const;
    int32_t GetNormalTextureIndex(MaterialHandle handle) const;
    int32_t GetOcclusionTextureIndex(MaterialHandle handle) const;
    int32_t GetEmissiveTextureIndex(MaterialHandle handle) const;
    int32_t GetTransmissionTextureIndex(MaterialHandle handle) const;

    // ========== 图像索引访问 ==========

    int32_t GetBaseColorImageIndex(MaterialHandle handle) const;
    int32_t GetMetallicRoughnessImageIndex(MaterialHandle handle) const;
    int32_t GetNormalImageIndex(MaterialHandle handle) const;
    int32_t GetOcclusionImageIndex(MaterialHandle handle) const;
    int32_t GetEmissiveImageIndex(MaterialHandle handle) const;
    int32_t GetTransmissionImageIndex(MaterialHandle handle) const;

    // ========== 采样器索引访问 ==========

    int32_t GetBaseColorSamplerIndex(MaterialHandle handle) const;
    int32_t GetMetallicRoughnessSamplerIndex(MaterialHandle handle) const;
    int32_t GetNormalSamplerIndex(MaterialHandle handle) const;
    int32_t GetOcclusionSamplerIndex(MaterialHandle handle) const;
    int32_t GetEmissiveSamplerIndex(MaterialHandle handle) const;
    int32_t GetTransmissionSamplerIndex(MaterialHandle handle) const;

    // ========== 纹理坐标集访问 ==========

    int32_t GetBaseColorTexCoordSet(MaterialHandle handle) const;
    int32_t GetMetallicRoughnessTexCoordSet(MaterialHandle handle) const;
    int32_t GetNormalTexCoordSet(MaterialHandle handle) const;
    int32_t GetOcclusionTexCoordSet(MaterialHandle handle) const;
    int32_t GetEmissiveTexCoordSet(MaterialHandle handle) const;
    int32_t GetTransmissionTexCoordSet(MaterialHandle handle) const;

    // ========== 索引引用访问 ==========

    int32_t GetMeshIndex(MaterialHandle handle) const;
    int32_t GetMaterialIndex(MaterialHandle handle) const;
    int32_t GetPrimitiveIndex(MaterialHandle handle) const;
    int32_t GetNodeIndex(MaterialHandle handle) const;

    /**
     * @brief 获取完整的 PBRMaterial 结构 (用于兼容)
     * @param handle 材质句柄
     * @return PBRMaterial 结构
     */
    PBRMaterial GetPBRMaterial(MaterialHandle handle) const;

    /**
     * @brief 清空所有材质
     */
    void Clear();

private:
    void InitializeDefaultMaterial();

    template<typename T>
    const T& GetProperty(MaterialHandle handle, const std::vector<T>& storage, const T& defaultValue) const {
        if (!IsValid(handle)) {
            return defaultValue;
        }
        return storage[handle];
    }

    // 有效性标记
    std::vector<bool> m_valid;

    // PBR 属性 (SOA)
    std::vector<Vec3> m_albedo;
    std::vector<double> m_metallic;
    std::vector<double> m_roughness;
    std::vector<uint8_t> m_doubleSided;
    std::vector<double> m_alpha;
    std::vector<double> m_transmissionFactor;
    std::vector<GLTFAlphaMode> m_alphaMode;
    std::vector<double> m_alphaCutoff;
    std::vector<Vec3> m_emissiveFactor;
    std::vector<double> m_ior;
    std::vector<double> m_specularFactor;
    std::vector<Vec3> m_specularColorFactor;

    // 纹理索引 (SOA)
    std::vector<int32_t> m_baseColorTextureIndex;
    std::vector<int32_t> m_metallicRoughnessTextureIndex;
    std::vector<int32_t> m_normalTextureIndex;
    std::vector<int32_t> m_occlusionTextureIndex;
    std::vector<int32_t> m_emissiveTextureIndex;
    std::vector<int32_t> m_transmissionTextureIndex;

    // 图像索引 (SOA)
    std::vector<int32_t> m_baseColorImageIndex;
    std::vector<int32_t> m_metallicRoughnessImageIndex;
    std::vector<int32_t> m_normalImageIndex;
    std::vector<int32_t> m_occlusionImageIndex;
    std::vector<int32_t> m_emissiveImageIndex;
    std::vector<int32_t> m_transmissionImageIndex;

    // 采样器索引 (SOA)
    std::vector<int32_t> m_baseColorSamplerIndex;
    std::vector<int32_t> m_metallicRoughnessSamplerIndex;
    std::vector<int32_t> m_normalSamplerIndex;
    std::vector<int32_t> m_occlusionSamplerIndex;
    std::vector<int32_t> m_emissiveSamplerIndex;
    std::vector<int32_t> m_transmissionSamplerIndex;

    // 纹理坐标集 (SOA)
    std::vector<int32_t> m_baseColorTexCoordSet;
    std::vector<int32_t> m_metallicRoughnessTexCoordSet;
    std::vector<int32_t> m_normalTexCoordSet;
    std::vector<int32_t> m_occlusionTexCoordSet;
    std::vector<int32_t> m_emissiveTexCoordSet;
    std::vector<int32_t> m_transmissionTexCoordSet;

    // 索引引用 (SOA)
    std::vector<int32_t> m_meshIndex;
    std::vector<int32_t> m_materialIndex;
    std::vector<int32_t> m_primitiveIndex;
    std::vector<int32_t> m_nodeIndex;

    // 空闲槽位列表
    std::deque<MaterialHandle> m_freeSlots;

    // 当前材质数量
    size_t m_count = 0;
};

} // namespace SR

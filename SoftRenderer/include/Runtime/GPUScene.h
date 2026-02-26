#pragma once

#include <unordered_map>
#include <vector>

#include "SoftRendererExport.h"
#include "Asset/GLTFAsset.h"
#include "Material/PBRMaterial.h"
#include "Math/Mat4.h"
#include "Scene/Mesh.h"
#include "Runtime/MeshPool.h"
#include "Runtime/MaterialPool.h"

namespace SR {

struct GPUSceneDrawItem {
    const Mesh* mesh = nullptr;
    const PBRMaterial* material = nullptr;
    Mat4 modelMatrix = Mat4::Identity();
    Mat4 normalMatrix = Mat4::Identity();
    int meshIndex = -1;
    int materialIndex = -1;
    int primitiveIndex = -1;
    int nodeIndex = -1;
    int baseColorTextureIndex = -1;
    int metallicRoughnessTextureIndex = -1;
    int normalTextureIndex = -1;
    int occlusionTextureIndex = -1;
    int emissiveTextureIndex = -1;
    int transmissionTextureIndex = -1;
    int baseColorImageIndex = -1;
    int metallicRoughnessImageIndex = -1;
    int normalImageIndex = -1;
    int occlusionImageIndex = -1;
    int emissiveImageIndex = -1;
    int transmissionImageIndex = -1;
    int baseColorSamplerIndex = -1;
    int metallicRoughnessSamplerIndex = -1;
    int normalSamplerIndex = -1;
    int occlusionSamplerIndex = -1;
    int emissiveSamplerIndex = -1;
    int transmissionSamplerIndex = -1;
    int baseColorTexCoordSet = 0;
    int metallicRoughnessTexCoordSet = 0;
    int normalTexCoordSet = 0;
    int occlusionTexCoordSet = 0;
    int emissiveTexCoordSet = 0;
    int transmissionTexCoordSet = 0;
};

/**
 * @brief 扁平化的渲染场景，由 DrawItem 列表组成
 *
 * 支持两种模式：
 * 1. 传统模式：使用 std::vector 存储资源
 * 2. 池化模式：使用 ResourcePool 管理资源（未来默认）
 *
 * ## 材质管理架构
 * GPUScene 同时持有两种材质存储方式：
 *
 * - **m_materials (vector<PBRMaterial>)**: 扁平化的材质数据
 * - **m_materialPool (MaterialPool)**: 池化的材质对象管理 (AOS 布局)
 *
 * 渲染时，GeometryProcessor 从这些存储中提取属性，
 * 添加到帧级别的 MaterialTable (SOA 布局) 供 FragmentShader 使用。
 *
 * @see MaterialPool 场景级别的 AOS 材质存储
 * @see MaterialTable 帧级别的 SOA 材质存储 (渲染时使用)
 */
class SR_API GPUScene {
public:
    /** @brief 预留空间 */
    void Reserve(size_t count);
    /** @brief 添加一个渲染项 */
    void AddDrawable(const GPUSceneDrawItem& item);
    /** @brief 直接设置所有渲染项 */
    void SetItems(std::vector<GPUSceneDrawItem>&& items);
    /** @brief 清空场景内容 */
    void Clear();
    /** @brief 获取所有渲染项列表 */
    const std::vector<GPUSceneDrawItem>& GetItems() const;
    /** @brief 从 glTF 资产构建场景 */
    void Build(const GLTFAsset& asset, int sceneIndex);
    /** @brief 获取场景相关的贴图列表 */
    const std::vector<GLTFImage>& GetImages() const;
    /** @brief 获取场景相关的采样器列表 */
    const std::vector<GLTFSampler>& GetSamplers() const;

    // ========== ResourcePool 集成 API (未来默认) ==========

    /** @brief 获取网格池 */
    MeshPool& GetMeshPool() { return m_meshPool; }
    const MeshPool& GetMeshPool() const { return m_meshPool; }

    /** @brief 获取材质池 */
    MaterialPool& GetMaterialPool() { return m_materialPool; }
    const MaterialPool& GetMaterialPool() const { return m_materialPool; }

    /** @brief 设置资源内存预算 */
    void SetMemoryBudget(size_t meshBytes, size_t materialBytes);

    /** @brief 执行 LRU 淘汰 */
    void EvictResources();

    /** @brief 获取总资源内存使用量 */
    size_t GetTotalMemoryUsage() const;

private:
    std::vector<GPUSceneDrawItem> m_items;
    std::vector<Mesh> m_ownedMeshes;
    std::vector<PBRMaterial> m_ownedMaterials;
    std::vector<GLTFImage> m_ownedImages;
    std::vector<GLTFSampler> m_ownedSamplers;

    // ResourcePool 成员（用于未来池化模式）
    MeshPool m_meshPool;
    MaterialPool m_materialPool;
};

} // namespace SR

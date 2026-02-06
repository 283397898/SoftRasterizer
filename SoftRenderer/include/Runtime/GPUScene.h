#pragma once

#include <unordered_map>
#include <vector>

#include "SoftRendererExport.h"
#include "Asset/GLTFAsset.h"
#include "Material/PBRMaterial.h"
#include "Math/Mat4.h"
#include "Scene/Mesh.h"

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
    int baseColorImageIndex = -1;
    int metallicRoughnessImageIndex = -1;
    int normalImageIndex = -1;
    int occlusionImageIndex = -1;
    int emissiveImageIndex = -1;
    int baseColorSamplerIndex = -1;
    int metallicRoughnessSamplerIndex = -1;
    int normalSamplerIndex = -1;
    int occlusionSamplerIndex = -1;
    int emissiveSamplerIndex = -1;
};

/**
 * @brief 扁平化的渲染场景，由 DrawItem 列表组成
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

private:
    std::vector<GPUSceneDrawItem> m_items;
    std::vector<Mesh> m_ownedMeshes;
    std::vector<PBRMaterial> m_ownedMaterials;
    std::vector<GLTFImage> m_ownedImages;
    std::vector<GLTFSampler> m_ownedSamplers;
};

} // namespace SR

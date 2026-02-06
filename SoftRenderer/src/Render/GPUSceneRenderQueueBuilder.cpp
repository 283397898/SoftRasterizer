#include "Render/GPUSceneRenderQueueBuilder.h"

#include "Runtime/GPUScene.h"
#include <algorithm>

namespace SR {

/**
 * @brief 从 GPUScene 扁平化数据构建渲染队列，并进行简单的排序优化
 * @param scene 运行时场景数据
 * @param outQueue 输出的渲染队列
 */
void GPUSceneRenderQueueBuilder::Build(const GPUScene& scene, RenderQueue& outQueue) const {
    std::vector<DrawItem> items;
    const auto& sceneItems = scene.GetItems();
    items.reserve(sceneItems.size());

    // 将场景中的 DrawItem 转换为渲染管线所用的格式
    for (const GPUSceneDrawItem& sceneItem : sceneItems) {
        DrawItem item{};
        item.mesh = sceneItem.mesh;
        item.material = sceneItem.material;
        item.modelMatrix = sceneItem.modelMatrix;
        item.normalMatrix = sceneItem.normalMatrix;
        item.meshIndex = sceneItem.meshIndex;
        item.materialIndex = sceneItem.materialIndex;
        item.primitiveIndex = sceneItem.primitiveIndex;
        item.nodeIndex = sceneItem.nodeIndex;
        item.baseColorTextureIndex = sceneItem.baseColorTextureIndex;
        item.metallicRoughnessTextureIndex = sceneItem.metallicRoughnessTextureIndex;
        item.normalTextureIndex = sceneItem.normalTextureIndex;
        item.occlusionTextureIndex = sceneItem.occlusionTextureIndex;
        item.emissiveTextureIndex = sceneItem.emissiveTextureIndex;
        item.baseColorImageIndex = sceneItem.baseColorImageIndex;
        item.metallicRoughnessImageIndex = sceneItem.metallicRoughnessImageIndex;
        item.normalImageIndex = sceneItem.normalImageIndex;
        item.occlusionImageIndex = sceneItem.occlusionImageIndex;
        item.emissiveImageIndex = sceneItem.emissiveImageIndex;
        item.baseColorSamplerIndex = sceneItem.baseColorSamplerIndex;
        item.metallicRoughnessSamplerIndex = sceneItem.metallicRoughnessSamplerIndex;
        item.normalSamplerIndex = sceneItem.normalSamplerIndex;
        item.occlusionSamplerIndex = sceneItem.occlusionSamplerIndex;
        item.emissiveSamplerIndex = sceneItem.emissiveSamplerIndex;
        items.push_back(item);
    }

    // 按材质和网格排序，有助于减少渲染状态切换 (虽然当前是软光栅，但逻辑一致)
    std::stable_sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) {
        if (a.material != b.material) {
            return a.material < b.material;
        }
        return a.mesh < b.mesh;
    });

    outQueue.SetItems(std::move(items));
}

} // namespace SR

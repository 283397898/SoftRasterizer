#include "Render/GPUSceneRenderQueueBuilder.h"

#include "Runtime/GPUScene.h"
#include <algorithm>

namespace SR {

/**
 * @brief Build render queue from GPUScene, with sorting optimization
 * @param scene Runtime scene data
 * @param outQueue Output render queue
 */
void GPUSceneRenderQueueBuilder::Build(const GPUScene& scene, RenderQueue& outQueue) const {
    std::vector<DrawItem> items;
    const auto& sceneItems = scene.GetItems();
    items.reserve(sceneItems.size());

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
        item.transmissionTextureIndex = sceneItem.transmissionTextureIndex;
        item.baseColorImageIndex = sceneItem.baseColorImageIndex;
        item.metallicRoughnessImageIndex = sceneItem.metallicRoughnessImageIndex;
        item.normalImageIndex = sceneItem.normalImageIndex;
        item.occlusionImageIndex = sceneItem.occlusionImageIndex;
        item.emissiveImageIndex = sceneItem.emissiveImageIndex;
        item.transmissionImageIndex = sceneItem.transmissionImageIndex;
        item.baseColorSamplerIndex = sceneItem.baseColorSamplerIndex;
        item.metallicRoughnessSamplerIndex = sceneItem.metallicRoughnessSamplerIndex;
        item.normalSamplerIndex = sceneItem.normalSamplerIndex;
        item.occlusionSamplerIndex = sceneItem.occlusionSamplerIndex;
        item.emissiveSamplerIndex = sceneItem.emissiveSamplerIndex;
        item.transmissionSamplerIndex = sceneItem.transmissionSamplerIndex;
        item.baseColorTexCoordSet = sceneItem.baseColorTexCoordSet;
        item.metallicRoughnessTexCoordSet = sceneItem.metallicRoughnessTexCoordSet;
        item.normalTexCoordSet = sceneItem.normalTexCoordSet;
        item.occlusionTexCoordSet = sceneItem.occlusionTexCoordSet;
        item.emissiveTexCoordSet = sceneItem.emissiveTexCoordSet;
        item.transmissionTexCoordSet = sceneItem.transmissionTexCoordSet;
        items.push_back(item);
    }

    // Sort by alphaMode first (Opaque=0, Mask=1, Blend=2), then by material/mesh.
    // This ensures opaque objects render first and populate the depth buffer,
    // so translucent objects can correctly blend with the colors behind them.
    std::stable_sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) {
        int alphaModeA = a.material ? a.material->alphaMode : 0;
        int alphaModeB = b.material ? b.material->alphaMode : 0;
        if (alphaModeA != alphaModeB) {
            return alphaModeA < alphaModeB;
        }
        if (a.material != b.material) {
            return a.material < b.material;
        }
        return a.mesh < b.mesh;
    });

    outQueue.SetItems(std::move(items));
}

} // namespace SR

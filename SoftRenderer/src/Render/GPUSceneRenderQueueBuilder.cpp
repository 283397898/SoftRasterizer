#include "Render/GPUSceneRenderQueueBuilder.h"

#include "Runtime/GPUScene.h"
#include <algorithm>

namespace SR {

/**
 * @brief 从 GPUScene 构建渲染队列（含排序优化）
 * @param scene            运行时场景数据
 * @param outQueue         输出的渲染队列
 * @param onlyMaterialIndex 若 >= 0，则仅包含指定材质索引的绘制项（调试用）
 */
void GPUSceneRenderQueueBuilder::Build(const GPUScene& scene, RenderQueue& outQueue, int onlyMaterialIndex) const {
    std::vector<DrawItem> items;
    const auto& sceneItems = scene.GetItems();
    items.reserve(sceneItems.size());

    for (const GPUSceneDrawItem& sceneItem : sceneItems) {
        if (onlyMaterialIndex >= 0 && sceneItem.materialIndex != onlyMaterialIndex) {
            continue;
        }

        DrawItem item{};
        item.mesh = sceneItem.mesh;
        item.material = sceneItem.material;
        item.modelMatrix = sceneItem.modelMatrix;
        item.normalMatrix = sceneItem.normalMatrix;
        item.meshIndex = sceneItem.meshIndex;
        item.materialIndex = sceneItem.materialIndex;
        item.primitiveIndex = sceneItem.primitiveIndex;
        item.nodeIndex = sceneItem.nodeIndex;
        item.textures = sceneItem.textures;
        items.push_back(item);
    }

    // 先按 alphaMode 排序（Opaque=0 < Mask=1 < Blend=2），再按材质/网格分组。
    // 确保不透明物体先渲染并填充深度缓冲区，半透明物体随后才能正确地与背景混合。
    std::stable_sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) {
        GLTFAlphaMode alphaModeA = a.material ? a.material->alphaMode : GLTFAlphaMode::Opaque;
        GLTFAlphaMode alphaModeB = b.material ? b.material->alphaMode : GLTFAlphaMode::Opaque;
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

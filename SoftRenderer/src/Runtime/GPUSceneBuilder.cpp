#include "Runtime/GPUSceneBuilder.h"

#include "Scene/ObjectGroup.h"
#include "Scene/Model.h"

namespace SR {

void GPUSceneBuilder::BuildFromObjectGroup(const ObjectGroup& objects, GPUScene& outScene) const {
    outScene.Clear();

    const auto& models = objects.GetModels();
    outScene.Reserve(models.size());

    const PBRMaterial* lastMaterial = nullptr;

    for (const Model& model : models) {
        Mesh* mesh = model.GetMesh();
        if (!mesh) {
            continue;
        }

        const PBRMaterial* material = &model.GetMaterial();
        // 简单材质复用缓存（仅指针比较，减少重复状态构建开销）
        if (material == lastMaterial) {
            material = lastMaterial;
        }

        GPUSceneDrawItem item{};
        item.mesh = mesh;
        item.material = material;
        item.modelMatrix = model.GetTransform().GetMatrix();
        item.normalMatrix = model.GetTransform().GetNormalMatrix();
        item.meshIndex = -1;
        item.materialIndex = -1;
        item.primitiveIndex = -1;
        item.nodeIndex = -1;
        outScene.AddDrawable(item);

        lastMaterial = material;
    }
}

} // namespace SR

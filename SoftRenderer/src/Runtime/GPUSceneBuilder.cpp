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
        item.baseColorTextureIndex = -1;
        item.metallicRoughnessTextureIndex = -1;
        item.normalTextureIndex = -1;
        item.occlusionTextureIndex = -1;
        item.emissiveTextureIndex = -1;
        item.baseColorImageIndex = -1;
        item.metallicRoughnessImageIndex = -1;
        item.normalImageIndex = -1;
        item.occlusionImageIndex = -1;
        item.emissiveImageIndex = -1;
        item.baseColorSamplerIndex = -1;
        item.metallicRoughnessSamplerIndex = -1;
        item.normalSamplerIndex = -1;
        item.occlusionSamplerIndex = -1;
        item.emissiveSamplerIndex = -1;
        outScene.AddDrawable(item);

        lastMaterial = material;
    }
}

} // namespace SR

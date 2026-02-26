#include "Pipeline/MaterialTable.h"

namespace SR {

MaterialTable::MaterialTable() {
    InitializeDefaultMaterial();
}

void MaterialTable::InitializeDefaultMaterial() {
    // 插槽 0 保留给默认材质（所有未指定材质的三角形使用此句柄）
    m_valid.push_back(true);
    m_albedo.emplace_back(1.0, 1.0, 1.0);
    m_metallic.push_back(0.0);
    m_roughness.push_back(0.5);
    m_doubleSided.push_back(false);
    m_alpha.push_back(1.0);
    m_transmissionFactor.push_back(0.0);
    m_alphaMode.push_back(GLTFAlphaMode::Opaque);
    m_alphaCutoff.push_back(0.5);
    m_emissiveFactor.emplace_back(0.0, 0.0, 0.0);
    m_ior.push_back(1.5);
    m_specularFactor.push_back(1.0);
    m_specularColorFactor.emplace_back(1.0, 1.0, 1.0);

    // 纹理索引（默认全部无纹理）
    m_baseColorTextureIndex.push_back(-1);
    m_metallicRoughnessTextureIndex.push_back(-1);
    m_normalTextureIndex.push_back(-1);
    m_occlusionTextureIndex.push_back(-1);
    m_emissiveTextureIndex.push_back(-1);
    m_transmissionTextureIndex.push_back(-1);

    // 图像索引
    m_baseColorImageIndex.push_back(-1);
    m_metallicRoughnessImageIndex.push_back(-1);
    m_normalImageIndex.push_back(-1);
    m_occlusionImageIndex.push_back(-1);
    m_emissiveImageIndex.push_back(-1);
    m_transmissionImageIndex.push_back(-1);

    // 采样器索引
    m_baseColorSamplerIndex.push_back(-1);
    m_metallicRoughnessSamplerIndex.push_back(-1);
    m_normalSamplerIndex.push_back(-1);
    m_occlusionSamplerIndex.push_back(-1);
    m_emissiveSamplerIndex.push_back(-1);
    m_transmissionSamplerIndex.push_back(-1);

    // UV 坐标集（默认使用主 UV 集 0）
    m_baseColorTexCoordSet.push_back(0);
    m_metallicRoughnessTexCoordSet.push_back(0);
    m_normalTexCoordSet.push_back(0);
    m_occlusionTexCoordSet.push_back(0);
    m_emissiveTexCoordSet.push_back(0);
    m_transmissionTexCoordSet.push_back(0);

    // 索引引用（默认均无效）
    m_meshIndex.push_back(-1);
    m_materialIndex.push_back(-1);
    m_primitiveIndex.push_back(-1);
    m_nodeIndex.push_back(-1);

    m_count = 1;
}

MaterialHandle MaterialTable::AddMaterial(const MaterialParams& params) {
    MaterialHandle handle;

    // 优先复用空闲插槽（避免数组无限扩张）
    if (!m_freeSlots.empty()) {
        handle = m_freeSlots.front();
        m_freeSlots.pop_front();
    } else {
        handle = static_cast<MaterialHandle>(m_count);
        m_count++;

        // 向所有 SOA 数组追加新插槽
        m_valid.push_back(true);

        // PBR 属性
        m_albedo.push_back(params.albedo);
        m_metallic.push_back(params.metallic);
        m_roughness.push_back(params.roughness);
        m_doubleSided.push_back(params.doubleSided);
        m_alpha.push_back(params.alpha);
        m_transmissionFactor.push_back(params.transmissionFactor);
        m_alphaMode.push_back(params.alphaMode);
        m_alphaCutoff.push_back(params.alphaCutoff);
        m_emissiveFactor.push_back(params.emissiveFactor);
        m_ior.push_back(params.ior);
        m_specularFactor.push_back(params.specularFactor);
        m_specularColorFactor.push_back(params.specularColorFactor);

        // 纹理索引
        m_baseColorTextureIndex.push_back(params.baseColorTextureIndex);
        m_metallicRoughnessTextureIndex.push_back(params.metallicRoughnessTextureIndex);
        m_normalTextureIndex.push_back(params.normalTextureIndex);
        m_occlusionTextureIndex.push_back(params.occlusionTextureIndex);
        m_emissiveTextureIndex.push_back(params.emissiveTextureIndex);
        m_transmissionTextureIndex.push_back(params.transmissionTextureIndex);

        // 图像索引
        m_baseColorImageIndex.push_back(params.baseColorImageIndex);
        m_metallicRoughnessImageIndex.push_back(params.metallicRoughnessImageIndex);
        m_normalImageIndex.push_back(params.normalImageIndex);
        m_occlusionImageIndex.push_back(params.occlusionImageIndex);
        m_emissiveImageIndex.push_back(params.emissiveImageIndex);
        m_transmissionImageIndex.push_back(params.transmissionImageIndex);

        // 采样器索引
        m_baseColorSamplerIndex.push_back(params.baseColorSamplerIndex);
        m_metallicRoughnessSamplerIndex.push_back(params.metallicRoughnessSamplerIndex);
        m_normalSamplerIndex.push_back(params.normalSamplerIndex);
        m_occlusionSamplerIndex.push_back(params.occlusionSamplerIndex);
        m_emissiveSamplerIndex.push_back(params.emissiveSamplerIndex);
        m_transmissionSamplerIndex.push_back(params.transmissionSamplerIndex);

        // UV 坐标集
        m_baseColorTexCoordSet.push_back(params.baseColorTexCoordSet);
        m_metallicRoughnessTexCoordSet.push_back(params.metallicRoughnessTexCoordSet);
        m_normalTexCoordSet.push_back(params.normalTexCoordSet);
        m_occlusionTexCoordSet.push_back(params.occlusionTexCoordSet);
        m_emissiveTexCoordSet.push_back(params.emissiveTexCoordSet);
        m_transmissionTexCoordSet.push_back(params.transmissionTexCoordSet);

        // 索引引用
        m_meshIndex.push_back(params.meshIndex);
        m_materialIndex.push_back(params.materialIndex);
        m_primitiveIndex.push_back(params.primitiveIndex);
        m_nodeIndex.push_back(params.nodeIndex);

        return handle;
    }

    // 复用已有插槽：更新对应位置的所有 SOA 数组
    m_valid[handle] = true;

    // PBR 属性
    m_albedo[handle] = params.albedo;
    m_metallic[handle] = params.metallic;
    m_roughness[handle] = params.roughness;
    m_doubleSided[handle] = params.doubleSided;
    m_alpha[handle] = params.alpha;
    m_transmissionFactor[handle] = params.transmissionFactor;
    m_alphaMode[handle] = params.alphaMode;
    m_alphaCutoff[handle] = params.alphaCutoff;
    m_emissiveFactor[handle] = params.emissiveFactor;
    m_ior[handle] = params.ior;
    m_specularFactor[handle] = params.specularFactor;
    m_specularColorFactor[handle] = params.specularColorFactor;

    // 纹理索引
    m_baseColorTextureIndex[handle] = params.baseColorTextureIndex;
    m_metallicRoughnessTextureIndex[handle] = params.metallicRoughnessTextureIndex;
    m_normalTextureIndex[handle] = params.normalTextureIndex;
    m_occlusionTextureIndex[handle] = params.occlusionTextureIndex;
    m_emissiveTextureIndex[handle] = params.emissiveTextureIndex;
    m_transmissionTextureIndex[handle] = params.transmissionTextureIndex;

    // 图像索引
    m_baseColorImageIndex[handle] = params.baseColorImageIndex;
    m_metallicRoughnessImageIndex[handle] = params.metallicRoughnessImageIndex;
    m_normalImageIndex[handle] = params.normalImageIndex;
    m_occlusionImageIndex[handle] = params.occlusionImageIndex;
    m_emissiveImageIndex[handle] = params.emissiveImageIndex;
    m_transmissionImageIndex[handle] = params.transmissionImageIndex;

    // 采样器索引
    m_baseColorSamplerIndex[handle] = params.baseColorSamplerIndex;
    m_metallicRoughnessSamplerIndex[handle] = params.metallicRoughnessSamplerIndex;
    m_normalSamplerIndex[handle] = params.normalSamplerIndex;
    m_occlusionSamplerIndex[handle] = params.occlusionSamplerIndex;
    m_emissiveSamplerIndex[handle] = params.emissiveSamplerIndex;
    m_transmissionSamplerIndex[handle] = params.transmissionSamplerIndex;

    // UV 坐标集
    m_baseColorTexCoordSet[handle] = params.baseColorTexCoordSet;
    m_metallicRoughnessTexCoordSet[handle] = params.metallicRoughnessTexCoordSet;
    m_normalTexCoordSet[handle] = params.normalTexCoordSet;
    m_occlusionTexCoordSet[handle] = params.occlusionTexCoordSet;
    m_emissiveTexCoordSet[handle] = params.emissiveTexCoordSet;
    m_transmissionTexCoordSet[handle] = params.transmissionTexCoordSet;

    // 索引引用
    m_meshIndex[handle] = params.meshIndex;
    m_materialIndex[handle] = params.materialIndex;
    m_primitiveIndex[handle] = params.primitiveIndex;
    m_nodeIndex[handle] = params.nodeIndex;

    return handle;
}

void MaterialTable::RemoveMaterial(MaterialHandle handle) {
    if (!IsValid(handle)) {
        return;
    }

    // 默认材质（句柄 0）不可被移除
    if (handle == 0) {
        return;
    }

    m_valid[handle] = false;
    m_freeSlots.push_back(handle);
}

bool MaterialTable::IsValid(MaterialHandle handle) const {
    if (handle == InvalidMaterialHandle || handle >= m_count) {
        return false;
    }
    return m_valid[handle];
}

// ========== PBR 属性访问 ==========

const Vec3& MaterialTable::GetAlbedo(MaterialHandle handle) const {
    static const Vec3 defaultAlbedo{1.0, 1.0, 1.0};
    return GetProperty(handle, m_albedo, defaultAlbedo);
}

double MaterialTable::GetMetallic(MaterialHandle handle) const {
    static const double defaultMetallic = 0.0;
    return GetProperty(handle, m_metallic, defaultMetallic);
}

double MaterialTable::GetRoughness(MaterialHandle handle) const {
    static const double defaultRoughness = 0.5;
    return GetProperty(handle, m_roughness, defaultRoughness);
}

bool MaterialTable::GetDoubleSided(MaterialHandle handle) const {
    static const uint8_t defaultDoubleSided = 0;
    return GetProperty(handle, m_doubleSided, defaultDoubleSided) != 0;
}

double MaterialTable::GetAlpha(MaterialHandle handle) const {
    static const double defaultAlpha = 1.0;
    return GetProperty(handle, m_alpha, defaultAlpha);
}

double MaterialTable::GetTransmissionFactor(MaterialHandle handle) const {
    static const double defaultTransmission = 0.0;
    return GetProperty(handle, m_transmissionFactor, defaultTransmission);
}

GLTFAlphaMode MaterialTable::GetAlphaMode(MaterialHandle handle) const {
    static const GLTFAlphaMode defaultAlphaMode = GLTFAlphaMode::Opaque;
    return GetProperty(handle, m_alphaMode, defaultAlphaMode);
}

double MaterialTable::GetAlphaCutoff(MaterialHandle handle) const {
    static const double defaultAlphaCutoff = 0.5;
    return GetProperty(handle, m_alphaCutoff, defaultAlphaCutoff);
}

const Vec3& MaterialTable::GetEmissiveFactor(MaterialHandle handle) const {
    static const Vec3 defaultEmissive{0.0, 0.0, 0.0};
    return GetProperty(handle, m_emissiveFactor, defaultEmissive);
}

double MaterialTable::GetIOR(MaterialHandle handle) const {
    static const double defaultIOR = 1.5;
    return GetProperty(handle, m_ior, defaultIOR);
}

double MaterialTable::GetSpecularFactor(MaterialHandle handle) const {
    static const double defaultSpecularFactor = 1.0;
    return GetProperty(handle, m_specularFactor, defaultSpecularFactor);
}

const Vec3& MaterialTable::GetSpecularColorFactor(MaterialHandle handle) const {
    static const Vec3 defaultSpecular{1.0, 1.0, 1.0};
    return GetProperty(handle, m_specularColorFactor, defaultSpecular);
}

// ========== 纹理索引访问 ==========

int32_t MaterialTable::GetBaseColorTextureIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_baseColorTextureIndex, defaultIndex);
}

int32_t MaterialTable::GetMetallicRoughnessTextureIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_metallicRoughnessTextureIndex, defaultIndex);
}

int32_t MaterialTable::GetNormalTextureIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_normalTextureIndex, defaultIndex);
}

int32_t MaterialTable::GetOcclusionTextureIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_occlusionTextureIndex, defaultIndex);
}

int32_t MaterialTable::GetEmissiveTextureIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_emissiveTextureIndex, defaultIndex);
}

int32_t MaterialTable::GetTransmissionTextureIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_transmissionTextureIndex, defaultIndex);
}

// ========== 图像索引访问 ==========

int32_t MaterialTable::GetBaseColorImageIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_baseColorImageIndex, defaultIndex);
}

int32_t MaterialTable::GetMetallicRoughnessImageIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_metallicRoughnessImageIndex, defaultIndex);
}

int32_t MaterialTable::GetNormalImageIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_normalImageIndex, defaultIndex);
}

int32_t MaterialTable::GetOcclusionImageIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_occlusionImageIndex, defaultIndex);
}

int32_t MaterialTable::GetEmissiveImageIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_emissiveImageIndex, defaultIndex);
}

int32_t MaterialTable::GetTransmissionImageIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_transmissionImageIndex, defaultIndex);
}

// ========== 采样器索引访问 ==========

int32_t MaterialTable::GetBaseColorSamplerIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_baseColorSamplerIndex, defaultIndex);
}

int32_t MaterialTable::GetMetallicRoughnessSamplerIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_metallicRoughnessSamplerIndex, defaultIndex);
}

int32_t MaterialTable::GetNormalSamplerIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_normalSamplerIndex, defaultIndex);
}

int32_t MaterialTable::GetOcclusionSamplerIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_occlusionSamplerIndex, defaultIndex);
}

int32_t MaterialTable::GetEmissiveSamplerIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_emissiveSamplerIndex, defaultIndex);
}

int32_t MaterialTable::GetTransmissionSamplerIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_transmissionSamplerIndex, defaultIndex);
}

// ========== 纹理坐标集访问 ==========

int32_t MaterialTable::GetBaseColorTexCoordSet(MaterialHandle handle) const {
    static const int32_t defaultTexCoordSet = 0;
    return GetProperty(handle, m_baseColorTexCoordSet, defaultTexCoordSet);
}

int32_t MaterialTable::GetMetallicRoughnessTexCoordSet(MaterialHandle handle) const {
    static const int32_t defaultTexCoordSet = 0;
    return GetProperty(handle, m_metallicRoughnessTexCoordSet, defaultTexCoordSet);
}

int32_t MaterialTable::GetNormalTexCoordSet(MaterialHandle handle) const {
    static const int32_t defaultTexCoordSet = 0;
    return GetProperty(handle, m_normalTexCoordSet, defaultTexCoordSet);
}

int32_t MaterialTable::GetOcclusionTexCoordSet(MaterialHandle handle) const {
    static const int32_t defaultTexCoordSet = 0;
    return GetProperty(handle, m_occlusionTexCoordSet, defaultTexCoordSet);
}

int32_t MaterialTable::GetEmissiveTexCoordSet(MaterialHandle handle) const {
    static const int32_t defaultTexCoordSet = 0;
    return GetProperty(handle, m_emissiveTexCoordSet, defaultTexCoordSet);
}

int32_t MaterialTable::GetTransmissionTexCoordSet(MaterialHandle handle) const {
    static const int32_t defaultTexCoordSet = 0;
    return GetProperty(handle, m_transmissionTexCoordSet, defaultTexCoordSet);
}

// ========== 索引引用访问 ==========

int32_t MaterialTable::GetMeshIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_meshIndex, defaultIndex);
}

int32_t MaterialTable::GetMaterialIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_materialIndex, defaultIndex);
}

int32_t MaterialTable::GetPrimitiveIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_primitiveIndex, defaultIndex);
}

int32_t MaterialTable::GetNodeIndex(MaterialHandle handle) const {
    static const int32_t defaultIndex = -1;
    return GetProperty(handle, m_nodeIndex, defaultIndex);
}

PBRMaterial MaterialTable::GetPBRMaterial(MaterialHandle handle) const {
    PBRMaterial mat;
    if (!IsValid(handle)) {
        return mat;
    }

    // 仅复制基础 PBR 属性（PBRMaterial 结构体不含纹理索引字段）
    mat.albedo = m_albedo[handle];
    mat.metallic = m_metallic[handle];
    mat.roughness = m_roughness[handle];
    mat.doubleSided = m_doubleSided[handle];
    mat.alpha = m_alpha[handle];
    mat.transmissionFactor = m_transmissionFactor[handle];
    mat.alphaMode = m_alphaMode[handle];
    mat.alphaCutoff = m_alphaCutoff[handle];
    mat.emissiveFactor = m_emissiveFactor[handle];
    mat.ior = m_ior[handle];
    mat.specularFactor = m_specularFactor[handle];
    mat.specularColorFactor = m_specularColorFactor[handle];

    return mat;
}

void MaterialTable::Clear() {
    m_valid.clear();
    m_albedo.clear();
    m_metallic.clear();
    m_roughness.clear();
    m_doubleSided.clear();
    m_alpha.clear();
    m_transmissionFactor.clear();
    m_alphaMode.clear();
    m_alphaCutoff.clear();
    m_emissiveFactor.clear();
    m_ior.clear();
    m_specularFactor.clear();
    m_specularColorFactor.clear();

    m_baseColorTextureIndex.clear();
    m_metallicRoughnessTextureIndex.clear();
    m_normalTextureIndex.clear();
    m_occlusionTextureIndex.clear();
    m_emissiveTextureIndex.clear();
    m_transmissionTextureIndex.clear();

    m_baseColorImageIndex.clear();
    m_metallicRoughnessImageIndex.clear();
    m_normalImageIndex.clear();
    m_occlusionImageIndex.clear();
    m_emissiveImageIndex.clear();
    m_transmissionImageIndex.clear();

    m_baseColorSamplerIndex.clear();
    m_metallicRoughnessSamplerIndex.clear();
    m_normalSamplerIndex.clear();
    m_occlusionSamplerIndex.clear();
    m_emissiveSamplerIndex.clear();
    m_transmissionSamplerIndex.clear();

    m_baseColorTexCoordSet.clear();
    m_metallicRoughnessTexCoordSet.clear();
    m_normalTexCoordSet.clear();
    m_occlusionTexCoordSet.clear();
    m_emissiveTexCoordSet.clear();
    m_transmissionTexCoordSet.clear();

    m_meshIndex.clear();
    m_materialIndex.clear();
    m_primitiveIndex.clear();
    m_nodeIndex.clear();

    m_freeSlots.clear();
    m_count = 0;

    // 清空后重新初始化默认材质（插槽 0）
    InitializeDefaultMaterial();
}

} // namespace SR

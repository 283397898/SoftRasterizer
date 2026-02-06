#include "Pipeline/GeometryProcessor.h"

#include "Pipeline/VertexShader.h"

namespace SR {

/**
 * @brief 构建经过变换的三角形集合
 * 执行 MVP 变换，世界空间变换，并计算世界空间法线
 */
void GeometryProcessor::BuildTriangles(const Mesh& mesh,
                                       const PBRMaterial& material,
                                       const DrawItem& item,
                                       const Mat4& modelMatrix,
                                       const Mat4& normalMatrix,
                                       const FrameContext& frameContext,
                                       std::vector<Triangle>& outTriangles) const {
    outTriangles.clear();
    m_lastTriangleCount = 0;

    const auto& vertices = mesh.GetVertices();
    const auto& indices = mesh.GetIndices();
    if (vertices.empty() || indices.size() < 3) {
        return;
    }

    outTriangles.reserve(indices.size() / 3);

    Mat4 mvp = modelMatrix * frameContext.view * frameContext.projection;

    VertexShader vertexShader;
    vertexShader.SetMVP(mvp);

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            continue;
        }

        const Vec3& p0 = vertices[i0].position;
        const Vec3& p1 = vertices[i1].position;
        const Vec3& p2 = vertices[i2].position;

        const Vec3& n0 = vertices[i0].normal;
        const Vec3& n1 = vertices[i1].normal;
        const Vec3& n2 = vertices[i2].normal;

        const Vec2& t0 = vertices[i0].texCoord;
        const Vec2& t1 = vertices[i1].texCoord;
        const Vec2& t2 = vertices[i2].texCoord;

        const Vec3& tg0 = vertices[i0].tangent;
        const Vec3& tg1 = vertices[i1].tangent;
        const Vec3& tg2 = vertices[i2].tangent;

        Triangle tri{};
        tri.v0 = vertexShader.TransformPosition(Vec4{p0.x, p0.y, p0.z, 1.0});
        tri.v1 = vertexShader.TransformPosition(Vec4{p1.x, p1.y, p1.z, 1.0});
        tri.v2 = vertexShader.TransformPosition(Vec4{p2.x, p2.y, p2.z, 1.0});

        tri.t0 = t0;
        tri.t1 = t1;
        tri.t2 = t2;

        tri.tg0 = tg0;
        tri.tg1 = tg1;
        tri.tg2 = tg2;

        Vec4 wp0 = modelMatrix.Multiply(Vec4{p0.x, p0.y, p0.z, 1.0});
        Vec4 wp1 = modelMatrix.Multiply(Vec4{p1.x, p1.y, p1.z, 1.0});
        Vec4 wp2 = modelMatrix.Multiply(Vec4{p2.x, p2.y, p2.z, 1.0});

        Vec4 wn0 = normalMatrix.Multiply(Vec4{n0.x, n0.y, n0.z, 0.0});
        Vec4 wn1 = normalMatrix.Multiply(Vec4{n1.x, n1.y, n1.z, 0.0});
        Vec4 wn2 = normalMatrix.Multiply(Vec4{n2.x, n2.y, n2.z, 0.0});

        tri.w0 = Vec3{wp0.x, wp0.y, wp0.z};
        tri.w1 = Vec3{wp1.x, wp1.y, wp1.z};
        tri.w2 = Vec3{wp2.x, wp2.y, wp2.z};

        tri.n0 = Vec3{wn0.x, wn0.y, wn0.z}.Normalized();
        tri.n1 = Vec3{wn1.x, wn1.y, wn1.z}.Normalized();
        tri.n2 = Vec3{wn2.x, wn2.y, wn2.z}.Normalized();

        tri.material = material;
        tri.meshIndex = item.meshIndex;
        tri.materialIndex = item.materialIndex;
        tri.primitiveIndex = item.primitiveIndex;
        tri.nodeIndex = item.nodeIndex;
        tri.baseColorTextureIndex = item.baseColorTextureIndex;
        tri.metallicRoughnessTextureIndex = item.metallicRoughnessTextureIndex;
        tri.normalTextureIndex = item.normalTextureIndex;
        tri.occlusionTextureIndex = item.occlusionTextureIndex;
        tri.emissiveTextureIndex = item.emissiveTextureIndex;
        tri.baseColorImageIndex = item.baseColorImageIndex;
        tri.metallicRoughnessImageIndex = item.metallicRoughnessImageIndex;
        tri.normalImageIndex = item.normalImageIndex;
        tri.occlusionImageIndex = item.occlusionImageIndex;
        tri.emissiveImageIndex = item.emissiveImageIndex;
        tri.baseColorSamplerIndex = item.baseColorSamplerIndex;
        tri.metallicRoughnessSamplerIndex = item.metallicRoughnessSamplerIndex;
        tri.normalSamplerIndex = item.normalSamplerIndex;
        tri.occlusionSamplerIndex = item.occlusionSamplerIndex;
        tri.emissiveSamplerIndex = item.emissiveSamplerIndex;
        outTriangles.push_back(tri);
    }

    m_lastTriangleCount = static_cast<uint64_t>(outTriangles.size());
}

/**
 * @brief 获取最后一次构建生成的三角形总数
 */
uint64_t GeometryProcessor::GetLastTriangleCount() const {
    return m_lastTriangleCount;
}

} // namespace SR

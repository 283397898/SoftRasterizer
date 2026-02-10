#include "Scene/Mesh.h"

#include <cmath>

namespace SR {

/**
 * @brief 设置网格的顶点和索引数据
 */
void Mesh::SetData(std::vector<Vertex> vertices, std::vector<uint32_t> indices) {
    m_vertices = std::move(vertices);
    m_indices = std::move(indices);
}

/**
 * @brief 自动生成网格法线
 * @param mode 法线生成模式 (Flat, Smooth, SmoothAngle)
 * @param hardAngleDegrees 当模式为 SmoothAngle 时的硬边阈值角度
 */
void Mesh::GenerateNormals(NormalMode mode, double hardAngleDegrees) {
    if (m_vertices.empty() || m_indices.size() < 3) {
        return;
    }

    // 1. Flat 模式：每个面生成独立的顶点，法线与面垂直
    if (mode == NormalMode::Flat) {
        std::vector<Vertex> newVertices;
        std::vector<uint32_t> newIndices;
        newVertices.reserve(m_indices.size());
        newIndices.reserve(m_indices.size());

        for (size_t i = 0; i + 2 < m_indices.size(); i += 3) {
            uint32_t i0 = m_indices[i];
            uint32_t i1 = m_indices[i + 1];
            uint32_t i2 = m_indices[i + 2];
            if (i0 >= m_vertices.size() || i1 >= m_vertices.size() || i2 >= m_vertices.size()) {
                continue;
            }

            Vertex v0 = m_vertices[i0];
            Vertex v1 = m_vertices[i1];
            Vertex v2 = m_vertices[i2];

            Vec3 e1 = v1.position - v0.position;
            Vec3 e2 = v2.position - v0.position;
            Vec3 faceNormal = Vec3::Cross(e1, e2).Normalized();

            v0.normal = faceNormal;
            v1.normal = faceNormal;
            v2.normal = faceNormal;

            uint32_t base = static_cast<uint32_t>(newVertices.size());
            newVertices.push_back(v0);
            newVertices.push_back(v1);
            newVertices.push_back(v2);

            newIndices.push_back(base + 0);
            newIndices.push_back(base + 1);
            newIndices.push_back(base + 2);
        }

        m_vertices = std::move(newVertices);
        m_indices = std::move(newIndices);
        return;
    }

    // 2. SmoothAngle 模式：基于面法线夹角决定是否平滑，常用于保持模型棱角
    if (mode == NormalMode::SmoothAngle) {
        std::vector<Vertex> newVertices;
        std::vector<uint32_t> newIndices;
        std::vector<Vec3> normalSums;
        std::vector<std::vector<size_t>> clusters(m_vertices.size());

        double radians = hardAngleDegrees * 3.14159265358979323846 / 180.0;
        double cosThreshold = std::cos(radians);

        newVertices.reserve(m_vertices.size());
        normalSums.reserve(m_vertices.size());
        newIndices.reserve(m_indices.size());

        for (size_t i = 0; i + 2 < m_indices.size(); i += 3) {
            uint32_t i0 = m_indices[i];
            uint32_t i1 = m_indices[i + 1];
            uint32_t i2 = m_indices[i + 2];
            if (i0 >= m_vertices.size() || i1 >= m_vertices.size() || i2 >= m_vertices.size()) {
                continue;
            }

            const Vertex& ov0 = m_vertices[i0];
            const Vertex& ov1 = m_vertices[i1];
            const Vertex& ov2 = m_vertices[i2];

            Vec3 e1 = ov1.position - ov0.position;
            Vec3 e2 = ov2.position - ov0.position;
            Vec3 faceNormal = Vec3::Cross(e1, e2).Normalized();

            uint32_t newTri[3] = {};
            const uint32_t orig[3] = { i0, i1, i2 };
            const Vertex* ovs[3] = { &ov0, &ov1, &ov2 };

            for (int c = 0; c < 3; ++c) {
                uint32_t origIndex = orig[c];
                auto& list = clusters[origIndex];
                size_t chosen = static_cast<size_t>(-1);

                for (size_t idx : list) {
                    Vec3 avg = normalSums[idx].Normalized();
                    if (Vec3::Dot(avg, faceNormal) >= cosThreshold) {
                        chosen = idx;
                        break;
                    }
                }

                if (chosen == static_cast<size_t>(-1)) {
                    Vertex nv = *ovs[c];
                    nv.normal = faceNormal;
                    newVertices.push_back(nv);
                    normalSums.push_back(faceNormal);
                    size_t newIndex = newVertices.size() - 1;
                    list.push_back(newIndex);
                    chosen = newIndex;
                } else {
                    normalSums[chosen] = normalSums[chosen] + faceNormal;
                }

                newTri[c] = static_cast<uint32_t>(chosen);
            }

            newIndices.push_back(newTri[0]);
            newIndices.push_back(newTri[1]);
            newIndices.push_back(newTri[2]);
        }

        for (size_t i = 0; i < newVertices.size(); ++i) {
            newVertices[i].normal = normalSums[i].Normalized();
        }

        m_vertices = std::move(newVertices);
        m_indices = std::move(newIndices);
        return;
    }

    // 3. 默认 Smooth 模式：所有共用顶点的三角形法线进行累加
    for (auto& v : m_vertices) {
        v.normal = Vec3{};
    }

    for (size_t i = 0; i + 2 < m_indices.size(); i += 3) {
        uint32_t i0 = m_indices[i];
        uint32_t i1 = m_indices[i + 1];
        uint32_t i2 = m_indices[i + 2];
        if (i0 >= m_vertices.size() || i1 >= m_vertices.size() || i2 >= m_vertices.size()) {
            continue;
        }

        const Vec3& p0 = m_vertices[i0].position;
        const Vec3& p1 = m_vertices[i1].position;
        const Vec3& p2 = m_vertices[i2].position;

        Vec3 e1 = p1 - p0;
        Vec3 e2 = p2 - p0;
        Vec3 n = Vec3::Cross(e1, e2); // 注意：这里通常应该带面积加权，但直接简单加和

        m_vertices[i0].normal = m_vertices[i0].normal + n;
        m_vertices[i1].normal = m_vertices[i1].normal + n;
        m_vertices[i2].normal = m_vertices[i2].normal + n;
    }

    for (auto& v : m_vertices) {
        v.normal = v.normal.Normalized();
    }
}

/**
 * @brief 生成网格切线 (Tangent)，用于法线贴图计算
 */
void Mesh::GenerateTangents() {
    if (m_vertices.empty() || m_indices.size() < 3) {
        return;
    }

    for (auto& v : m_vertices) {
        v.tangent = Vec3{};
    }

    // 遍历所有三角形，计算切线方向
    for (size_t i = 0; i + 2 < m_indices.size(); i += 3) {
        uint32_t i0 = m_indices[i];
        uint32_t i1 = m_indices[i + 1];
        uint32_t i2 = m_indices[i + 2];
        if (i0 >= m_vertices.size() || i1 >= m_vertices.size() || i2 >= m_vertices.size()) {
            continue;
        }

        const Vec3& p0 = m_vertices[i0].position;
        const Vec3& p1 = m_vertices[i1].position;
        const Vec3& p2 = m_vertices[i2].position;

        const Vec2& uv0 = m_vertices[i0].texCoord;
        const Vec2& uv1 = m_vertices[i1].texCoord;
        const Vec2& uv2 = m_vertices[i2].texCoord;

        Vec3 e1 = p1 - p0;
        Vec3 e2 = p2 - p0;

        double du1 = uv1.x - uv0.x;
        double dv1 = uv1.y - uv0.y;
        double du2 = uv2.x - uv0.x;
        double dv2 = uv2.y - uv0.y;

        double denom = du1 * dv2 - du2 * dv1;
        if (std::fabs(denom) < 1e-12) {
            continue;
        }

        double inv = 1.0 / denom;
        Vec3 tangent{
            (e1.x * dv2 - e2.x * dv1) * inv,
            (e1.y * dv2 - e2.y * dv1) * inv,
            (e1.z * dv2 - e2.z * dv1) * inv
        };

        m_vertices[i0].tangent = m_vertices[i0].tangent + tangent;
        m_vertices[i1].tangent = m_vertices[i1].tangent + tangent;
        m_vertices[i2].tangent = m_vertices[i2].tangent + tangent;
    }

    for (auto& v : m_vertices) {
        v.tangent = v.tangent.Normalized();
    }
}

/**
 * @brief 程序化生成球体网格
 */
Mesh Mesh::CreateSphere(double radius, int segments, int rings) {
    Mesh mesh;
    if (segments < 3) {
        segments = 3;
    }
    if (rings < 2) {
        rings = 2;
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(static_cast<size_t>(segments) * static_cast<size_t>(rings + 1));

    for (int r = 0; r <= rings; ++r) {
        double v = static_cast<double>(r) / static_cast<double>(rings);
        double phi = v * 3.14159265358979323846;
        double sinPhi = std::sin(phi);
        double cosPhi = std::cos(phi);

        for (int s = 0; s <= segments; ++s) {
            double u = static_cast<double>(s) / static_cast<double>(segments);
            double theta = u * 2.0 * 3.14159265358979323846;
            double sinTheta = std::sin(theta);
            double cosTheta = std::cos(theta);

            Vec3 position{
                radius * sinPhi * cosTheta,
                radius * cosPhi,
                radius * sinPhi * sinTheta
            };

            Vec3 normal = position.Normalized();

            Vec3 tangent{
                -sinTheta,
                0.0,
                cosTheta
            };

            Vec2 uv{u, v};
            vertices.push_back(Vertex{position, normal, uv, uv, Vec4{1.0, 1.0, 1.0, 1.0}, tangent});
        }
    }

    int stride = segments + 1;
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            int i0 = r * stride + s;
            int i1 = i0 + 1;
            int i2 = i0 + stride;
            int i3 = i2 + 1;

            indices.push_back(static_cast<uint32_t>(i0));
            indices.push_back(static_cast<uint32_t>(i2));
            indices.push_back(static_cast<uint32_t>(i1));

            indices.push_back(static_cast<uint32_t>(i1));
            indices.push_back(static_cast<uint32_t>(i2));
            indices.push_back(static_cast<uint32_t>(i3));
        }
    }

    mesh.SetData(std::move(vertices), std::move(indices));
    return mesh;
}

/**
 * @brief 程序化生成立方体网格
 */
Mesh Mesh::CreateCube(double size) {
    Mesh mesh;
    if (size <= 0.0) {
        return mesh;
    }

    double h = size * 0.5;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(24);
    indices.reserve(36);

    auto addFace = [&](const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3) {
        uint32_t base = static_cast<uint32_t>(vertices.size());

        Vec2 uv0{0.0, 0.0};
        Vec2 uv1{1.0, 0.0};
        Vec2 uv2{0.0, 1.0};
        Vec2 uv3{1.0, 1.0};
        vertices.push_back(Vertex{v0, Vec3{}, uv0, uv0, Vec4{1.0, 1.0, 1.0, 1.0}, Vec3{}});
        vertices.push_back(Vertex{v1, Vec3{}, uv1, uv1, Vec4{1.0, 1.0, 1.0, 1.0}, Vec3{}});
        vertices.push_back(Vertex{v2, Vec3{}, uv2, uv2, Vec4{1.0, 1.0, 1.0, 1.0}, Vec3{}});
        vertices.push_back(Vertex{v3, Vec3{}, uv3, uv3, Vec4{1.0, 1.0, 1.0, 1.0}, Vec3{}});

        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 1);

        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    };

    // +Z
    addFace(Vec3{-h, h, h}, Vec3{h, h, h}, Vec3{-h, -h, h}, Vec3{h, -h, h});
    // -Z
    addFace(Vec3{h, h, -h}, Vec3{-h, h, -h}, Vec3{h, -h, -h}, Vec3{-h, -h, -h});
    // +X
    addFace(Vec3{h, h, h}, Vec3{h, h, -h}, Vec3{h, -h, h}, Vec3{h, -h, -h});
    // -X
    addFace(Vec3{-h, h, -h}, Vec3{-h, h, h}, Vec3{-h, -h, -h}, Vec3{-h, -h, h});
    // +Y
    addFace(Vec3{-h, h, -h}, Vec3{h, h, -h}, Vec3{-h, h, h}, Vec3{h, h, h});
    // -Y
    addFace(Vec3{-h, -h, h}, Vec3{h, -h, h}, Vec3{-h, -h, -h}, Vec3{h, -h, -h});

    mesh.SetData(std::move(vertices), std::move(indices));
    mesh.GenerateNormals();
    return mesh;
}

const std::vector<Vertex>& Mesh::GetVertices() const {
    return m_vertices;
}

const std::vector<uint32_t>& Mesh::GetIndices() const {
    return m_indices;
}

} // namespace SR

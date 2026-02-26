#pragma once

#include "Runtime/ResourcePool.h"
#include "Scene/Mesh.h"

namespace SR {

/**
 * @brief 网格资源池
 *
 * 特化版本，添加网格特定的功能：
 * - 内存使用估算
 * - 顶点/索引计数
 */
class MeshPool : public ResourcePool<Mesh> {
public:
    using Handle = ResourcePool<Mesh>::Handle;

    /**
     * @brief 获取网格内存使用量估算
     * @param h 网格句柄
     * @return 字节数
     */
    size_t GetMeshMemory(Handle h) const {
        const Mesh* mesh = Get(h);
        if (!mesh) return 0;
        // Vertex: ~56 bytes (position, normal, texCoord, tangent, color, etc.)
        // Index: 4 bytes
        const auto& vertices = mesh->GetVertices();
        const auto& indices = mesh->GetIndices();
        return vertices.size() * sizeof(Vertex) + indices.size() * sizeof(uint32_t);
    }

    /**
     * @brief 获取网格总顶点数
     * @param h 网格句柄
     * @return 顶点数
     */
    size_t GetVertexCount(Handle h) const {
        const Mesh* mesh = Get(h);
        if (!mesh) return 0;
        return mesh->GetVertices().size();
    }

    /**
     * @brief 获取网格总三角形数
     * @param h 网格句柄
     * @return 三角形数
     */
    size_t GetTriangleCount(Handle h) const {
        const Mesh* mesh = Get(h);
        if (!mesh) return 0;
        return mesh->GetIndices().size() / 3;
    }

    /**
     * @brief 获取所有网格的总顶点数
     * @return 总顶点数
     */
    size_t GetTotalVertexCount() const {
        size_t total = 0;
        ForEach([&total](Handle, const Mesh* mesh) {
            total += mesh->GetVertices().size();
        });
        return total;
    }

    /**
     * @brief 获取所有网格的总三角形数
     * @return 总三角形数
     */
    size_t GetTotalTriangleCount() const {
        size_t total = 0;
        ForEach([&total](Handle, const Mesh* mesh) {
            total += mesh->GetIndices().size() / 3;
        });
        return total;
    }
};

} // namespace SR

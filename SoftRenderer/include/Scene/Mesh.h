#pragma once

#include <cstdint>
#include <vector>

#include "SoftRendererExport.h"
#include "Scene/Vertex.h"

namespace SR {

/**
 * @brief 网格类，存储顶点和索引数据，并提供各种几何处理功能
 */
class SR_API Mesh {
public:
    /** @brief 法线计算模式 */
    enum class NormalMode {
        Smooth,      ///< 平滑法线 (顶点法线累加)
        SmoothAngle, ///< 带角度阈值的平滑 (硬边处理)
        Flat         ///< 面法线 (面内每个顶点法线一致)
    };

    /** @brief 初始化网格数据 */
    void SetData(std::vector<Vertex> vertices, std::vector<uint32_t> indices);
    /** @brief 基于拓扑结构自动生成法线 */
    void GenerateNormals(NormalMode mode = NormalMode::Smooth, double hardAngleDegrees = 60.0);
    /** @brief 基于 UV 坐标自动生成切线 */
    void GenerateTangents();

    /** @brief 工厂方法：创建一个程序化球体 */
    static Mesh CreateSphere(double radius, int segments, int rings);
    /** @brief 工厂方法：创建一个程序化立方体 */
    static Mesh CreateCube(double size);

    /** @brief 获取顶点数组引用 */
    const std::vector<Vertex>& GetVertices() const;
    /** @brief 获取索引数组引用 */
    const std::vector<uint32_t>& GetIndices() const;

private:
    std::vector<Vertex> m_vertices;   ///< 顶点缓中区
    std::vector<uint32_t> m_indices; ///< 索引缓冲区 (支持非索引绘制时可为空，但目前逻辑倾向于有索引)
};

} // namespace SR

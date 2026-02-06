#pragma once

#include <vector>

#include "Material/PBRMaterial.h"
#include "Pipeline/FrameContext.h"
#include "Pipeline/Rasterizer.h"
#include "Scene/Mesh.h"
#include "Scene/Transform.h"
#include "Scene/RenderQueue.h"

namespace SR {

/**
 * @brief 几何处理器类，负责顶点变换和三角形构建
 */
class GeometryProcessor {
public:
    /**
     * @brief 从网格和材质构建一组经过变换的三角形
     * @param mesh 输入网格数据
     * @param material 输入材质参数
     * @param item 渲染提交项
     * @param modelMatrix 模型到世界坐标变换矩阵
     * @param normalMatrix 法线变换矩阵
     * @param frameContext 系统级帧上下文 (包含 View/Projection)
     * @param outTriangles 输出构建好的三角形列表
     */
    void BuildTriangles(const Mesh& mesh,
                        const PBRMaterial& material,
                        const DrawItem& item,
                        const Mat4& modelMatrix,
                        const Mat4& normalMatrix,
                        const FrameContext& frameContext,
                        std::vector<Triangle>& outTriangles) const;
    /** @brief 获取最后一次构建生成的三角形总数 */
    uint64_t GetLastTriangleCount() const;

private:
    mutable uint64_t m_lastTriangleCount = 0;
};

} // namespace SR

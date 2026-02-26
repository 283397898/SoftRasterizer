#pragma once

#include <vector>

#include "Material/PBRMaterial.h"
#include "Pipeline/FrameContext.h"
#include "Pipeline/MaterialTable.h"
#include "Scene/Mesh.h"
#include "Scene/Transform.h"
#include "Scene/RenderQueue.h"

namespace SR {

struct Triangle;

/**
 * @brief 从 PBRMaterial 和 DrawItem 构建 MaterialParams
 *
 * 纯函数，将材质属性和纹理绑定信息组装为 MaterialTable 可接受的参数结构。
 * 设计为在单线程预处理阶段调用，避免在并行循环中操作 MaterialTable。
 */
MaterialParams BuildMaterialParams(const PBRMaterial& material, const DrawItem& item);

/**
 * @brief 几何处理器类，负责顶点变换和三角形构建
 */
class GeometryProcessor {
public:
    /**
     * @brief 从网格构建一组经过变换的三角形
     * @param mesh 输入网格数据
     * @param item 渲染提交项
     * @param modelMatrix 模型到世界坐标变换矩阵
     * @param normalMatrix 法线变换矩阵
     * @param frameContext 系统级帧上下文 (包含 View/Projection)
     * @param materialHandle 预注册的材质句柄
     * @param outTriangles 输出构建好的三角形列表
     */
    void BuildTriangles(const Mesh& mesh,
                        const DrawItem& item,
                        const Mat4& modelMatrix,
                        const Mat4& normalMatrix,
                        const FrameContext& frameContext,
                        MaterialHandle materialHandle,
                        std::vector<Triangle>& outTriangles) const;
    /** @brief 获取最后一次构建生成的三角形总数 */
    uint64_t GetLastTriangleCount() const;

private:
    mutable uint64_t m_lastTriangleCount = 0;
};

} // namespace SR

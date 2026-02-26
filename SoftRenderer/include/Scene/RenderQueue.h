#pragma once

#include <vector>

#include "Material/PBRMaterial.h"
#include "Math/Mat4.h"
#include "Scene/Mesh.h"
#include "Scene/TextureBinding.h"
#include "Scene/Transform.h"

namespace SR {

class ObjectGroup;

/**
 * @brief 绘制负载项，包含单个绘制指令所需的全部信息
 */
struct DrawItem {
    const Mesh* mesh = nullptr;                 ///< 渲染所用的网格
    const PBRMaterial* material = nullptr;       ///< 渲染所用的材质
    Mat4 modelMatrix = Mat4::Identity();         ///< 模型变换矩阵
    Mat4 normalMatrix = Mat4::Identity();        ///< 法线变换矩阵
    int meshIndex = -1;                          ///< 网格索引
    int materialIndex = -1;                      ///< 材质索引
    int primitiveIndex = -1;                     ///< Primitive 索引
    int nodeIndex = -1;                          ///< 节点索引
    TextureBindingArray textures{}; ///< 纹理绑定数组
};

/**
 * @brief 渲染队列，存储一帧中所有需要绘制的项目
 */
class RenderQueue {
public:
    /** @brief 设置队列内容 */
    void SetItems(std::vector<DrawItem>&& items);
    /** @brief 清空队列 */
    void Clear();
    /** @brief 获取所有绘制项 */
    const std::vector<DrawItem>& GetItems() const;

private:
    std::vector<DrawItem> m_items; ///< 存储 DrawItem 的列表
};

} // namespace SR

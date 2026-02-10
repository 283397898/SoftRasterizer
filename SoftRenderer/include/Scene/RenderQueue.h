#pragma once

#include <vector>

#include "Material/PBRMaterial.h"
#include "Math/Mat4.h"
#include "Scene/Mesh.h"
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
    int baseColorTextureIndex = -1;              ///< 基础颜色纹理索引
    int metallicRoughnessTextureIndex = -1;      ///< 金属度-粗糙度纹理索引
    int normalTextureIndex = -1;                  ///< 法线纹理索引
    int occlusionTextureIndex = -1;               ///< 遮蔽纹理索引
    int emissiveTextureIndex = -1;                ///< 自发光纹理索引
    int transmissionTextureIndex = -1;            ///< 透射纹理索引
    int baseColorImageIndex = -1;                 ///< 基础颜色图像索引
    int metallicRoughnessImageIndex = -1;         ///< 金属度-粗糙度图像索引
    int normalImageIndex = -1;                     ///< 法线图像索引
    int occlusionImageIndex = -1;                  ///< 遮蔽图像索引
    int emissiveImageIndex = -1;                   ///< 自发光图像索引
    int transmissionImageIndex = -1;              ///< 透射图像索引
    int baseColorSamplerIndex = -1;               ///< 基础颜色采样器索引
    int metallicRoughnessSamplerIndex = -1;       ///< 金属度-粗糙度采样器索引
    int normalSamplerIndex = -1;                  ///< 法线采样器索引
    int occlusionSamplerIndex = -1;               ///< 遮蔽采样器索引
    int emissiveSamplerIndex = -1;                ///< 自发光采样器索引
    int transmissionSamplerIndex = -1;            ///< 透射采样器索引
    int baseColorTexCoordSet = 0;                 ///< 基础颜色 UV 集索引
    int metallicRoughnessTexCoordSet = 0;         ///< 金属度-粗糙度 UV 集索引
    int normalTexCoordSet = 0;                    ///< 法线 UV 集索引
    int occlusionTexCoordSet = 0;                 ///< 遮蔽 UV 集索引
    int emissiveTexCoordSet = 0;                  ///< 自发光 UV 集索引
    int transmissionTexCoordSet = 0;              ///< 透射纹理 UV 集索引
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

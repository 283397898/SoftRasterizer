#pragma once

#include "Runtime/GPUScene.h"

namespace SR {

class ObjectGroup;

/**
 * @brief GPUScene 构建器，将 ObjectGroup 转换为扁平化的 GPUScene 结构。
 *
 * 遍历 ObjectGroup 中的所有 Model，提取网格、材质和变换信息，
 * 生成供渲染管线使用的 GPUSceneDrawItem 列表。
 */
class GPUSceneBuilder {
public:
    /**
     * @brief 从 ObjectGroup 构建 GPUScene
     * @param objects  输入的场景对象组
     * @param outScene 输出的 GPUScene（追加方式，不会清空原有内容）
     */
    void BuildFromObjectGroup(const ObjectGroup& objects, GPUScene& outScene) const;
};

} // namespace SR

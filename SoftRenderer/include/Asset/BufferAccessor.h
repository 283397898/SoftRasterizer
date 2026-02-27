#pragma once

#include <vector>

#include "Asset/GLTFAsset.h"

namespace SR {

/**
 * @brief glTF 缓冲区访问器辅助类
 *
 * 提供类型化的读取接口，将 glTF 访问器中的原始字节流解析为具体的 C++ 类型数组。
 * 支持整数到浮点的自动归一化转换（accessor.normalized == true 时）。
 */
class BufferAccessor {
public:
    /**
     * @brief 从 glTF 资产中读取指定访问器的数据
     * @tparam T       目标数据类型（如 float、uint32_t、Vec3 等）
     * @param  asset    glTF 资产
     * @param  accessor 要读取的访问器描述
     * @return 解析后的元素数组
     */
    template <typename T>
    std::vector<T> Read(const GLTFAsset& asset, const GLTFAccessor& accessor) const;
};

} // namespace SR

#include "Asset/BufferAccessor.inl"

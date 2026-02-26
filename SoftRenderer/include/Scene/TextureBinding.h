#pragma once

#include <array>
#include <cstddef>

namespace SR {

/// @brief 材质纹理插槽枚举，与 PBR 工作流的各贴图通道对应
enum class TextureSlot : size_t {
    BaseColor         = 0, ///< 基础颜色 / 反照率贴图
    MetallicRoughness,     ///< 金属度-粗糙度贴图（B=金属度, G=粗糙度）
    Normal,                ///< 法线贴图（切线空间）
    Occlusion,             ///< 环境遮蔽贴图（R 通道）
    Emissive,              ///< 自发光贴图
    Transmission,          ///< 透射强度贴图（KHR_materials_transmission）
    Count                  ///< 插槽总数（用于数组大小）
};

/// @brief 单个纹理绑定信息，记录贴图对应的图像、采样器及 UV 坐标集
struct TextureBinding {
    int textureIndex = -1; ///< glTF 纹理索引，-1 表示无贴图
    int imageIndex   = -1; ///< 对应的图像索引
    int samplerIndex = -1; ///< 对应的采样器索引，-1 表示默认采样器
    int texCoordSet  = 0;  ///< 使用的 UV 坐标集（0=主 UV, 1=次 UV）
};

/// @brief 材质所有纹理插槽的绑定数组（按 TextureSlot 枚举索引）
using TextureBindingArray = std::array<TextureBinding, static_cast<size_t>(TextureSlot::Count)>;

} // namespace SR

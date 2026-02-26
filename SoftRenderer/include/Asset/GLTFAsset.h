#pragma once

#include <string>
#include <vector>

#include "Asset/GLTFTypes.h"

namespace SR {

/**
 * @brief glTF 资产顶层容器，存储解析后的所有 glTF 数据
 */
struct GLTFAsset {
    std::vector<GLTFBuffer>     buffers;            ///< 二进制缓冲区列表
    std::vector<GLTFBufferView> bufferViews;         ///< 缓冲区视图列表
    std::vector<GLTFAccessor>   accessors;           ///< 访问器列表
    std::vector<GLTFImage>      images;              ///< 图像列表（已解码为像素）
    std::vector<GLTFSampler>    samplers;            ///< 采样器列表
    std::vector<GLTFTexture>    textures;            ///< 纹理列表
    std::vector<GLTFMaterial>   materials;           ///< 材质列表
    std::vector<GLTFMesh>       meshes;              ///< 网格列表
    std::vector<GLTFNode>       nodes;               ///< 节点列表
    std::vector<GLTFScene>      scenes;              ///< 场景列表
    int         defaultSceneIndex = -1;              ///< 默认场景索引，-1 表示使用第一个
    std::string generator;                           ///< 生成该资产的工具名称
};

} // namespace SR

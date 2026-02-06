#pragma once

#include <string>

#include "SoftRendererExport.h"
#include "Asset/GLTFAsset.h"

namespace SR {

/**
 * @brief glTF/GLB 资产加载器
 */
class SR_API GLTFLoader {
public:
    /** @brief 加载二进制 GLB 文件 */
    GLTFAsset LoadGLB(const std::string& path);
    /** @brief 加载文本 GLTF 文件 */
    GLTFAsset LoadGLTF(const std::string& path);
    /** @brief 获取最后一次加载错误信息 */
    const std::string& GetLastError() const;

private:
    std::string m_lastError;
};

} // namespace SR

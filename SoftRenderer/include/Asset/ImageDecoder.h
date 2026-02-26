#pragma once

#include <string>
#include <vector>

#include "Asset/GLTFTypes.h"

namespace SR {

/**
 * @brief 图像解码器，支持 PNG/JPEG 等格式
 *
 * 将原始字节流解码为 RGBA8 像素数据，供 glTF 材质纹理使用
 */
class ImageDecoder {
public:
    /**
     * @brief 解码图像数据
     * @param data     原始图像字节流
     * @param mimeType 图像 MIME 类型（如 "image/png"）
     * @param outImage 解码后的图像输出
     * @return 成功返回 true
     */
    bool Decode(const std::vector<uint8_t>& data, const std::string& mimeType, GLTFImage& outImage);
    /** @brief 获取最近一次解码错误信息 */
    const std::string& GetLastError() const;

private:
    std::string m_lastError; ///< 最近一次错误信息
};

} // namespace SR

#pragma once

#include <string>
#include <vector>

#include "Core/HDRImage.h"

namespace SR {

/**
 * @brief 零依赖 EXR 解码器
 *        支持 scanline 格式，NONE/ZIP/ZIPS 压缩，HALF/FLOAT 通道
 */
class EXRDecoder {
public:
    /**
     * @brief 从文件路径加载 EXR 图像
     * @param path 文件路径
     * @param outImage 输出的 HDR 图像
     * @return 成功返回 true
     */
    bool LoadFromFile(const std::string& path, HDRImage& outImage);

    /**
     * @brief 从内存数据解码 EXR 图像
     * @param data 原始 EXR 字节数据
     * @param outImage 输出的 HDR 图像
     * @return 成功返回 true
     */
    bool Decode(const std::vector<uint8_t>& data, HDRImage& outImage);

    /** @brief 获取最近一次错误信息 */
    const std::string& GetLastError() const { return m_lastError; }

private:
    std::string m_lastError;
};

} // namespace SR

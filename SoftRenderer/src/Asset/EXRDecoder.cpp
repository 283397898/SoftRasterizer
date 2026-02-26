#include "Asset/EXRDecoder.h"
#include "Utils/Compression.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace SR {

namespace {

// ============================================================================
// 小端读取工具
// ============================================================================

uint32_t ReadU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

int32_t ReadI32LE(const uint8_t* p) {
    return static_cast<int32_t>(ReadU32LE(p));
}

uint64_t ReadU64LE(const uint8_t* p) {
    return static_cast<uint64_t>(ReadU32LE(p))
         | (static_cast<uint64_t>(ReadU32LE(p + 4)) << 32);
}

// ============================================================================
// IEEE 754 半精度 (16-bit) 转单精度 (32-bit)
// ============================================================================

float HalfToFloat(uint16_t h) {
    uint32_t sign = (static_cast<uint32_t>(h) >> 15) & 1u;
    uint32_t exponent = (static_cast<uint32_t>(h) >> 10) & 0x1Fu;
    uint32_t mantissa = static_cast<uint32_t>(h) & 0x3FFu;

    if (exponent == 0) {
        if (mantissa == 0) {
            // 零
            uint32_t result = sign << 31;
            float f;
            std::memcpy(&f, &result, 4);
            return f;
        }
        // 非规格化数 → 规格化
        while (!(mantissa & 0x400u)) {
            mantissa <<= 1;
            exponent--;
        }
        exponent++;
        mantissa &= ~0x400u;
    } else if (exponent == 31) {
        // Inf 或 NaN
        uint32_t result = (sign << 31) | 0x7F800000u | (mantissa << 13);
        float f;
        std::memcpy(&f, &result, 4);
        return f;
    }

    exponent = exponent + (127 - 15);
    uint32_t result = (sign << 31) | (exponent << 23) | (mantissa << 13);
    float f;
    std::memcpy(&f, &result, 4);
    return f;
}

// ============================================================================
// EXR 后处理：delta predictor + 字节交织还原
// ============================================================================

void UndoEXRPredictor(std::vector<uint8_t>& data) {
    if (data.size() < 2) return;
    // 还原 delta 编码（OpenEXR predictor 带 +128 偏移）
    // 编码: d[i] = (orig[i] - orig[i-1] + 128) mod 256
    // 解码: orig[i] = (d[i] + orig[i-1] - 128) mod 256
    for (size_t i = 1; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(data[i] + data[i - 1] - 128);
    }
    // 还原字节交织
    const size_t n = data.size();
    const size_t half = (n + 1) / 2;
    std::vector<uint8_t> temp(n);
    for (size_t i = 0; i < n; ++i) {
        if (i % 2 == 0)
            temp[i] = data[i / 2];
        else
            temp[i] = data[half + i / 2];
    }
    data = std::move(temp);
}

// ============================================================================
// EXR 通道信息
// ============================================================================

struct EXRChannel {
    std::string name;
    int pixelType = 0;  // 0=UINT, 1=HALF, 2=FLOAT
    int xSampling = 1;
    int ySampling = 1;
};

} // namespace

// ============================================================================
// EXRDecoder 实现
// ============================================================================

bool EXRDecoder::LoadFromFile(const std::string& path, HDRImage& outImage) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        m_lastError = "Failed to open file: " + path;
        return false;
    }
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    return Decode(data, outImage);
}

bool EXRDecoder::Decode(const std::vector<uint8_t>& data, HDRImage& outImage) {
    if (data.size() < 12) {
        m_lastError = "EXR data too small";
        return false;
    }

    // 1. 验证 magic number
    if (data[0] != 0x76 || data[1] != 0x2F || data[2] != 0x31 || data[3] != 0x01) {
        m_lastError = "Invalid EXR magic number";
        return false;
    }

    // 2. 读取版本
    uint32_t version = ReadU32LE(data.data() + 4);
    bool isTiled = (version >> 9) & 1;
    if (isTiled) {
        m_lastError = "Tiled EXR not supported (only scanline)";
        return false;
    }

    // 3. 解析 header attributes
    std::vector<EXRChannel> channels;
    int compression = -1;
    int dataXMin = 0, dataYMin = 0, dataXMax = 0, dataYMax = 0;
    // int dispXMin = 0, dispYMin = 0, dispXMax = 0, dispYMax = 0;

    size_t pos = 8;
    while (pos < data.size()) {
        // 读取 attribute name
        std::string attrName;
        while (pos < data.size() && data[pos] != 0) {
            attrName.push_back(static_cast<char>(data[pos++]));
        }
        if (pos >= data.size()) break;
        pos++; // 跳过 null 终止符

        // 空名称 = header 结束
        if (attrName.empty()) break;

        // 读取 attribute type
        std::string attrType;
        while (pos < data.size() && data[pos] != 0) {
            attrType.push_back(static_cast<char>(data[pos++]));
        }
        if (pos >= data.size()) break;
        pos++; // 跳过 null 终止符

        // 读取 attribute size
        if (pos + 4 > data.size()) break;
        uint32_t attrSize = ReadU32LE(data.data() + pos);
        pos += 4;

        if (pos + attrSize > data.size()) break;
        const uint8_t* attrData = data.data() + pos;

        // 解析关键属性
        if (attrName == "channels" && attrType == "chlist") {
            size_t cp = 0;
            while (cp < attrSize) {
                std::string chName;
                while (cp < attrSize && attrData[cp] != 0) {
                    chName.push_back(static_cast<char>(attrData[cp++]));
                }
                if (chName.empty()) break;
                cp++; // 跳过 null 终止符
                if (cp + 16 > attrSize) break;
                EXRChannel ch;
                ch.name = chName;
                ch.pixelType = ReadI32LE(attrData + cp); cp += 4;
                cp += 4; // 跳过 pLinear 标志和保留字段
                ch.xSampling = ReadI32LE(attrData + cp); cp += 4;
                ch.ySampling = ReadI32LE(attrData + cp); cp += 4;
                channels.push_back(ch);
            }
        } else if (attrName == "compression" && attrType == "compression") {
            compression = static_cast<int>(attrData[0]);
        } else if (attrName == "dataWindow" && attrType == "box2i") {
            if (attrSize >= 16) {
                dataXMin = ReadI32LE(attrData);
                dataYMin = ReadI32LE(attrData + 4);
                dataXMax = ReadI32LE(attrData + 8);
                dataYMax = ReadI32LE(attrData + 12);
            }
        }

        pos += attrSize;
    }

    // 验证解析结果
    if (channels.empty()) { m_lastError = "No channels found in EXR"; return false; }
    if (compression < 0) { m_lastError = "No compression attribute in EXR"; return false; }
    if (compression != 0 && compression != 2 && compression != 3) {
        m_lastError = "Unsupported EXR compression: " + std::to_string(compression);
        return false;
    }

    const int imgWidth = dataXMax - dataXMin + 1;
    const int imgHeight = dataYMax - dataYMin + 1;
    if (imgWidth <= 0 || imgHeight <= 0) {
        m_lastError = "Invalid EXR data window";
        return false;
    }

    // 排序通道（EXR 规范要求按字母序）
    std::sort(channels.begin(), channels.end(),
        [](const EXRChannel& a, const EXRChannel& b) { return a.name < b.name; });

    // 找到 R, G, B 通道索引（在排序后的列表中）
    int rIdx = -1, gIdx = -1, bIdx = -1;
    for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        if (channels[i].name == "R") rIdx = i;
        else if (channels[i].name == "G") gIdx = i;
        else if (channels[i].name == "B") bIdx = i;
    }
    if (rIdx < 0 || gIdx < 0 || bIdx < 0) {
        m_lastError = "EXR missing R, G, or B channel";
        return false;
    }

    // 验证通道类型一致
    for (auto& ch : channels) {
        if (ch.pixelType != 1 && ch.pixelType != 2) {
            m_lastError = "Unsupported EXR channel type: " + std::to_string(ch.pixelType) + " for " + ch.name;
            return false;
        }
    }

    // 计算每个通道每像素的字节数
    // EXR 像素数据按通道优先（channel-major）排列：
    //   [通道0 所有扫描行][通道1 所有扫描行]...
    auto bytesPerPixel = [](int pixelType) -> int {
        return (pixelType == 1) ? 2 : 4; // HALF=2, FLOAT=4
    };

    // 每通道每行的字节数
    std::vector<int> channelLineBytes(channels.size());
    int bytesPerScanline = 0;
    for (size_t i = 0; i < channels.size(); ++i) {
        channelLineBytes[i] = bytesPerPixel(channels[i].pixelType) * imgWidth;
        bytesPerScanline += channelLineBytes[i];
    }

    // 4. 读取 offset table
    int scanLinesPerChunk = 1;
    if (compression == 0) scanLinesPerChunk = 1;       // NONE
    else if (compression == 2) scanLinesPerChunk = 1;   // ZIPS
    else if (compression == 3) scanLinesPerChunk = 16;  // ZIP

    int numChunks = (imgHeight + scanLinesPerChunk - 1) / scanLinesPerChunk;

    if (pos + static_cast<size_t>(numChunks) * 8 > data.size()) {
        m_lastError = "EXR offset table out of range";
        return false;
    }

    std::vector<uint64_t> offsets(numChunks);
    for (int i = 0; i < numChunks; ++i) {
        offsets[i] = ReadU64LE(data.data() + pos);
        pos += 8;
    }

    // 5. 准备输出
    outImage.width = imgWidth;
    outImage.height = imgHeight;
    outImage.pixels.resize(static_cast<size_t>(imgWidth) * imgHeight * 3, 0.0f);

    // 6. 逐 chunk 解码
    for (int chunk = 0; chunk < numChunks; ++chunk) {
        size_t chunkPos = static_cast<size_t>(offsets[chunk]);
        if (chunkPos + 8 > data.size()) {
            m_lastError = "EXR chunk header out of range";
            return false;
        }

        int32_t chunkY = ReadI32LE(data.data() + chunkPos);
        chunkPos += 4;
        int32_t pixelDataSize = ReadI32LE(data.data() + chunkPos);
        chunkPos += 4;

        if (chunkPos + pixelDataSize > data.size()) {
            m_lastError = "EXR chunk data out of range";
            return false;
        }

        int firstScanline = chunkY - dataYMin;
        int numScanlines = std::min(scanLinesPerChunk, imgHeight - firstScanline);
        int expectedUncompressedSize = bytesPerScanline * numScanlines;

        std::vector<uint8_t> pixelData;

        if (compression == 0) {
            // NONE: 直接复制
            pixelData.assign(data.data() + chunkPos, data.data() + chunkPos + pixelDataSize);
        } else {
            // ZIP / ZIPS: zlib 解压
            std::vector<uint8_t> compressed(data.data() + chunkPos, data.data() + chunkPos + pixelDataSize);
            std::string zlibError;
            if (!InflateZlib(compressed, pixelData, zlibError)) {
                m_lastError = "EXR zlib decompress failed: " + zlibError;
                return false;
            }

            // EXR ZIP 后处理：delta predictor + 字节交织还原
            UndoEXRPredictor(pixelData);
        }

        if (static_cast<int>(pixelData.size()) < expectedUncompressedSize) {
            m_lastError = "EXR decompressed data too small (got " + std::to_string(pixelData.size())
                        + ", expected " + std::to_string(expectedUncompressedSize) + ")";
            return false;
        }

        // 7. 从 chunk 像素数据提取 R, G, B 到 HDRImage
        // 扫描行优先布局：[sl0: ch0 pixels | ch1 pixels | ...][sl1: ch0 pixels | ...]
        // channelScanlineOffset[ci] = 单行内通道 ci 的起始偏移
        std::vector<int> channelScanlineOffset(channels.size());
        {
            int off = 0;
            for (size_t i = 0; i < channels.size(); ++i) {
                channelScanlineOffset[i] = off;
                off += channelLineBytes[i];
            }
        }

        for (int sl = 0; sl < numScanlines; ++sl) {
            int y = firstScanline + sl;
            if (y < 0 || y >= imgHeight) continue;
            int slOffset = sl * bytesPerScanline;

            for (int x = 0; x < imgWidth; ++x) {
                float rgb[3] = {0.0f, 0.0f, 0.0f};
                int chIndices[3] = {rIdx, gIdx, bIdx};

                for (int c = 0; c < 3; ++c) {
                    int ci = chIndices[c];
                    int bpp = bytesPerPixel(channels[ci].pixelType);
                    // 扫描行优先寻址：行偏移 + 行内通道偏移 + 像素偏移
                    int pixOff = slOffset + channelScanlineOffset[ci] + x * bpp;

                    if (channels[ci].pixelType == 1) {
                        // HALF
                        uint16_t h = static_cast<uint16_t>(pixelData[pixOff])
                                   | (static_cast<uint16_t>(pixelData[pixOff + 1]) << 8);
                        rgb[c] = HalfToFloat(h);
                    } else {
                        // FLOAT
                        uint32_t bits = static_cast<uint32_t>(pixelData[pixOff])
                                      | (static_cast<uint32_t>(pixelData[pixOff + 1]) << 8)
                                      | (static_cast<uint32_t>(pixelData[pixOff + 2]) << 16)
                                      | (static_cast<uint32_t>(pixelData[pixOff + 3]) << 24);
                        float f;
                        std::memcpy(&f, &bits, 4);
                        rgb[c] = f;
                    }
                }

                size_t outIdx = (static_cast<size_t>(y) * imgWidth + x) * 3;
                outImage.pixels[outIdx + 0] = rgb[0];
                outImage.pixels[outIdx + 1] = rgb[1];
                outImage.pixels[outIdx + 2] = rgb[2];
            }
        }
    }

    return true;
}

} // namespace SR

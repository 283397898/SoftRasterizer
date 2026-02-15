#include "Asset/EXRDecoder.h"

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
// Zlib 解压缩（从 ImageDecoder 复制的核心 inflate 实现）
// ============================================================================

struct BitReader {
    const uint8_t* data = nullptr;
    size_t size = 0;
    size_t bytePos = 0;
    uint32_t bitBuffer = 0;
    int bitCount = 0;

    bool EnsureBits(int count) {
        while (bitCount < count && bytePos < size) {
            bitBuffer |= static_cast<uint32_t>(data[bytePos++]) << bitCount;
            bitCount += 8;
        }
        return bitCount >= count;
    }
    uint32_t PeekBits(int count) const { return bitBuffer & ((1u << count) - 1u); }
    void DropBits(int count) { bitBuffer >>= count; bitCount -= count; }
    bool ReadBits(int count, uint32_t& out) {
        if (!EnsureBits(count)) return false;
        out = PeekBits(count);
        DropBits(count);
        return true;
    }
    bool ReadBit(bool& out) {
        uint32_t v = 0;
        if (!ReadBits(1, v)) return false;
        out = (v != 0);
        return true;
    }
    void AlignToByte() {
        int skip = bitCount % 8;
        if (skip > 0) DropBits(skip);
    }
};

uint32_t ReverseBits(uint32_t value, int bits) {
    uint32_t result = 0;
    for (int i = 0; i < bits; ++i) {
        result = (result << 1) | (value & 1u);
        value >>= 1u;
    }
    return result;
}

bool BuildHuffmanTable(const std::vector<int>& lengths, int maxBits, std::vector<uint32_t>& outTable) {
    std::vector<int> blCount(static_cast<size_t>(maxBits) + 1, 0);
    for (int len : lengths) {
        if (len < 0 || len > maxBits) return false;
        if (len > 0) blCount[len]++;
    }
    std::vector<int> nextCode(static_cast<size_t>(maxBits) + 1, 0);
    int code = 0;
    for (int bits = 1; bits <= maxBits; ++bits) {
        code = (code + blCount[bits - 1]) << 1;
        nextCode[bits] = code;
    }
    const uint32_t invalid = 0xFFFFFFFFu;
    outTable.assign(1u << maxBits, invalid);
    for (size_t symbol = 0; symbol < lengths.size(); ++symbol) {
        int len = lengths[symbol];
        if (len == 0) continue;
        uint32_t codeValue = static_cast<uint32_t>(nextCode[len]++);
        uint32_t reversed = ReverseBits(codeValue, len);
        uint32_t fill = 1u << (maxBits - len);
        uint32_t entry = (static_cast<uint32_t>(len) << 16) | static_cast<uint32_t>(symbol);
        for (uint32_t i = 0; i < fill; ++i) {
            outTable[reversed | (i << len)] = entry;
        }
    }
    return true;
}

bool DecodeSymbol(BitReader& reader, const std::vector<uint32_t>& table, int maxBits, int& outSymbol) {
    if (!reader.EnsureBits(maxBits)) return false;
    uint32_t entry = table[reader.PeekBits(maxBits)];
    if (entry == 0xFFFFFFFFu) return false;
    int len = static_cast<int>(entry >> 16);
    outSymbol = static_cast<int>(entry & 0xFFFFu);
    reader.DropBits(len);
    return true;
}

bool InflateZlib(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, std::string& outError) {
    output.clear();
    if (input.size() < 2) { outError = "Zlib data too small"; return false; }
    uint8_t cmf = input[0];
    uint8_t flg = input[1];
    if ((cmf & 0x0F) != 8) { outError = "Unsupported zlib compression method"; return false; }
    uint16_t check = static_cast<uint16_t>((cmf << 8) | flg);
    if (check % 31 != 0) { outError = "Invalid zlib header"; return false; }
    if (flg & 0x20) { outError = "Zlib preset dictionary not supported"; return false; }

    BitReader reader;
    reader.data = input.data() + 2;
    reader.size = input.size() - 2;

    std::vector<uint32_t> fixedLitTable, fixedDistTable;
    std::vector<int> fixedLitLengths(288, 0), fixedDistLengths(32, 5);
    for (int i = 0; i <= 143; ++i) fixedLitLengths[i] = 8;
    for (int i = 144; i <= 255; ++i) fixedLitLengths[i] = 9;
    for (int i = 256; i <= 279; ++i) fixedLitLengths[i] = 7;
    for (int i = 280; i <= 287; ++i) fixedLitLengths[i] = 8;
    BuildHuffmanTable(fixedLitLengths, 15, fixedLitTable);
    BuildHuffmanTable(fixedDistLengths, 15, fixedDistTable);

    constexpr int kLengthBase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    constexpr int kLengthExtra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    constexpr int kDistBase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    constexpr int kDistExtra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

    bool isFinal = false;
    while (!isFinal) {
        if (!reader.ReadBit(isFinal)) { outError = "Failed to read block header"; return false; }
        uint32_t blockType = 0;
        if (!reader.ReadBits(2, blockType)) { outError = "Failed to read block type"; return false; }

        if (blockType == 0) {
            reader.AlignToByte();
            uint32_t len = 0, nlen = 0;
            if (!reader.ReadBits(16, len) || !reader.ReadBits(16, nlen)) { outError = "Invalid uncompressed block"; return false; }
            if ((len ^ 0xFFFFu) != nlen) { outError = "Invalid uncompressed block length"; return false; }
            if (reader.bytePos + len > reader.size) { outError = "Block out of range"; return false; }
            output.insert(output.end(), reader.data + reader.bytePos, reader.data + reader.bytePos + len);
            reader.bytePos += len;
        } else if (blockType == 1 || blockType == 2) {
            std::vector<uint32_t> litTable = fixedLitTable, distTable = fixedDistTable;
            if (blockType == 2) {
                uint32_t hlit = 0, hdist = 0, hclen = 0;
                if (!reader.ReadBits(5, hlit) || !reader.ReadBits(5, hdist) || !reader.ReadBits(4, hclen)) { outError = "Invalid dynamic Huffman header"; return false; }
                hlit += 257; hdist += 1; hclen += 4;
                static constexpr int kCodeOrder[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                std::vector<int> codeLengths(19, 0);
                for (uint32_t i = 0; i < hclen; ++i) { uint32_t l = 0; reader.ReadBits(3, l); codeLengths[kCodeOrder[i]] = static_cast<int>(l); }
                std::vector<uint32_t> codeTable;
                if (!BuildHuffmanTable(codeLengths, 7, codeTable)) { outError = "Failed to build code length table"; return false; }
                std::vector<int> litDistLengths;
                litDistLengths.reserve(hlit + hdist);
                while (litDistLengths.size() < hlit + hdist) {
                    int symbol = 0;
                    if (!DecodeSymbol(reader, codeTable, 7, symbol)) { outError = "Invalid code length symbol"; return false; }
                    if (symbol <= 15) { litDistLengths.push_back(symbol); }
                    else if (symbol == 16) {
                        if (litDistLengths.empty()) { outError = "Invalid repeat"; return false; }
                        uint32_t extra = 0; reader.ReadBits(2, extra);
                        litDistLengths.insert(litDistLengths.end(), 3 + static_cast<int>(extra), litDistLengths.back());
                    } else if (symbol == 17) {
                        uint32_t extra = 0; reader.ReadBits(3, extra);
                        litDistLengths.insert(litDistLengths.end(), 3 + static_cast<int>(extra), 0);
                    } else if (symbol == 18) {
                        uint32_t extra = 0; reader.ReadBits(7, extra);
                        litDistLengths.insert(litDistLengths.end(), 11 + static_cast<int>(extra), 0);
                    }
                }
                std::vector<int> ll(litDistLengths.begin(), litDistLengths.begin() + hlit);
                std::vector<int> dl(litDistLengths.begin() + hlit, litDistLengths.end());
                if (!BuildHuffmanTable(ll, 15, litTable)) { outError = "Failed to build literal table"; return false; }
                if (dl.empty()) dl.push_back(0);
                if (!BuildHuffmanTable(dl, 15, distTable)) { outError = "Failed to build distance table"; return false; }
            }
            while (true) {
                int symbol = 0;
                if (!DecodeSymbol(reader, litTable, 15, symbol)) { outError = "Failed to decode symbol"; return false; }
                if (symbol < 256) { output.push_back(static_cast<uint8_t>(symbol)); }
                else if (symbol == 256) { break; }
                else if (symbol >= 257 && symbol <= 285) {
                    int li = symbol - 257;
                    int length = kLengthBase[li];
                    if (kLengthExtra[li] > 0) { uint32_t e = 0; reader.ReadBits(kLengthExtra[li], e); length += static_cast<int>(e); }
                    int ds = 0;
                    if (!DecodeSymbol(reader, distTable, 15, ds)) { outError = "Failed to decode distance"; return false; }
                    if (ds < 0 || ds >= 30) { outError = "Invalid distance symbol"; return false; }
                    int distance = kDistBase[ds];
                    if (kDistExtra[ds] > 0) { uint32_t e = 0; reader.ReadBits(kDistExtra[ds], e); distance += static_cast<int>(e); }
                    if (distance <= 0 || static_cast<size_t>(distance) > output.size()) { outError = "Invalid distance"; return false; }
                    size_t start = output.size() - static_cast<size_t>(distance);
                    for (int i = 0; i < length; ++i) output.push_back(output[start + (i % distance)]);
                }
            }
        } else { outError = "Unsupported block type"; return false; }
    }
    return true;
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
        pos++; // skip null terminator

        // 空名称 = header 结束
        if (attrName.empty()) break;

        // 读取 attribute type
        std::string attrType;
        while (pos < data.size() && data[pos] != 0) {
            attrType.push_back(static_cast<char>(data[pos++]));
        }
        if (pos >= data.size()) break;
        pos++; // skip null terminator

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
                cp++; // null terminator
                if (cp + 16 > attrSize) break;
                EXRChannel ch;
                ch.name = chName;
                ch.pixelType = ReadI32LE(attrData + cp); cp += 4;
                cp += 4; // pLinear + reserved
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

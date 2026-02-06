#include "Asset/ImageDecoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <cstdio>
#define NOMINMAX
#include <windows.h>
#undef min
#undef max

namespace SR {

namespace {

uint32_t ReadU32BE(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
        (static_cast<uint32_t>(data[1]) << 16) |
        (static_cast<uint32_t>(data[2]) << 8) |
        static_cast<uint32_t>(data[3]);
}

uint16_t ReadU16BE(const uint8_t* data) {
    return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

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

    uint32_t PeekBits(int count) const {
        return bitBuffer & ((1u << count) - 1u);
    }

    void DropBits(int count) {
        bitBuffer >>= count;
        bitCount -= count;
    }

    bool ReadBits(int count, uint32_t& out) {
        if (!EnsureBits(count)) {
            return false;
        }
        out = PeekBits(count);
        DropBits(count);
        return true;
    }

    bool ReadBit(bool& out) {
        uint32_t value = 0;
        if (!ReadBits(1, value)) {
            return false;
        }
        out = (value != 0);
        return true;
    }

    void AlignToByte() {
        int skip = bitCount % 8;
        if (skip > 0) {
            DropBits(skip);
        }
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
        if (len < 0 || len > maxBits) {
            return false;
        }
        if (len > 0) {
            blCount[len]++;
        }
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
        if (len == 0) {
            continue;
        }
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
    if (!reader.EnsureBits(maxBits)) {
        return false;
    }
    uint32_t entry = table[reader.PeekBits(maxBits)];
    if (entry == 0xFFFFFFFFu) {
        return false;
    }
    int len = static_cast<int>(entry >> 16);
    outSymbol = static_cast<int>(entry & 0xFFFFu);
    reader.DropBits(len);
    return true;
}

bool InflateZlib(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, std::string& outError) {
    output.clear();
    if (input.size() < 2) {
        outError = "Zlib data too small";
        return false;
    }

    uint8_t cmf = input[0];
    uint8_t flg = input[1];
    if ((cmf & 0x0F) != 8) {
        outError = "Unsupported zlib compression method";
        return false;
    }
    uint16_t check = static_cast<uint16_t>((cmf << 8) | flg);
    if (check % 31 != 0) {
        outError = "Invalid zlib header";
        return false;
    }
    if (flg & 0x20) {
        outError = "Zlib preset dictionary not supported";
        return false;
    }

    BitReader reader;
    reader.data = input.data() + 2;
    reader.size = input.size() - 2;

    std::vector<uint32_t> fixedLitTable;
    std::vector<uint32_t> fixedDistTable;
    std::vector<int> fixedLitLengths(288, 0);
    std::vector<int> fixedDistLengths(32, 5);
    for (int i = 0; i <= 143; ++i) fixedLitLengths[i] = 8;
    for (int i = 144; i <= 255; ++i) fixedLitLengths[i] = 9;
    for (int i = 256; i <= 279; ++i) fixedLitLengths[i] = 7;
    for (int i = 280; i <= 287; ++i) fixedLitLengths[i] = 8;
    BuildHuffmanTable(fixedLitLengths, 15, fixedLitTable);
    BuildHuffmanTable(fixedDistLengths, 15, fixedDistTable);

    constexpr int kLengthBase[29] = {
        3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
        35,43,51,59,67,83,99,115,131,163,195,227,258
    };
    constexpr int kLengthExtra[29] = {
        0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
        3,3,3,3,4,4,4,4,5,5,5,5,0
    };
    constexpr int kDistBase[30] = {
        1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,
        193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
    };
    constexpr int kDistExtra[30] = {
        0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
    };

    bool isFinal = false;
    while (!isFinal) {
        if (!reader.ReadBit(isFinal)) {
            outError = "Failed to read zlib block header";
            return false;
        }
        uint32_t blockType = 0;
        if (!reader.ReadBits(2, blockType)) {
            outError = "Failed to read zlib block type";
            return false;
        }

        if (blockType == 0) {
            reader.AlignToByte();
            if (!reader.EnsureBits(16)) {
                outError = "Invalid uncompressed block";
                return false;
            }
            uint32_t len = 0;
            uint32_t nlen = 0;
            reader.ReadBits(16, len);
            reader.ReadBits(16, nlen);
            if ((len ^ 0xFFFFu) != nlen) {
                outError = "Invalid uncompressed block length";
                return false;
            }
            if (reader.bytePos + len > reader.size) {
                outError = "Uncompressed block out of range";
                return false;
            }
            output.insert(output.end(), reader.data + reader.bytePos, reader.data + reader.bytePos + len);
            reader.bytePos += len;
        } else if (blockType == 1 || blockType == 2) {
            std::vector<uint32_t> litTable = fixedLitTable;
            std::vector<uint32_t> distTable = fixedDistTable;
            if (blockType == 2) {
                uint32_t hlit = 0;
                uint32_t hdist = 0;
                uint32_t hclen = 0;
                if (!reader.ReadBits(5, hlit) || !reader.ReadBits(5, hdist) || !reader.ReadBits(4, hclen)) {
                    outError = "Invalid dynamic Huffman header";
                    return false;
                }
                hlit += 257;
                hdist += 1;
                hclen += 4;
                static constexpr int kCodeOrder[19] = {
                    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
                };
                std::vector<int> codeLengths(19, 0);
                for (uint32_t i = 0; i < hclen; ++i) {
                    uint32_t len = 0;
                    if (!reader.ReadBits(3, len)) {
                        outError = "Invalid code length";
                        return false;
                    }
                    codeLengths[kCodeOrder[i]] = static_cast<int>(len);
                }
                std::vector<uint32_t> codeTable;
                if (!BuildHuffmanTable(codeLengths, 7, codeTable)) {
                    outError = "Failed to build code length table";
                    return false;
                }
                std::vector<int> litDistLengths;
                litDistLengths.reserve(hlit + hdist);
                while (litDistLengths.size() < hlit + hdist) {
                    int symbol = 0;
                    if (!DecodeSymbol(reader, codeTable, 7, symbol)) {
                        outError = "Invalid code length symbol";
                        return false;
                    }
                    if (symbol <= 15) {
                        litDistLengths.push_back(symbol);
                    } else if (symbol == 16) {
                        if (litDistLengths.empty()) {
                            outError = "Invalid repeat length";
                            return false;
                        }
                        uint32_t extra = 0;
                        if (!reader.ReadBits(2, extra)) {
                            outError = "Invalid repeat length extra bits";
                            return false;
                        }
                        int repeat = 3 + static_cast<int>(extra);
                        int prev = litDistLengths.back();
                        litDistLengths.insert(litDistLengths.end(), repeat, prev);
                    } else if (symbol == 17) {
                        uint32_t extra = 0;
                        if (!reader.ReadBits(3, extra)) {
                            outError = "Invalid zero repeat";
                            return false;
                        }
                        int repeat = 3 + static_cast<int>(extra);
                        litDistLengths.insert(litDistLengths.end(), repeat, 0);
                    } else if (symbol == 18) {
                        uint32_t extra = 0;
                        if (!reader.ReadBits(7, extra)) {
                            outError = "Invalid zero repeat";
                            return false;
                        }
                        int repeat = 11 + static_cast<int>(extra);
                        litDistLengths.insert(litDistLengths.end(), repeat, 0);
                    } else {
                        outError = "Invalid code length symbol";
                        return false;
                    }
                }
                std::vector<int> litLengths(litDistLengths.begin(), litDistLengths.begin() + hlit);
                std::vector<int> distLengths(litDistLengths.begin() + hlit, litDistLengths.end());
                if (!BuildHuffmanTable(litLengths, 15, litTable)) {
                    outError = "Failed to build literal table";
                    return false;
                }
                if (distLengths.empty()) {
                    distLengths.push_back(0);
                }
                if (!BuildHuffmanTable(distLengths, 15, distTable)) {
                    outError = "Failed to build distance table";
                    return false;
                }
            }

            while (true) {
                int symbol = 0;
                if (!DecodeSymbol(reader, litTable, 15, symbol)) {
                    outError = "Failed to decode symbol";
                    return false;
                }
                if (symbol < 256) {
                    output.push_back(static_cast<uint8_t>(symbol));
                } else if (symbol == 256) {
                    break;
                } else if (symbol >= 257 && symbol <= 285) {
                    int lengthIndex = symbol - 257;
                    int length = kLengthBase[lengthIndex];
                    int extraBits = kLengthExtra[lengthIndex];
                    if (extraBits > 0) {
                        uint32_t extra = 0;
                        if (!reader.ReadBits(extraBits, extra)) {
                            outError = "Invalid length extra bits";
                            return false;
                        }
                        length += static_cast<int>(extra);
                    }
                    int distSymbol = 0;
                    if (!DecodeSymbol(reader, distTable, 15, distSymbol)) {
                        outError = "Failed to decode distance";
                        return false;
                    }
                    if (distSymbol < 0 || distSymbol >= 30) {
                        outError = "Invalid distance symbol";
                        return false;
                    }
                    int distance = kDistBase[distSymbol];
                    int distExtra = kDistExtra[distSymbol];
                    if (distExtra > 0) {
                        uint32_t extra = 0;
                        if (!reader.ReadBits(distExtra, extra)) {
                            outError = "Invalid distance extra bits";
                            return false;
                        }
                        distance += static_cast<int>(extra);
                    }
                    if (distance <= 0 || static_cast<size_t>(distance) > output.size()) {
                        outError = "Invalid distance";
                        return false;
                    }
                    size_t start = output.size() - static_cast<size_t>(distance);
                    for (int i = 0; i < length; ++i) {
                        output.push_back(output[start + (i % distance)]);
                    }
                } else {
                    outError = "Invalid literal/length symbol";
                    return false;
                }
            }
        } else {
            outError = "Unsupported zlib block type";
            return false;
        }
    }

    return true;
}

uint8_t PaethPredictor(uint8_t a, uint8_t b, uint8_t c) {
    int p = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
    int pa = std::abs(p - static_cast<int>(a));
    int pb = std::abs(p - static_cast<int>(b));
    int pc = std::abs(p - static_cast<int>(c));
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

bool UnfilterScanlines(const std::vector<uint8_t>& input, int width, int height, int rowBytes, int bytesPerPixel, std::vector<uint8_t>& outData, std::string& outError) {
    size_t stride = static_cast<size_t>(rowBytes);
    size_t expected = (stride + 1) * static_cast<size_t>(height);
    if (input.size() < expected) {
        outError = "PNG data size mismatch";
        return false;
    }

    outData.assign(static_cast<size_t>(height) * stride, 0);
    const uint8_t* src = input.data();
    for (int y = 0; y < height; ++y) {
        uint8_t filter = *src++;
        uint8_t* dstRow = outData.data() + static_cast<size_t>(y) * stride;
        const uint8_t* prevRow = (y > 0) ? (outData.data() + static_cast<size_t>(y - 1) * stride) : nullptr;
        for (size_t x = 0; x < stride; ++x) {
            uint8_t raw = src[x];
            uint8_t left = (x >= static_cast<size_t>(bytesPerPixel)) ? dstRow[x - bytesPerPixel] : 0;
            uint8_t up = prevRow ? prevRow[x] : 0;
            uint8_t upLeft = (prevRow && x >= static_cast<size_t>(bytesPerPixel)) ? prevRow[x - bytesPerPixel] : 0;
            uint8_t value = 0;
            switch (filter) {
                case 0: value = raw; break;
                case 1: value = static_cast<uint8_t>(raw + left); break;
                case 2: value = static_cast<uint8_t>(raw + up); break;
                case 3: value = static_cast<uint8_t>(raw + static_cast<uint8_t>((static_cast<int>(left) + static_cast<int>(up)) / 2)); break;
                case 4: value = static_cast<uint8_t>(raw + PaethPredictor(left, up, upLeft)); break;
                default:
                    outError = "Unsupported PNG filter";
                    return false;
            }
            dstRow[x] = value;
        }
        src += stride;
    }

    return true;
}

bool ExpandIndexed(const std::vector<uint8_t>& filtered, int width, int height, int bitDepth,
    const std::vector<uint8_t>& palette, const std::vector<uint8_t>& alphaTable,
    std::vector<uint8_t>& outRGBA, std::string& outError) {
    if (palette.empty() || palette.size() % 3 != 0) {
        outError = "PNG palette missing";
        return false;
    }

    int paletteEntries = static_cast<int>(palette.size() / 3);
    outRGBA.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

    auto getAlpha = [&](int index) -> uint8_t {
        if (index < 0 || index >= paletteEntries) {
            return 255;
        }
        if (index < static_cast<int>(alphaTable.size())) {
            return alphaTable[static_cast<size_t>(index)];
        }
        return 255;
    };

    size_t dstIndex = 0;
    size_t srcIndex = 0;
    for (int y = 0; y < height; ++y) {
        int bitPos = 0;
        uint8_t current = 0;
        for (int x = 0; x < width; ++x) {
            if (bitPos == 0) {
                current = filtered[srcIndex++];
            }
            int shift = 8 - bitDepth - bitPos;
            int mask = (1 << bitDepth) - 1;
            int index = (current >> shift) & mask;
            bitPos += bitDepth;
            if (bitPos >= 8) {
                bitPos = 0;
            }

            if (index >= paletteEntries) {
                outError = "PNG palette index out of range";
                return false;
            }

            size_t pal = static_cast<size_t>(index) * 3;
            outRGBA[dstIndex + 0] = palette[pal + 0];
            outRGBA[dstIndex + 1] = palette[pal + 1];
            outRGBA[dstIndex + 2] = palette[pal + 2];
            outRGBA[dstIndex + 3] = getAlpha(index);
            dstIndex += 4;
        }

        // align to next byte if not already
        if (bitPos != 0) {
            bitPos = 0;
        }
    }

    return true;
}

bool DecodePNG(const std::vector<uint8_t>& data, GLTFImage& outImage, std::string& outError) {
    static constexpr uint8_t kSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (data.size() < 8 || !std::equal(kSignature, kSignature + 8, data.begin())) {
        outError = "Invalid PNG signature";
        return false;
    }

    size_t pos = 8;
    int width = 0;
    int height = 0;
    int bitDepth = 0;
    int colorType = 0;
    int interlace = 0;
    std::vector<uint8_t> idat;
    std::vector<uint8_t> palette;
    std::vector<uint8_t> paletteAlpha;
    bool hasIHDR = false;

    while (pos + 8 <= data.size()) {
        uint32_t length = ReadU32BE(data.data() + pos);
        pos += 4;
        if (pos + 4 > data.size()) {
            outError = "PNG chunk truncated";
            return false;
        }
        std::string type(reinterpret_cast<const char*>(data.data() + pos), 4);
        pos += 4;
        if (pos + length + 4 > data.size()) {
            outError = "PNG chunk out of bounds";
            return false;
        }
        const uint8_t* chunkData = data.data() + pos;
        if (type == "IHDR") {
            if (length < 13) {
                outError = "Invalid IHDR length";
                return false;
            }
            width = static_cast<int>(ReadU32BE(chunkData));
            height = static_cast<int>(ReadU32BE(chunkData + 4));
            bitDepth = chunkData[8];
            colorType = chunkData[9];
            int compression = chunkData[10];
            int filter = chunkData[11];
            interlace = chunkData[12];
            if (compression != 0 || filter != 0) {
                outError = "Unsupported PNG compression or filter method";
                return false;
            }
            hasIHDR = true;
        } else if (type == "PLTE") {
            palette.assign(chunkData, chunkData + length);
        } else if (type == "tRNS") {
            paletteAlpha.assign(chunkData, chunkData + length);
        } else if (type == "IDAT") {
            idat.insert(idat.end(), chunkData, chunkData + length);
        } else if (type == "IEND") {
            break;
        }
        pos += length + 4; // skip CRC
    }

    if (!hasIHDR || idat.empty()) {
        outError = "Missing IHDR or IDAT";
        return false;
    }

    if (interlace != 0) {
        outError = "Interlaced PNG not supported";
        return false;
    }

    int bpp = 0;
    int rowBytes = 0;
    switch (colorType) {
        case 0:
            if (bitDepth != 8) {
                outError = "Only 8-bit grayscale PNG is supported";
                return false;
            }
            bpp = 1;
            rowBytes = width * bpp;
            break;
        case 2:
            if (bitDepth != 8) {
                outError = "Only 8-bit RGB PNG is supported";
                return false;
            }
            bpp = 3;
            rowBytes = width * bpp;
            break;
        case 4:
            if (bitDepth != 8) {
                outError = "Only 8-bit grayscale+alpha PNG is supported";
                return false;
            }
            bpp = 2;
            rowBytes = width * bpp;
            break;
        case 6:
            if (bitDepth != 8) {
                outError = "Only 8-bit RGBA PNG is supported";
                return false;
            }
            bpp = 4;
            rowBytes = width * bpp;
            break;
        case 3:
            if (bitDepth != 1 && bitDepth != 2 && bitDepth != 4 && bitDepth != 8) {
                outError = "Unsupported indexed PNG bit depth";
                return false;
            }
            bpp = 1;
            rowBytes = (width * bitDepth + 7) / 8;
            break;
        default:
            outError = "Unsupported PNG color type";
            return false;
    }

    std::vector<uint8_t> decompressed;
    if (!InflateZlib(idat, decompressed, outError)) {
        return false;
    }

    std::vector<uint8_t> scanlines;
    if (!UnfilterScanlines(decompressed, width, height, rowBytes, bpp, scanlines, outError)) {
        return false;
    }

    if (colorType == 3) {
        if (!ExpandIndexed(scanlines, width, height, bitDepth, palette, paletteAlpha, outImage.pixels, outError)) {
            return false;
        }
        outImage.width = width;
        outImage.height = height;
        outImage.channels = 4;
        return true;
    }

    outImage.width = width;
    outImage.height = height;
    outImage.channels = 4;
    outImage.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

    const uint8_t* src = scanlines.data();
    uint8_t* dst = outImage.pixels.data();
    size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    for (size_t i = 0; i < pixelCount; ++i) {
        if (colorType == 2) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = 255;
            src += 3;
        } else if (colorType == 6) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            src += 4;
        } else if (colorType == 0) {
            uint8_t g = src[0];
            dst[0] = g;
            dst[1] = g;
            dst[2] = g;
            dst[3] = 255;
            src += 1;
        } else if (colorType == 4) {
            uint8_t g = src[0];
            dst[0] = g;
            dst[1] = g;
            dst[2] = g;
            dst[3] = src[1];
            src += 2;
        }
        dst += 4;
    }

    return true;
}

struct JpegHuffmanTable {
    bool defined = false;
    std::vector<uint32_t> table;
};

struct JpegQuantTable {
    bool defined = false;
    std::array<uint16_t, 64> values{};
};

struct JpegComponent {
    int id = 0;
    int hFactor = 1;
    int vFactor = 1;
    int quantId = 0;
    int dcTable = 0;
    int acTable = 0;
    int width = 0;
    int height = 0;
    int dcPred = 0;
    std::vector<uint8_t> plane;
};

struct JpegBitReader {
    const uint8_t* data = nullptr;
    size_t size = 0;
    size_t pos = 0;
    uint32_t bitBuffer = 0;
    int bitCount = 0;
    bool markerFound = false;
    uint8_t marker = 0;
    bool endOfData = false;

    bool Fill(int count) {
        while (bitCount < count && !endOfData) {
            if (pos >= size) {
                endOfData = true;
                break;
            }
            uint8_t byte = data[pos++];
            if (byte == 0xFF) {
                if (pos >= size) {
                    endOfData = true;
                    break;
                }
                uint8_t next = data[pos++];
                if (next == 0x00) {
                    byte = 0xFF;
                } else if (next >= 0xD0 && next <= 0xD7) {
                    bitBuffer = 0;
                    bitCount = 0;
                    continue;
                } else {
                    markerFound = true;
                    marker = next;
                    endOfData = true;
                    break;
                }
            }
            bitBuffer = (bitBuffer << 8) | byte;
            bitCount += 8;
        }
        return bitCount >= count;
    }

    bool PeekBits(int count, uint32_t& out) {
        if (!Fill(count)) {
            return false;
        }
        out = (bitBuffer >> (bitCount - count)) & ((1u << count) - 1u);
        return true;
    }

    bool ReadBits(int count, uint32_t& out) {
        if (!PeekBits(count, out)) {
            return false;
        }
        bitCount -= count;
        if (bitCount == 0) {
            bitBuffer = 0;
        } else {
            bitBuffer &= (1u << bitCount) - 1u;
        }
        return true;
    }
};

constexpr int kZigZag[64] = {
    0, 1, 5, 6, 14, 15, 27, 28,
    2, 4, 7, 13, 16, 26, 29, 42,
    3, 8, 12, 17, 25, 30, 41, 43,
    9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

bool BuildJpegHuffmanTable(const std::array<uint8_t, 16>& counts, const std::vector<uint8_t>& symbols, JpegHuffmanTable& outTable) {
    size_t symbolIndex = 0;
    int code = 0;
    outTable.table.assign(1u << 16, 0xFFFFFFFFu);
    for (int len = 1; len <= 16; ++len) {
        int count = counts[static_cast<size_t>(len - 1)];
        for (int i = 0; i < count; ++i) {
            if (symbolIndex >= symbols.size()) {
                return false;
            }
            uint8_t symbol = symbols[symbolIndex++];
            uint32_t entry = (static_cast<uint32_t>(len) << 16) | symbol;
            uint32_t fill = 1u << (16 - len);
            uint32_t base = static_cast<uint32_t>(code) << (16 - len);
            for (uint32_t j = 0; j < fill; ++j) {
                outTable.table[base | j] = entry;
            }
            ++code;
        }
        code <<= 1;
    }
    outTable.defined = true;
    return true;
}

bool DecodeJpegSymbol(JpegBitReader& reader, const JpegHuffmanTable& table, int& outSymbol) {
    if (!table.defined) {
        return false;
    }
    uint32_t bits = 0;
    if (!reader.PeekBits(16, bits)) {
        return false;
    }
    uint32_t entry = table.table[bits];
    if (entry == 0xFFFFFFFFu) {
        return false;
    }
    int len = static_cast<int>(entry >> 16);
    outSymbol = static_cast<int>(entry & 0xFFFFu);
    uint32_t discard = 0;
    return reader.ReadBits(len, discard);
}

int ExtendSign(int value, int bits) {
    if (bits == 0) {
        return 0;
    }
    int vt = 1 << (bits - 1);
    if (value < vt) {
        value -= (1 << bits) - 1;
    }
    return value;
}

void IDCT8x8(const int16_t* inBlock, uint8_t* out, int outStride) {
    static double cosTable[8][8];
    static bool initialized = false;
    constexpr double kPi = 3.14159265358979323846;
    if (!initialized) {
        for (int v = 0; v < 8; ++v) {
            for (int x = 0; x < 8; ++x) {
                cosTable[v][x] = std::cos(((2.0 * x + 1.0) * v * kPi) / 16.0);
            }
        }
        initialized = true;
    }

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            double sum = 0.0;
            for (int v = 0; v < 8; ++v) {
                double cv = (v == 0) ? (1.0 / std::sqrt(2.0)) : 1.0;
                for (int u = 0; u < 8; ++u) {
                    double cu = (u == 0) ? (1.0 / std::sqrt(2.0)) : 1.0;
                    double basis = cosTable[u][x] * cosTable[v][y];
                    sum += cu * cv * static_cast<double>(inBlock[v * 8 + u]) * basis;
                }
            }
            int value = static_cast<int>(std::round(sum / 4.0)) + 128;
            value = std::clamp(value, 0, 255);
            out[y * outStride + x] = static_cast<uint8_t>(value);
        }
    }
}

bool DecodeJpegScan(const uint8_t* data, size_t size, int width, int height,
    std::vector<JpegComponent>& components, const std::array<JpegQuantTable, 4>& quantTables,
    const std::array<JpegHuffmanTable, 4>& dcTables, const std::array<JpegHuffmanTable, 4>& acTables,
    int restartInterval, GLTFImage& outImage, std::string& outError) {

    int maxH = 1;
    int maxV = 1;
    for (const auto& comp : components) {
        maxH = (maxH > comp.hFactor) ? maxH : comp.hFactor;
        maxV = (maxV > comp.vFactor) ? maxV : comp.vFactor;
    }

    int mcuWidth = maxH * 8;
    int mcuHeight = maxV * 8;
    int mcuCountX = (width + mcuWidth - 1) / mcuWidth;
    int mcuCountY = (height + mcuHeight - 1) / mcuHeight;

    for (auto& comp : components) {
        comp.width = (width * comp.hFactor + maxH - 1) / maxH;
        comp.height = (height * comp.vFactor + maxV - 1) / maxV;
        comp.plane.assign(static_cast<size_t>(comp.width) * static_cast<size_t>(comp.height), 0);
        comp.dcPred = 0;
    }

    JpegBitReader reader;
    reader.data = data;
    reader.size = size;

    int restartCountdown = restartInterval;
    for (int my = 0; my < mcuCountY; ++my) {
        for (int mx = 0; mx < mcuCountX; ++mx) {
            for (auto& comp : components) {
                if (comp.quantId < 0 || comp.quantId >= 4 || !quantTables[comp.quantId].defined) {
                    outError = "JPEG quant table missing";
                    return false;
                }
                const auto& qtable = quantTables[comp.quantId].values;
                for (int vy = 0; vy < comp.vFactor; ++vy) {
                    for (int hx = 0; hx < comp.hFactor; ++hx) {
                        int16_t block[64] = {};
                        int symbol = 0;
                        if (!DecodeJpegSymbol(reader, dcTables[comp.dcTable], symbol)) {
                            outError = "JPEG DC symbol decode failed";
                            return false;
                        }
                        uint32_t bits = 0;
                        if (symbol > 0 && !reader.ReadBits(symbol, bits)) {
                            outError = "JPEG DC bits decode failed";
                            return false;
                        }
                        int diff = ExtendSign(static_cast<int>(bits), symbol);
                        comp.dcPred += diff;
                        block[0] = static_cast<int16_t>(comp.dcPred * static_cast<int>(qtable[0]));

                        int k = 1;
                        while (k < 64) {
                            if (!DecodeJpegSymbol(reader, acTables[comp.acTable], symbol)) {
                                outError = "JPEG AC symbol decode failed";
                                return false;
                            }
                            int run = (symbol >> 4) & 0xF;
                            int size = symbol & 0xF;
                            if (size == 0) {
                                if (run == 0) {
                                    break;
                                }
                                if (run == 15) {
                                    k += 16;
                                    continue;
                                }
                            }
                            k += run;
                            if (k >= 64) {
                                break;
                            }
                            uint32_t coeffBits = 0;
                            if (size > 0 && !reader.ReadBits(size, coeffBits)) {
                                outError = "JPEG AC bits decode failed";
                                return false;
                            }
                            int coeff = ExtendSign(static_cast<int>(coeffBits), size);
                            int index = kZigZag[k];
                            block[index] = static_cast<int16_t>(coeff * static_cast<int>(qtable[index]));
                            ++k;
                        }

                        int blockX = (mx * comp.hFactor + hx) * 8;
                        int blockY = (my * comp.vFactor + vy) * 8;
                        uint8_t blockPixels[64];
                        IDCT8x8(block, blockPixels, 8);

                        for (int by = 0; by < 8; ++by) {
                            int py = blockY + by;
                            if (py >= comp.height) {
                                continue;
                            }
                            for (int bx = 0; bx < 8; ++bx) {
                                int px = blockX + bx;
                                if (px >= comp.width) {
                                    continue;
                                }
                                comp.plane[static_cast<size_t>(py) * comp.width + px] = blockPixels[by * 8 + bx];
                            }
                        }
                    }
                }
            }

            if (restartInterval > 0) {
                --restartCountdown;
                if (restartCountdown == 0) {
                    reader.bitBuffer = 0;
                    reader.bitCount = 0;
                    for (auto& comp : components) {
                        comp.dcPred = 0;
                    }
                    restartCountdown = restartInterval;
                }
            }
        }
    }

    outImage.width = width;
    outImage.height = height;
    outImage.channels = 4;
    outImage.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

    if (components.size() == 1) {
        const auto& c = components[0];
        for (int y = 0; y < height; ++y) {
            int sy = y * c.height / height;
            for (int x = 0; x < width; ++x) {
                int sx = x * c.width / width;
                uint8_t v = c.plane[static_cast<size_t>(sy) * c.width + sx];
                size_t dstIndex = (static_cast<size_t>(y) * width + x) * 4;
                outImage.pixels[dstIndex + 0] = v;
                outImage.pixels[dstIndex + 1] = v;
                outImage.pixels[dstIndex + 2] = v;
                outImage.pixels[dstIndex + 3] = 255;
            }
        }
        return true;
    }

    if (components.size() < 3) {
        outError = "JPEG missing color components";
        return false;
    }

    const auto& yComp = components[0];
    const auto& cbComp = components[1];
    const auto& crComp = components[2];

    for (int y = 0; y < height; ++y) {
        int yY = y * yComp.height / height;
        int yCb = y * cbComp.height / height;
        int yCr = y * crComp.height / height;
        for (int x = 0; x < width; ++x) {
            int xY = x * yComp.width / width;
            int xCb = x * cbComp.width / width;
            int xCr = x * crComp.width / width;
            int Y = yComp.plane[static_cast<size_t>(yY) * yComp.width + xY];
            int Cb = cbComp.plane[static_cast<size_t>(yCb) * cbComp.width + xCb] - 128;
            int Cr = crComp.plane[static_cast<size_t>(yCr) * crComp.width + xCr] - 128;
            int R = static_cast<int>(std::round(Y + 1.402 * Cr));
            int G = static_cast<int>(std::round(Y - 0.344136 * Cb - 0.714136 * Cr));
            int B = static_cast<int>(std::round(Y + 1.772 * Cb));
            R = std::clamp(R, 0, 255);
            G = std::clamp(G, 0, 255);
            B = std::clamp(B, 0, 255);
            size_t dstIndex = (static_cast<size_t>(y) * width + x) * 4;
            outImage.pixels[dstIndex + 0] = static_cast<uint8_t>(R);
            outImage.pixels[dstIndex + 1] = static_cast<uint8_t>(G);
            outImage.pixels[dstIndex + 2] = static_cast<uint8_t>(B);
            outImage.pixels[dstIndex + 3] = 255;
        }
    }

    return true;
}

bool DecodeJPEG(const std::vector<uint8_t>& data, GLTFImage& outImage, std::string& outError) {
    if (data.size() < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        outError = "Invalid JPEG signature";
        return false;
    }

    size_t pos = 2;
    int width = 0;
    int height = 0;
    int precision = 0;
    int restartInterval = 0;

    std::array<JpegQuantTable, 4> quantTables{};
    std::array<JpegHuffmanTable, 4> dcTables{};
    std::array<JpegHuffmanTable, 4> acTables{};
    std::vector<JpegComponent> components;

    while (pos + 1 < data.size()) {
        if (data[pos] != 0xFF) {
            outError = "Invalid JPEG marker";
            return false;
        }
        while (pos < data.size() && data[pos] == 0xFF) {
            ++pos;
        }
        if (pos >= data.size()) {
            break;
        }
        uint8_t marker = data[pos++];
        if (marker == 0xD9) {
            break;
        }
        if (marker == 0xDA) {
            if (pos + 2 > data.size()) {
                outError = "Invalid SOS length";
                return false;
            }
            uint16_t length = ReadU16BE(data.data() + pos);
            pos += 2;
            if (pos + length - 2 > data.size()) {
                outError = "SOS segment out of bounds";
                return false;
            }
            int count = data[pos++];
            for (int i = 0; i < count; ++i) {
                uint8_t id = data[pos++];
                uint8_t tableSel = data[pos++];
                int dcId = (tableSel >> 4) & 0xF;
                int acId = tableSel & 0xF;
                for (auto& comp : components) {
                    if (comp.id == id) {
                        comp.dcTable = dcId;
                        comp.acTable = acId;
                        break;
                    }
                }
            }
            pos += 3; // spectral selection + approximation

            if (width <= 0 || height <= 0 || components.empty()) {
                outError = "JPEG SOF0 missing";
                return false;
            }

            size_t scanStart = pos;
            size_t scanEnd = data.size();
            if (!DecodeJpegScan(data.data() + scanStart, scanEnd - scanStart, width, height, components, quantTables, dcTables, acTables, restartInterval, outImage, outError)) {
                return false;
            }
            return true;
        }

        if (pos + 2 > data.size()) {
            outError = "Invalid JPEG segment length";
            return false;
        }
        uint16_t length = ReadU16BE(data.data() + pos);
        pos += 2;
        if (length < 2 || pos + length - 2 > data.size()) {
            outError = "JPEG segment out of bounds";
            return false;
        }
        const uint8_t* segment = data.data() + pos;

        if (marker == 0xDB) {
            size_t offset = 0;
            while (offset + 1 < length - 2) {
                uint8_t pqTq = segment[offset++];
                int precisionBits = (pqTq >> 4) & 0xF;
                int tableId = pqTq & 0xF;
                if (tableId < 0 || tableId >= 4) {
                    outError = "Invalid JPEG quant table id";
                    return false;
                }
                if (precisionBits != 0) {
                    outError = "Only 8-bit JPEG quant tables supported";
                    return false;
                }
                if (offset + 64 > length - 2) {
                    outError = "Invalid JPEG quant table length";
                    return false;
                }
                for (int i = 0; i < 64; ++i) {
                    quantTables[tableId].values[kZigZag[i]] = segment[offset++];
                }
                quantTables[tableId].defined = true;
            }
        } else if (marker == 0xC0) {
            if (length < 8) {
                outError = "Invalid SOF0 length";
                return false;
            }
            precision = segment[0];
            height = static_cast<int>(ReadU16BE(segment + 1));
            width = static_cast<int>(ReadU16BE(segment + 3));
            int compCount = segment[5];
            if (precision != 8) {
                outError = "Only 8-bit JPEG supported";
                return false;
            }
            components.clear();
            size_t offset = 6;
            for (int i = 0; i < compCount; ++i) {
                if (offset + 3 > length - 2) {
                    outError = "Invalid SOF0 component data";
                    return false;
                }
                JpegComponent comp;
                comp.id = segment[offset++];
                uint8_t sampling = segment[offset++];
                comp.hFactor = (sampling >> 4) & 0xF;
                comp.vFactor = sampling & 0xF;
                comp.quantId = segment[offset++];
                components.push_back(comp);
            }
        } else if (marker == 0xC4) {
            size_t offset = 0;
            while (offset + 17 <= length - 2) {
                uint8_t tcTh = segment[offset++];
                int tableClass = (tcTh >> 4) & 0xF;
                int tableId = tcTh & 0xF;
                if (tableId < 0 || tableId >= 4) {
                    outError = "Invalid JPEG Huffman table id";
                    return false;
                }
                std::array<uint8_t, 16> counts{};
                int totalSymbols = 0;
                for (int i = 0; i < 16; ++i) {
                    counts[static_cast<size_t>(i)] = segment[offset++];
                    totalSymbols += counts[static_cast<size_t>(i)];
                }
                if (offset + totalSymbols > length - 2) {
                    outError = "Invalid JPEG Huffman table length";
                    return false;
                }
                std::vector<uint8_t> symbols;
                symbols.reserve(static_cast<size_t>(totalSymbols));
                for (int i = 0; i < totalSymbols; ++i) {
                    symbols.push_back(segment[offset++]);
                }
                if (tableClass == 0) {
                    if (!BuildJpegHuffmanTable(counts, symbols, dcTables[tableId])) {
                        outError = "Failed to build JPEG DC Huffman table";
                        return false;
                    }
                } else if (tableClass == 1) {
                    if (!BuildJpegHuffmanTable(counts, symbols, acTables[tableId])) {
                        outError = "Failed to build JPEG AC Huffman table";
                        return false;
                    }
                } else {
                    outError = "Unsupported JPEG Huffman table class";
                    return false;
                }
            }
        } else if (marker == 0xDD) {
            if (length < 4) {
                outError = "Invalid DRI length";
                return false;
            }
            restartInterval = static_cast<int>(ReadU16BE(segment));
        }

        pos += length - 2;
    }

    outError = "JPEG missing SOS segment";
    return false;
}

bool IsPNG(const std::vector<uint8_t>& data) {
    static constexpr uint8_t kSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    return data.size() >= 8 && std::equal(kSignature, kSignature + 8, data.begin());
}

bool IsJPEG(const std::vector<uint8_t>& data) {
    return data.size() >= 2 && data[0] == 0xFF && data[1] == 0xD8;
}

} // namespace

bool ImageDecoder::Decode(const std::vector<uint8_t>& data, const std::string& mimeType, GLTFImage& outImage) {
    using Clock = std::chrono::high_resolution_clock;
    auto t0 = Clock::now();
    m_lastError.clear();

    if (data.empty()) {
        m_lastError = "Image data is empty";
        return false;
    }

    std::string resolvedMime = mimeType;
    if (resolvedMime.empty()) {
        if (IsPNG(data)) {
            resolvedMime = "image/png";
        } else if (IsJPEG(data)) {
            resolvedMime = "image/jpeg";
        }
    }

    if (resolvedMime == "image/png") {
        if (!DecodePNG(data, outImage, m_lastError)) {
            return false;
        }
        auto t1 = Clock::now();
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "Image decode PNG(ms): %.3f\n",
            std::chrono::duration<double, std::milli>(t1 - t0).count());
        OutputDebugStringA(buffer);
        return true;
    }
    if (resolvedMime == "image/jpeg") {
        if (!DecodeJPEG(data, outImage, m_lastError)) {
            return false;
        }
        auto t1 = Clock::now();
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "Image decode JPEG(ms): %.3f\n",
            std::chrono::duration<double, std::milli>(t1 - t0).count());
        OutputDebugStringA(buffer);
        return true;
    }

    m_lastError = "Unsupported image mimeType: " + resolvedMime;
    return false;
}

const std::string& ImageDecoder::GetLastError() const {
    return m_lastError;
}

} // namespace SR

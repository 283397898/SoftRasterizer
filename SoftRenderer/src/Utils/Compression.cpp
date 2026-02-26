#include "Utils/Compression.h"

namespace SR {

namespace {

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

} // namespace

bool InflateDeflate(const uint8_t* data, size_t size, std::vector<uint8_t>& output, std::string& outError) {
    output.clear();
    if (!data || size == 0) {
        outError = "Deflate data too small";
        return false;
    }

    BitReader reader;
    reader.data = data;
    reader.size = size;

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
            outError = "Failed to read deflate block header";
            return false;
        }
        uint32_t blockType = 0;
        if (!reader.ReadBits(2, blockType)) {
            outError = "Failed to read deflate block type";
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
                uint32_t hlit = 0, hdist = 0, hclen = 0;
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
                        litDistLengths.insert(litDistLengths.end(), repeat, litDistLengths.back());
                    } else if (symbol == 17) {
                        uint32_t extra = 0;
                        if (!reader.ReadBits(3, extra)) {
                            outError = "Invalid zero repeat";
                            return false;
                        }
                        litDistLengths.insert(litDistLengths.end(), 3 + static_cast<int>(extra), 0);
                    } else if (symbol == 18) {
                        uint32_t extra = 0;
                        if (!reader.ReadBits(7, extra)) {
                            outError = "Invalid zero repeat";
                            return false;
                        }
                        litDistLengths.insert(litDistLengths.end(), 11 + static_cast<int>(extra), 0);
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
            outError = "Unsupported deflate block type";
            return false;
        }
    }

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

    return InflateDeflate(input.data() + 2, input.size() - 2, output, outError);
}

} // namespace SR

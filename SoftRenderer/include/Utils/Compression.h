#pragma once

/**
 * @file Compression.h
 * @brief 统一的 Zlib/Deflate 解压接口，替代 ImageDecoder 和 EXRDecoder 中的重复实现。
 *
 * 实现基于 RFC 1950 (Zlib) 和 RFC 1951 (DEFLATE)，不依赖第三方库。
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SR {

/**
 * @brief 解压原始 DEFLATE 数据流（无 Zlib 头）
 * @param data     压缩数据起始指针
 * @param size     压缩数据字节长度
 * @param output   解压后的数据输出缓冲区
 * @param outError 失败时填入错误描述
 * @return 成功返回 true，失败返回 false 并设置 outError
 */
bool InflateDeflate(const uint8_t* data, size_t size, std::vector<uint8_t>& output, std::string& outError);

/**
 * @brief 解压 Zlib 封装的 DEFLATE 数据（带 2 字节 Zlib 头和 4 字节 Adler-32 尾）
 * @param input    Zlib 格式的压缩数据
 * @param output   解压后的数据输出缓冲区
 * @param outError 失败时填入错误描述
 * @return 成功返回 true，失败返回 false 并设置 outError
 */
bool InflateZlib(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, std::string& outError);

} // namespace SR

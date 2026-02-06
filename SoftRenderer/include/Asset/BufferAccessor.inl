#pragma once

#include "Asset/BufferAccessor.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Math/Vec4.h"

namespace SR {

template <typename T>
std::vector<T> BufferAccessor::Read(const GLTFAsset& asset, const GLTFAccessor& accessor) const {
    if (accessor.bufferViewIndex < 0 || accessor.bufferViewIndex >= static_cast<int>(asset.bufferViews.size())) {
        return {};
    }

    const GLTFBufferView& view = asset.bufferViews[accessor.bufferViewIndex];
    if (view.bufferIndex < 0 || view.bufferIndex >= static_cast<int>(asset.buffers.size())) {
        return {};
    }

    const GLTFBuffer& buffer = asset.buffers[view.bufferIndex];
    size_t baseOffset = view.byteOffset + accessor.byteOffset;

    auto componentSize = [](int componentType) -> size_t {
        switch (componentType) {
            case 5120: return 1; // BYTE
            case 5121: return 1; // UNSIGNED_BYTE
            case 5122: return 2; // SHORT
            case 5123: return 2; // UNSIGNED_SHORT
            case 5125: return 4; // UNSIGNED_INT
            case 5126: return 4; // FLOAT
            default: return 0;
        }
    };

    auto componentCount = [](int type) -> size_t {
        switch (type) {
            case 1: return 1; // SCALAR
            case 2: return 2; // VEC2
            case 3: return 3; // VEC3
            case 4: return 4; // VEC4
            case 9: return 9; // MAT3
            case 16: return 16; // MAT4
            default: return 0;
        }
    };

    const size_t compSize = componentSize(accessor.componentType);
    const size_t compCount = componentCount(accessor.type);
    if (compSize == 0 || compCount == 0) {
        return {};
    }

    const size_t elementSize = compSize * compCount;
    const size_t stride = view.byteStride > 0 ? view.byteStride : elementSize;
    if (baseOffset + stride * accessor.count > buffer.data.size()) {
        return {};
    }

    std::vector<T> out;
    out.reserve(accessor.count);

    auto readComponent = [&](const uint8_t* ptr) -> double {
        switch (accessor.componentType) {
            case 5120: {
                int8_t v = 0;
                std::memcpy(&v, ptr, sizeof(v));
                if (accessor.normalized) {
                    double maxValue = static_cast<double>((std::numeric_limits<int8_t>::max)());
                    double minValue = static_cast<double>((std::numeric_limits<int8_t>::min)());
                    (void)minValue;
                    return (std::max)(-1.0, static_cast<double>(v) / maxValue);
                }
                return static_cast<double>(v);
            }
            case 5121: {
                uint8_t v = 0;
                std::memcpy(&v, ptr, sizeof(v));
                if (accessor.normalized) {
                    double maxValue = static_cast<double>((std::numeric_limits<uint8_t>::max)());
                    return static_cast<double>(v) / maxValue;
                }
                return static_cast<double>(v);
            }
            case 5122: {
                int16_t v = 0;
                std::memcpy(&v, ptr, sizeof(v));
                if (accessor.normalized) {
                    double maxValue = static_cast<double>((std::numeric_limits<int16_t>::max)());
                    return (std::max)(-1.0, static_cast<double>(v) / maxValue);
                }
                return static_cast<double>(v);
            }
            case 5123: {
                uint16_t v = 0;
                std::memcpy(&v, ptr, sizeof(v));
                if (accessor.normalized) {
                    double maxValue = static_cast<double>((std::numeric_limits<uint16_t>::max)());
                    return static_cast<double>(v) / maxValue;
                }
                return static_cast<double>(v);
            }
            case 5125: {
                uint32_t v = 0;
                std::memcpy(&v, ptr, sizeof(v));
                if (accessor.normalized) {
                    double maxValue = static_cast<double>((std::numeric_limits<uint32_t>::max)());
                    return static_cast<double>(v) / maxValue;
                }
                return static_cast<double>(v);
            }
            case 5126: {
                float v = 0.0f;
                std::memcpy(&v, ptr, sizeof(v));
                return static_cast<double>(v);
            }
            default:
                return 0.0;
        }
    };

    for (size_t i = 0; i < accessor.count; ++i) {
        const uint8_t* elementPtr = buffer.data.data() + baseOffset + i * stride;

        if constexpr (std::is_same_v<T, Vec2>) {
            if (compCount < 2) {
                out.emplace_back();
                continue;
            }
            double x = readComponent(elementPtr + 0 * compSize);
            double y = readComponent(elementPtr + 1 * compSize);
            out.emplace_back(x, y);
        } else if constexpr (std::is_same_v<T, Vec3>) {
            if (compCount < 3) {
                out.emplace_back();
                continue;
            }
            double x = readComponent(elementPtr + 0 * compSize);
            double y = readComponent(elementPtr + 1 * compSize);
            double z = readComponent(elementPtr + 2 * compSize);
            out.emplace_back(x, y, z);
        } else if constexpr (std::is_same_v<T, Vec4>) {
            if (compCount < 4) {
                out.emplace_back();
                continue;
            }
            double x = readComponent(elementPtr + 0 * compSize);
            double y = readComponent(elementPtr + 1 * compSize);
            double z = readComponent(elementPtr + 2 * compSize);
            double w = readComponent(elementPtr + 3 * compSize);
            out.emplace_back(x, y, z, w);
        } else if constexpr (std::is_floating_point_v<T>) {
            if (compCount != 1) {
                out.emplace_back();
                continue;
            }
            double v = readComponent(elementPtr);
            out.push_back(static_cast<T>(v));
        } else if constexpr (std::is_integral_v<T>) {
            if (compCount != 1) {
                out.emplace_back();
                continue;
            }
            double v = readComponent(elementPtr);
            out.push_back(static_cast<T>(v));
        } else {
            out.emplace_back();
        }
    }

    return out;
}

} // namespace SR

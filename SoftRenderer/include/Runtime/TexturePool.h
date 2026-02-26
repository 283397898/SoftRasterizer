#pragma once

#include "Runtime/ResourcePool.h"
#include "Core/Texture.h"

namespace SR {

/**
 * @brief 纹理资源池
 *
 * 特化版本，添加纹理特定的功能：
 * - 内存使用估算
 * - 纹理查找
 */
class TexturePool : public ResourcePool<Texture> {
public:
    using Handle = ResourcePool<Texture>::Handle;

    /**
     * @brief 分配纹理并设置尺寸
     * @param width 纹理宽度
     * @param height 纹理高度
     * @param pixels 像素数据
     * @return 纹理句柄
     */
    Handle AllocateTexture(int width, int height, std::vector<uint32_t> pixels) {
        Handle h = Allocate();
        Texture* tex = Get(h);
        if (tex) {
            tex->SetPixels(width, height, std::move(pixels));
            UpdateTextureMemory(h);
        }
        return h;
    }

    /**
     * @brief 获取纹理内存使用量估算
     * @param h 纹理句柄
     * @return 字节数
     */
    size_t GetTextureMemory(Handle h) const {
        const Texture* tex = Get(h);
        if (!tex) return 0;
        return static_cast<size_t>(tex->GetWidth()) * tex->GetHeight() * 4; // BGRA8
    }

    /**
     * @brief 更新纹理内存统计
     * @param h 纹理句柄
     */
    void UpdateTextureMemory(Handle h) {
        size_t mem = GetTextureMemory(h);
        // Recalculate total memory
        size_t total = 0;
        ForEach([&total](Handle, const Texture* tex) {
            total += static_cast<size_t>(tex->GetWidth()) * tex->GetHeight() * 4;
        });
        // Note: ResourcePool doesn't expose a way to set memory usage directly
        // For now, this is informational
    }

    /**
     * @brief 按尺寸查找纹理 (用于缓存命中)
     * @param width 宽度
     * @param height 高度
     * @return 匹配的纹理句柄，未找到返回 InvalidHandle
     */
    Handle FindBySize(int width, int height) const {
        Handle result = InvalidHandle;
        ForEach([&](Handle h, const Texture* tex) {
            if (tex->GetWidth() == width && tex->GetHeight() == height) {
                result = h;
            }
        });
        return result;
    }
};

} // namespace SR

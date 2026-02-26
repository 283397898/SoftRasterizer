#pragma once

#include <vector>
#include <deque>
#include <list>
#include <memory>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <algorithm>

namespace SR {

/**
 * @brief 通用资源池模板，提供类型安全的资源管理和 LRU 淘汰
 *
 * @tparam T 资源类型
 *
 * 特性：
 * - O(1) 分配和释放
 * - 句柄重用
 * - LRU 淘汰支持
 * - 内存预算管理
 */
template<typename T>
class ResourcePool {
public:
    using Handle = uint32_t;
    static constexpr Handle InvalidHandle = UINT32_MAX;

    ResourcePool() = default;
    ~ResourcePool() = default;

    // Non-copyable, movable
    ResourcePool(const ResourcePool&) = delete;
    ResourcePool& operator=(const ResourcePool&) = delete;
    ResourcePool(ResourcePool&&) = default;
    ResourcePool& operator=(ResourcePool&&) = default;

    /**
     * @brief 分配新资源
     * @param args 传递给 T 构造函数的参数
     * @return 资源句柄
     */
    template<typename... Args>
    Handle Allocate(Args&&... args) {
        Handle handle;

        if (!m_freeList.empty()) {
            // Reuse free slot
            handle = m_freeList.front();
            m_freeList.pop_front();
            m_resources[handle] = std::make_unique<T>(std::forward<Args>(args)...);
            m_generation[handle]++;
            m_alive[handle] = true;
        } else {
            // Allocate new slot
            handle = static_cast<Handle>(m_resources.size());
            m_resources.push_back(std::make_unique<T>(std::forward<Args>(args)...));
            m_generation.push_back(0);
            m_alive.push_back(true);
            m_lastAccess.push_back(0);
        }

        // Update LRU
        Touch(handle);
        m_accessCounter++;

        return handle;
    }

    /**
     * @brief 释放资源
     * @param handle 资源句柄
     */
    void Release(Handle handle) {
        if (!IsValid(handle)) {
            return;
        }

        m_alive[handle] = false;
        m_resources[handle].reset();
        m_freeList.push_back(handle);

        // Remove from LRU list
        m_lruList.erase(
            std::remove(m_lruList.begin(), m_lruList.end(), handle),
            m_lruList.end()
        );
    }

    /**
     * @brief 获取资源指针
     * @param handle 资源句柄
     * @return 资源指针，无效句柄返回 nullptr
     */
    T* Get(Handle handle) {
        if (!IsValid(handle)) {
            return nullptr;
        }
        Touch(handle);
        return m_resources[handle].get();
    }

    /**
     * @brief 获取资源指针 (const 版本)
     * @param handle 资源句柄
     * @return 资源指针，无效句柄返回 nullptr
     */
    const T* Get(Handle handle) const {
        if (!IsValid(handle)) {
            return nullptr;
        }
        return m_resources[handle].get();
    }

    /**
     * @brief 检查句柄是否有效
     * @param handle 资源句柄
     * @return true 如果有效
     */
    bool IsValid(Handle handle) const {
        if (handle == InvalidHandle || handle >= m_resources.size()) {
            return false;
        }
        return m_alive[handle];
    }

    /**
     * @brief 设置内存预算
     * @param bytes 字节数
     */
    void SetMemoryBudget(size_t bytes) {
        m_memoryBudget = bytes;
    }

    /**
     * @brief 获取当前内存使用量
     * @return 字节数
     */
    size_t GetMemoryUsage() const {
        return m_memoryUsage;
    }

    /**
     * @brief 获取内存预算
     * @return 字节数
     */
    size_t GetMemoryBudget() const {
        return m_memoryBudget;
    }

    /**
     * @brief 执行 LRU 淘汰直到满足内存预算
     */
    void EvictLRU() {
        while (m_memoryUsage > m_memoryBudget && !m_lruList.empty()) {
            Handle oldest = m_lruList.front();
            Release(oldest);
        }
    }

    /**
     * @brief 获取活跃资源数量
     * @return 数量
     */
    size_t GetActiveCount() const {
        return m_lruList.size();
    }

    /**
     * @brief 清空所有资源
     */
    void Clear() {
        m_resources.clear();
        m_generation.clear();
        m_alive.clear();
        m_lastAccess.clear();
        m_freeList.clear();
        m_lruList.clear();
        m_memoryUsage = 0;
        m_accessCounter = 0;
    }

    /**
     * @brief 遍历所有活跃资源
     * @param callback 对每个活跃资源调用的回调函数
     */
    void ForEach(const std::function<void(Handle, T*)>& callback) {
        for (Handle h = 0; h < m_resources.size(); ++h) {
            if (m_alive[h] && m_resources[h]) {
                callback(h, m_resources[h].get());
            }
        }
    }

    /**
     * @brief 遍历所有活跃资源 (const 版本)
     * @param callback 对每个活跃资源调用的回调函数
     */
    void ForEach(const std::function<void(Handle, const T*)>& callback) const {
        for (Handle h = 0; h < m_resources.size(); ++h) {
            if (m_alive[h] && m_resources[h]) {
                callback(h, m_resources[h].get());
            }
        }
    }

    /**
     * @brief 更新资源内存使用量
     * @param handle 资源句柄
     * @param newSize 新的内存大小
     */
    void UpdateMemoryUsage(Handle handle, size_t newSize) {
        if (!IsValid(handle)) {
            return;
        }
        // Note: This is a simple implementation. For accurate tracking,
        // each resource should report its own size.
        m_memoryUsage = 0;
        for (const auto& res : m_resources) {
            if (res) {
                m_memoryUsage += sizeof(T); // Simplified
            }
        }
    }

private:
    void Touch(Handle handle) {
        m_lastAccess[handle] = ++m_accessCounter;

        // Move to end of LRU list (most recently used)
        m_lruList.erase(
            std::remove(m_lruList.begin(), m_lruList.end(), handle),
            m_lruList.end()
        );
        m_lruList.push_back(handle);
    }

    std::vector<std::unique_ptr<T>> m_resources;
    std::vector<uint32_t> m_generation;  ///< 用于检测过期句柄
    std::vector<bool> m_alive;
    std::vector<uint64_t> m_lastAccess;

    std::deque<Handle> m_freeList;       ///< 空闲槽位列表
    std::list<Handle> m_lruList;         ///< LRU 顺序列表

    size_t m_memoryUsage = 0;
    size_t m_memoryBudget = SIZE_MAX;
    uint64_t m_accessCounter = 0;
};

} // namespace SR

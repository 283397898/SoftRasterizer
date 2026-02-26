#pragma once

#include <deque>
#include <list>
#include <memory>
#include <cstddef>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

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
    struct Handle {
        uint32_t packed = 0xFFFFFFFFu;

        constexpr Handle() = default;
        constexpr explicit Handle(uint32_t value) : packed(value) {}

        constexpr uint16_t Index() const { return static_cast<uint16_t>(packed & 0xFFFFu); }
        constexpr uint16_t Generation() const { return static_cast<uint16_t>((packed >> 16) & 0xFFFFu); }
        constexpr bool IsInvalid() const { return packed == 0xFFFFFFFFu; }

        static constexpr Handle Pack(uint16_t index, uint16_t generation) {
            return Handle((static_cast<uint32_t>(generation) << 16) | static_cast<uint32_t>(index));
        }

        constexpr bool operator==(const Handle&) const = default;
    };

    static constexpr Handle InvalidHandle = Handle{};

    ResourcePool() = default;
    ~ResourcePool() = default;

    // 禁止拷贝，允许移动
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
        uint16_t index = 0;

        if (!m_freeList.empty()) {
            index = m_freeList.front();
            m_freeList.pop_front();
            m_resources[index] = std::make_unique<T>(std::forward<Args>(args)...);
            m_alive[index] = 1;
        } else {
            if (m_resources.size() >= static_cast<size_t>(UINT16_MAX) + 1) {
                return InvalidHandle;
            }
            index = static_cast<uint16_t>(m_resources.size());
            m_resources.push_back(std::make_unique<T>(std::forward<Args>(args)...));
            m_generation.push_back(0);
            m_alive.push_back(1);
        }

        Touch(index);
        return Handle::Pack(index, m_generation[index]);
    }

    /**
     * @brief 释放资源
     * @param handle 资源句柄
     */
    void Release(Handle handle) {
        if (!IsValid(handle)) {
            return;
        }

        uint16_t index = handle.Index();
        m_alive[index] = 0;
        m_resources[index].reset();
        m_generation[index] = static_cast<uint16_t>(m_generation[index] + 1);
        m_freeList.push_back(index);
        RemoveFromLRU(index);
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
        uint16_t index = handle.Index();
        Touch(index);
        return m_resources[index].get();
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
        return m_resources[handle.Index()].get();
    }

    /**
     * @brief 检查句柄是否有效
     * @param handle 资源句柄
     * @return true 如果有效
     */
    bool IsValid(Handle handle) const {
        if (handle.IsInvalid()) {
            return false;
        }
        uint16_t index = handle.Index();
        if (index >= m_resources.size()) {
            return false;
        }
        return m_alive[index] != 0 && m_generation[index] == handle.Generation();
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
            uint16_t oldest = m_lruList.front();
            Release(Handle::Pack(oldest, m_generation[oldest]));
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
        m_freeList.clear();
        m_lruList.clear();
        m_lruMap.clear();
        m_memoryUsage = 0;
    }

    /**
     * @brief 遍历所有活跃资源
     * @param callback 对每个活跃资源调用的回调函数
     */
    void ForEach(const std::function<void(Handle, T*)>& callback) {
        for (size_t i = 0; i < m_resources.size(); ++i) {
            if (m_alive[i] != 0 && m_resources[i]) {
                callback(Handle::Pack(static_cast<uint16_t>(i), m_generation[i]), m_resources[i].get());
            }
        }
    }

    /**
     * @brief 遍历所有活跃资源 (const 版本)
     * @param callback 对每个活跃资源调用的回调函数
     */
    void ForEach(const std::function<void(Handle, const T*)>& callback) const {
        for (size_t i = 0; i < m_resources.size(); ++i) {
            if (m_alive[i] != 0 && m_resources[i]) {
                callback(Handle::Pack(static_cast<uint16_t>(i), m_generation[i]), m_resources[i].get());
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
        // 简单实现：重新遍历计算，精确追踪需各资源自报大小
        m_memoryUsage = 0;
        for (const auto& res : m_resources) {
            if (res) {
                m_memoryUsage += sizeof(T); // 简化估算
            }
        }
    }

private:
    void RemoveFromLRU(uint16_t index) {
        auto it = m_lruMap.find(index);
        if (it == m_lruMap.end()) {
            return;
        }
        m_lruList.erase(it->second);
        m_lruMap.erase(it);
    }

    void Touch(uint16_t index) {
        auto it = m_lruMap.find(index);
        if (it != m_lruMap.end()) {
            m_lruList.splice(m_lruList.end(), m_lruList, it->second);
            return;
        }
        m_lruList.push_back(index);
        auto tail = m_lruList.end();
        --tail;
        m_lruMap[index] = tail;
    }

    std::vector<std::unique_ptr<T>> m_resources;
    std::vector<uint16_t> m_generation;  ///< 用于检测过期句柄
    std::vector<uint8_t> m_alive;

    std::deque<uint16_t> m_freeList;     ///< 空闲槽位列表
    std::list<uint16_t> m_lruList;       ///< LRU 顺序列表
    std::unordered_map<uint16_t, typename std::list<uint16_t>::iterator> m_lruMap;

    size_t m_memoryUsage = 0;
    size_t m_memoryBudget = SIZE_MAX;
};

} // namespace SR

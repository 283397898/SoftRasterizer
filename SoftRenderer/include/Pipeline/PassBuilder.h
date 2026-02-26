#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include "Pipeline/RenderPass.h"

namespace SR {

/**
 * @brief Pass 构建器
 *
 * 使用构建器模式创建可配置的渲染管线。
 * 支持依赖关系和条件执行。
 */
class PassBuilder {
public:
    PassBuilder() = default;

    /**
     * @brief 添加 Pass 到管线
     * @param pass Pass 实例
     * @return 构建器引用（支持链式调用）
     */
    PassBuilder& AddPass(std::unique_ptr<RenderPass> pass);

    /**
     * @brief 添加 Pass 依赖关系
     * @param from Pass 名称（依赖者）
     * @param to Pass 名称（被依赖者）
     * @return 构建器引用
     */
    PassBuilder& AddDependency(const std::string& from, const std::string& to);

    /**
     * @brief 设置 Pass 执行条件
     * @param passName Pass 名称
     * @param condition 条件函数
     * @return 构建器引用
     */
    using PassCondition = std::function<bool(const RenderContext&)>;
    PassBuilder& SetCondition(const std::string& passName, PassCondition condition);

    /**
     * @brief 构建并返回排序后的 Pass 列表
     * @return 按依赖关系排序的 Pass 列表
     */
    std::vector<std::unique_ptr<RenderPass>> Build();

    /**
     * @brief 验证管线配置
     * @return true 如果配置有效
     */
    bool Validate() const;

    /**
     * @brief 获取错误信息
     * @return 错误信息字符串
     */
    const std::string& GetError() const { return m_error; }

    /**
     * @brief 清空构建器状态
     */
    void Clear();

private:
    /**
     * @brief 拓扑排序 Pass
     * @return 排序后的 Pass 名称列表
     */
    std::vector<std::string> TopologicalSort() const;

    /**
     * @brief 检测循环依赖
     * @return true 如果存在循环依赖
     */
    bool HasCircularDependency() const;

    struct PassNode {
        std::unique_ptr<RenderPass> pass;
        std::unordered_set<std::string> dependencies;
        PassCondition condition;
    };

    std::unordered_map<std::string, PassNode> m_passes;
    mutable std::string m_error;  ///< 错误信息（mutable 允许在 const 方法中修改）
};

/**
 * @brief 默认管线配置
 *
 * 创建标准渲染管线：Opaque → Skybox → Transparent → PostProcess
 */
class DefaultPipeline {
public:
    /**
     * @brief 创建默认渲染管线
     * @return 配置好的 Pass 列表
     */
    static std::vector<std::unique_ptr<RenderPass>> Create();

    /**
     * @brief 使用 PassBuilder 创建默认管线
     * @param builder PassBuilder 实例
     * @return 构建器引用
     */
    static PassBuilder& Configure(PassBuilder& builder);
};

} // namespace SR

#include "Pipeline/PassBuilder.h"
#include "Pipeline/OpaquePass.h"
#include <queue>
#include <algorithm>

namespace SR {

PassBuilder& PassBuilder::AddPass(std::unique_ptr<RenderPass> pass) {
    if (!pass) {
        m_error = "Cannot add null pass";
        return *this;
    }

    std::string name = pass->GetName();
    if (m_passes.find(name) != m_passes.end()) {
        m_error = "Pass with name '" + name + "' already exists";
        return *this;
    }

    PassNode node;
    node.pass = std::move(pass);
    m_passes[name] = std::move(node);

    return *this;
}

PassBuilder& PassBuilder::AddDependency(const std::string& from, const std::string& to) {
    if (m_passes.find(from) == m_passes.end()) {
        m_error = "Pass '" + from + "' does not exist";
        return *this;
    }

    if (m_passes.find(to) == m_passes.end()) {
        m_error = "Pass '" + to + "' does not exist";
        return *this;
    }

    m_passes[from].dependencies.insert(to);
    return *this;
}

PassBuilder& PassBuilder::SetCondition(const std::string& passName, PassCondition condition) {
    auto it = m_passes.find(passName);
    if (it == m_passes.end()) {
        m_error = "Pass '" + passName + "' does not exist";
        return *this;
    }

    it->second.condition = std::move(condition);
    return *this;
}

/**
 * @brief 对所有 Pass 进行拓扑排序（Kahn 算法）
 *
 * 使用 BFS + 入度表实现，保证被依赖的 Pass 先于依赖者执行。
 * 若存在循环依赖，返回的结果数组长度将小于 m_passes.size()，
 * 可通过 HasCircularDependency() 检测。
 *
 * @return 按执行顺序排列的 Pass 名称列表（空表示循环依赖）
 */
std::vector<std::string> PassBuilder::TopologicalSort() const {
    // 初始化所有 Pass 的入度为 0
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> dependents;

    for (const auto& [name, node] : m_passes) {
        inDegree[name] = 0;
    }

    // 构建依赖图：对于 "A 依赖 B"，B 的 dependents 列表加入 A，A 的入度加 1
    for (const auto& [name, node] : m_passes) {
        for (const auto& dep : node.dependencies) {
            dependents[dep].push_back(name);
            inDegree[name]++;
        }
    }

    // Kahn 算法：从所有入度为 0 的节点开始 BFS
    std::queue<std::string> queue;
    for (const auto& [name, degree] : inDegree) {
        if (degree == 0) {
            queue.push(name);
        }
    }

    std::vector<std::string> result;
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        result.push_back(current);

        // 当前 Pass 完成后，其所有依赖者入度减 1；变为 0 时加入队列
        for (const auto& dependent : dependents[current]) {
            inDegree[dependent]--;
            if (inDegree[dependent] == 0) {
                queue.push(dependent);
            }
        }
    }

    return result;
}

// 若拓扑排序结果数量不等于 Pass 总数，说明存在循环依赖
bool PassBuilder::HasCircularDependency() const {
    return TopologicalSort().size() != m_passes.size();
}

bool PassBuilder::Validate() const {
    if (m_passes.empty()) {
        m_error = "No passes added to pipeline";
        return false;
    }

    if (HasCircularDependency()) {
        m_error = "Circular dependency detected in pass pipeline";
        return false;
    }

    m_error.clear();
    return true;
}

std::vector<std::unique_ptr<RenderPass>> PassBuilder::Build() {
    std::vector<std::unique_ptr<RenderPass>> result;

    if (!Validate()) {
        return result;
    }

    std::vector<std::string> sortedNames = TopologicalSort();
    result.reserve(sortedNames.size());

    for (const auto& name : sortedNames) {
        auto it = m_passes.find(name);
        if (it != m_passes.end() && it->second.pass) {
            result.push_back(std::move(it->second.pass));
        }
    }

    // Build 完成后清空构建器状态，防止重复使用
    m_passes.clear();
    m_error.clear();

    return result;
}

void PassBuilder::Clear() {
    m_passes.clear();
    m_error.clear();
}

// ============================================================================
// DefaultPipeline — 标准渲染管线配置
// 执行顺序：OpaquePass → SkyboxPass → TransparentPass → PostProcessPass
// ============================================================================

std::vector<std::unique_ptr<RenderPass>> DefaultPipeline::Create() {
    PassBuilder builder;
    Configure(builder);
    return builder.Build();
}

PassBuilder& DefaultPipeline::Configure(PassBuilder& builder) {
    // 注册四个标准渲染阶段
    builder.AddPass(std::make_unique<OpaquePass>());       // 不透明/Mask 几何体
    builder.AddPass(std::make_unique<SkyboxPass>());       // 天空盒（填充深度为远平面的像素）
    builder.AddPass(std::make_unique<TransparentPass>());  // 半透明几何体（从远到近排序）
    builder.AddPass(std::make_unique<PostProcessPass>());  // 后处理（FXAA + 色调映射）

    // 天空盒必须在不透明几何体之后（避免覆盖已有像素）
    builder.AddDependency("SkyboxPass", "OpaquePass");
    // 透明物体必须在不透明和天空盒之后（正确的混合需要完整的背景色）
    builder.AddDependency("TransparentPass", "OpaquePass");
    builder.AddDependency("TransparentPass", "SkyboxPass");
    // 后处理必须在所有渲染 Pass 完成后执行
    builder.AddDependency("PostProcessPass", "OpaquePass");
    builder.AddDependency("PostProcessPass", "SkyboxPass");
    builder.AddDependency("PostProcessPass", "TransparentPass");

    return builder;
}

} // namespace SR

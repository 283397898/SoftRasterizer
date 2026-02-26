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

std::vector<std::string> PassBuilder::TopologicalSort() const {
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> dependents;

    // Initialize in-degree for all passes
    for (const auto& [name, node] : m_passes) {
        inDegree[name] = 0;
    }

    // Build dependency graph and compute in-degrees
    for (const auto& [name, node] : m_passes) {
        for (const auto& dep : node.dependencies) {
            dependents[dep].push_back(name);
            inDegree[name]++;
        }
    }

    // Kahn's algorithm with priority queue for stable ordering
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

        for (const auto& dependent : dependents[current]) {
            inDegree[dependent]--;
            if (inDegree[dependent] == 0) {
                queue.push(dependent);
            }
        }
    }

    return result;
}

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

    // Clear the builder state after build
    m_passes.clear();
    m_error.clear();

    return result;
}

void PassBuilder::Clear() {
    m_passes.clear();
    m_error.clear();
}

// DefaultPipeline implementation
std::vector<std::unique_ptr<RenderPass>> DefaultPipeline::Create() {
    PassBuilder builder;
    Configure(builder);
    return builder.Build();
}

PassBuilder& DefaultPipeline::Configure(PassBuilder& builder) {
    // Add passes in default order
    builder.AddPass(std::make_unique<OpaquePass>());
    builder.AddPass(std::make_unique<SkyboxPass>());
    builder.AddPass(std::make_unique<TransparentPass>());
    builder.AddPass(std::make_unique<PostProcessPass>());

    // Set up dependencies: Transparent depends on Opaque and Skybox
    builder.AddDependency("TransparentPass", "OpaquePass");
    builder.AddDependency("TransparentPass", "SkyboxPass");

    // PostProcess depends on all rendering passes
    builder.AddDependency("PostProcessPass", "OpaquePass");
    builder.AddDependency("PostProcessPass", "SkyboxPass");
    builder.AddDependency("PostProcessPass", "TransparentPass");

    return builder;
}

} // namespace SR

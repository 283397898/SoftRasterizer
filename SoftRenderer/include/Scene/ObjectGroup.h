#pragma once

#include <vector>

#include "SoftRendererExport.h"
#include "Scene/Model.h"

namespace SR {

/**
 * @brief 场景对象组，管理场景中所有的静态或动态模型
 */
class SR_API ObjectGroup {
public:
    /** @brief 清空对象组中的所有模型 */
    void Clear();
    /** @brief 向对象组中添加一个模型 */
    void AddModel(const Model& model);

    /** @brief 获取对象组中所有的模型列表 */
    const std::vector<Model>& GetModels() const;

private:
    std::vector<Model> m_models; ///< 模型容器
};

} // namespace SR

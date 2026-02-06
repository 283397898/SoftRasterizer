#include "Scene/ObjectGroup.h"

namespace SR {

/**
 * @brief 清除对象组
 */
void ObjectGroup::Clear() {
	m_models.clear();
}

/**
 * @brief 添加一个新的模型到渲染组
 */
void ObjectGroup::AddModel(const Model& model) {
	m_models.push_back(model);
}

/**
 * @brief 返回组内所有模型
 */
const std::vector<Model>& ObjectGroup::GetModels() const {
	return m_models;
}

} // namespace SR

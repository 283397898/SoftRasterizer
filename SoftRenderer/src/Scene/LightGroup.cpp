#include "Scene/LightGroup.h"

namespace SR {

/**
 * @brief 清空当前光源组中的所有灯光
 */
void LightGroup::Clear() {
	m_directionalLights.clear();
}

/**
 * @brief 向场景中追加一盏新的平行光
 */
void LightGroup::AddDirectionalLight(const DirectionalLight& light) {
	m_directionalLights.push_back(light);
}

/**
 * @brief 返回场景中所有的平行光集合
 */
const std::vector<DirectionalLight>& LightGroup::GetDirectionalLights() const {
	return m_directionalLights;
}

} // namespace SR

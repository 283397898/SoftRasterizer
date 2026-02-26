#pragma once

#include "Runtime/ResourcePool.h"
#include "Material/PBRMaterial.h"
#include "Pipeline/MaterialTable.h"

namespace SR {

/**
 * @brief 材质资源池 (AOS 布局)
 *
 * 继承自 ResourcePool<PBRMaterial>，提供材质特定的工厂方法。
 * 使用 Array of Structures (AOS) 布局存储完整的 PBRMaterial 对象。
 *
 * ## 与 MaterialTable 的关系
 * - **MaterialPool**: 场景级别的材质对象管理，存储完整的 PBRMaterial 结构
 * - **MaterialTable**: 渲染时的高性能 SOA 存储，仅包含着色所需的纹理索引和属性
 *
 * ## 数据流
 * 1. 加载场景时，材质数据存储在 MaterialPool 中的 PBRMaterial 对象
 * 2. 渲染时，GeometryProcessor 从 PBRMaterial 提取属性并添加到 MaterialTable
 * 3. FragmentShader 通过 MaterialHandle 从 MaterialTable 获取着色数据
 *
 * ## 使用场景
 * - 需要独立管理材质对象时使用此类
 * - 高性能渲染路径应使用 MaterialTable
 */
class MaterialPool : public ResourcePool<PBRMaterial> {
public:
    using Handle = ResourcePool<PBRMaterial>::Handle;

    /**
     * @brief 创建默认材质
     * @return 材质句柄
     */
    Handle CreateDefaultMaterial() {
        return Allocate();
    }

    /**
     * @brief 创建金属材质
     * @param albedo 基础颜色
     * @param roughness 粗糙度
     * @return 材质句柄
     */
    Handle CreateMetallicMaterial(const Vec3& albedo, double roughness = 0.5) {
        Handle h = Allocate();
        PBRMaterial* mat = Get(h);
        if (mat) {
            mat->albedo = albedo;
            mat->metallic = 1.0;
            mat->roughness = roughness;
        }
        return h;
    }

    /**
     * @brief 创建电介质材质
     * @param albedo 基础颜色
     * @param roughness 粗糙度
     * @param ior 折射率
     * @return 材质句柄
     */
    Handle CreateDielectricMaterial(const Vec3& albedo, double roughness = 0.5, double ior = 1.5) {
        Handle h = Allocate();
        PBRMaterial* mat = Get(h);
        if (mat) {
            mat->albedo = albedo;
            mat->metallic = 0.0;
            mat->roughness = roughness;
            mat->ior = ior;
        }
        return h;
    }

    /**
     * @brief 创建透明材质
     * @param albedo 基础颜色
     * @param alpha 透明度
     * @return 材质句柄
     */
    Handle CreateTransparentMaterial(const Vec3& albedo, double alpha = 0.5) {
        Handle h = Allocate();
        PBRMaterial* mat = Get(h);
        if (mat) {
            mat->albedo = albedo;
            mat->alpha = alpha;
            mat->alphaMode = GLTFAlphaMode::Blend;
        }
        return h;
    }

    /**
     * @brief 创建自发光材质
     * @param emissiveFactor 自发光颜色
     * @return 材质句柄
     */
    Handle CreateEmissiveMaterial(const Vec3& emissiveFactor) {
        Handle h = Allocate();
        PBRMaterial* mat = Get(h);
        if (mat) {
            mat->emissiveFactor = emissiveFactor;
        }
        return h;
    }

    /**
     * @brief 按金属度查找材质
     * @param metallic 金属度
     * @return 匹配的材质句柄，未找到返回 InvalidHandle
     */
    Handle FindByMetallic(double metallic) const {
        Handle result = InvalidHandle;
        ForEach([&](Handle h, const PBRMaterial* mat) {
            if (std::abs(mat->metallic - metallic) < 0.01) {
                result = h;
            }
        });
        return result;
    }
};

} // namespace SR

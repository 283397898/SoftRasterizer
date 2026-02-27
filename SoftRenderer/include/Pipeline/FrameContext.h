#pragma once

#include <vector>

#include "Math/Mat4.h"
#include "Math/Vec3.h"
#include "Scene/LightGroup.h"

namespace SR {

enum class OpenMPSchedulePolicy {
    Static,
    Dynamic,
    Guided
};

struct OpenMPTuningOptions {
#if defined(SR_INTEL_OMP)
    OpenMPSchedulePolicy clipSchedule = OpenMPSchedulePolicy::Guided;          ///< 裁剪阶段并行调度策略
    OpenMPSchedulePolicy binCountSchedule = OpenMPSchedulePolicy::Guided;      ///< Bin 计数阶段并行调度策略
    OpenMPSchedulePolicy clearSchedule = OpenMPSchedulePolicy::Guided;         ///< Clear 阶段并行调度策略
    OpenMPSchedulePolicy postProcessSchedule = OpenMPSchedulePolicy::Guided;   ///< 后处理阶段并行调度策略
#else
    OpenMPSchedulePolicy clipSchedule = OpenMPSchedulePolicy::Dynamic;         ///< 裁剪阶段并行调度策略
    OpenMPSchedulePolicy binCountSchedule = OpenMPSchedulePolicy::Dynamic;     ///< Bin 计数阶段并行调度策略
    OpenMPSchedulePolicy clearSchedule = OpenMPSchedulePolicy::Dynamic;        ///< Clear 阶段并行调度策略
    OpenMPSchedulePolicy postProcessSchedule = OpenMPSchedulePolicy::Dynamic;  ///< 后处理阶段并行调度策略
#endif
    int clipChunk = 64;                                                         ///< 裁剪阶段并行 chunk 大小
    int binCountChunk = 64;                                                     ///< Bin 计数阶段并行 chunk 大小
    int clearChunk = 4096;                                                      ///< Clear 阶段并行 chunk 大小
    int postProcessChunk = 1;                                                   ///< 后处理阶段并行 chunk 大小
    OpenMPSchedulePolicy rasterTileSchedule = OpenMPSchedulePolicy::Dynamic;   ///< Tile 光栅并行调度策略
    int rasterTileChunk = 1;                                                   ///< Tile 光栅并行 chunk 大小
    OpenMPSchedulePolicy drawItemBuildSchedule = OpenMPSchedulePolicy::Dynamic; ///< DrawItem 构建并行调度策略
    int drawItemBuildChunk = 1;                                                ///< DrawItem 构建并行 chunk 大小
    bool enableProfiling = false;                                              ///< 是否输出 OpenMP 分阶段调试统计
    bool enableLegacyBinReduction = false;                                     ///< 是否启用旧版 critical 归约路径（回退开关）
};

struct GLTFImage;
struct GLTFSampler;
class EnvironmentMap;
class MaterialTable;

/**
 * @brief 每帧全局渲染上下文
 *
 * 包含当前帧渲染所需的所有全局数据：变换矩阵、光照、相机参数和资源指针。
 * 由 FrameContextBuilder 构建，在渲染开始前传递给管线各阶段。
 */
struct FrameContext {
    Mat4 view       = Mat4::Identity();      ///< 观察矩阵（世界空间 → 相机空间）
    Mat4 projection = Mat4::Identity();      ///< 投影矩阵（相机空间 → 裁剪空间）
    Vec3 cameraPos{0.0, 0.0, 0.0};          ///< 相机在世界空间中的位置
    Vec3 ambientColor{0.03, 0.03, 0.03};    ///< 全局环境光颜色（无 IBL 时使用）
    std::vector<DirectionalLight> lights;    ///< 场景平行光列表
    const std::vector<GLTFImage>*   images   = nullptr; ///< 场景图像数组（纹理采样用）
    const std::vector<GLTFSampler>* samplers = nullptr; ///< 场景采样器数组（纹理过滤用）
    const EnvironmentMap* environmentMap     = nullptr; ///< IBL 环境贴图（可选，nullptr 时退回常量环境光）
    const MaterialTable*  materialTable      = nullptr; ///< 帧级材质表 (SOA 布局，由 GeometryProcessor 填充)
    OpenMPTuningOptions openmp{};                        ///< OpenMP 调优选项（调度策略、chunk、统计开关）
};

} // namespace SR

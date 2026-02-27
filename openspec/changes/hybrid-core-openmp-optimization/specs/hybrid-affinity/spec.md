## ADDED Requirements

### Requirement: Intel OpenMP 运行时亲和性 SHALL 在渲染器初始化时自动配置
当编译器为 Intel ICX（`SR_INTEL_OMP` 宏已定义）时，系统 SHALL 在首个 OpenMP 并行区执行前设置 KMP 亲和性参数，确保线程均匀分散到所有物理核心（包括 P 核和 E 核）。

#### Scenario: ICX 编译下自动设置 KMP 亲和性
- **WHEN** 使用 ICX 编译器构建且 `SR_INTEL_OMP` 宏已定义
- **AND** 渲染器初始化函数被调用
- **THEN** 系统 SHALL 设置 `KMP_AFFINITY=granularity=core,balanced`
- **AND** 系统 SHALL 设置 `KMP_HW_SUBSET=1t`（每核一线程）
- **AND** 设置 SHALL 发生在首个 `#pragma omp parallel` 执行之前

#### Scenario: 非 ICX 编译器下跳过 KMP 设置
- **WHEN** 使用非 Intel 编译器构建（`SR_INTEL_OMP` 未定义）
- **THEN** 系统 SHALL 跳过所有 KMP 专有环境变量设置
- **AND** OpenMP 行为 SHALL 由操作系统和编译器默认策略控制
- **AND** 编译 SHALL 无错误或警告完成

### Requirement: `SR_INTEL_OMP` 宏 SHALL 由 CMake 自动检测并定义
构建系统 SHALL 在检测到 Intel ICX 编译器时自动为 SoftRenderer 目标定义 `SR_INTEL_OMP` 编译宏。

#### Scenario: ICX 编译器自动定义宏
- **WHEN** `CMAKE_CXX_COMPILER_ID` 为 `"IntelLLVM"`
- **THEN** CMake SHALL 为 SoftRenderer 目标添加 `SR_INTEL_OMP` 编译定义
- **AND** 该宏 SHALL 在所有 SoftRenderer 源文件中可用

#### Scenario: 非 ICX 编译器不定义宏
- **WHEN** `CMAKE_CXX_COMPILER_ID` 不为 `"IntelLLVM"`
- **THEN** CMake SHALL 不定义 `SR_INTEL_OMP`
- **AND** 所有 `#if defined(SR_INTEL_OMP)` 保护的代码块 SHALL 被跳过

### Requirement: 默认调度策略 SHALL 根据编译器条件选择
系统 SHALL 根据编译器类型为新增的调度配置字段选择不同的默认值，ICX 下优先 `Guided`，其他编译器优先 `Dynamic`。

#### Scenario: ICX 编译器下默认 Guided
- **WHEN** `SR_INTEL_OMP` 已定义
- **THEN** `OpenMPTuningOptions` 中 clip、bin、clear、postProcess 阶段的默认调度策略 SHALL 为 `Guided`

#### Scenario: 非 ICX 编译器下默认 Dynamic
- **WHEN** `SR_INTEL_OMP` 未定义
- **THEN** `OpenMPTuningOptions` 中 clip、bin、clear、postProcess 阶段的默认调度策略 SHALL 为 `Dynamic`

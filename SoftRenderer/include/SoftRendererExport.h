#pragma once

/// @brief DLL 导入/导出宏
/// 编译 SoftRenderer DLL 时定义 SOFTRENDERER_EXPORTS，外部使用者自动导入
#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef SOFTRENDERER_EXPORTS
        #define SR_API __declspec(dllexport)
    #else
        #define SR_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define SR_API __attribute__((visibility("default")))
#else
    #define SR_API
#endif

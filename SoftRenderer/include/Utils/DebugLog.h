#pragma once

/**
 * @file DebugLog.h
 * @brief 条件编译的调试日志宏（跨平台）。
 *
 * Debug 构建输出到标准错误，Release 构建为空操作。
 */

#if !defined(NDEBUG)
#include <cstdio>
/** @brief 在 Debug 构建下向 stderr 输出字符串，Release 下为空操作 */
#define SR_DEBUG_LOG(msg) do { std::fputs((msg), stderr); } while (0)
#else
#define SR_DEBUG_LOG(msg) ((void)0)
#endif

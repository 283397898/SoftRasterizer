#pragma once

/**
 * @file DebugLog.h
 * @brief 条件编译的调试日志宏（跨平台）。
 *
 * SR_DEBUG_LOG  — Debug 构建输出到 stderr，Release 构建为空操作。
 * SR_PERF_LOG   — Release/Debug 均输出（用于性能诊断埋点），
 *                 Windows 上走 OutputDebugStringA（VS 调试输出 / DebugView 可见），
 *                 其他平台走 stderr。
 */

#if !defined(NDEBUG)
#include <cstdio>
/** @brief 在 Debug 构建下向 stderr 输出字符串，Release 下为空操作 */
#define SR_DEBUG_LOG(msg) do { std::fputs((msg), stderr); } while (0)
#else
#define SR_DEBUG_LOG(msg) ((void)0)
#endif

// --- Release 可用的性能诊断日志 ---
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
/** @brief Release/Debug 均输出到 VS 调试输出窗口（OutputDebugStringA） */
#define SR_PERF_LOG(msg) do { OutputDebugStringA(msg); } while (0)
#else
#include <cstdio>
#define SR_PERF_LOG(msg) do { std::fputs((msg), stderr); } while (0)
#endif

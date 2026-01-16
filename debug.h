#pragma once
#include <stdio.h>

/*
 * 文件用途：调试控制台与日志宏
 *
 * 说明：
 * - 在 `_DEBUG` 构建下：
 *   - `INIT_CONSOLE`/`RELEASE_CONSOLE` 映射到实际实现，分配/释放调试控制台；
 *   - `LOG` 输出到标准输出（printf）。
 * - 在非调试构建下：宏为空实现，避免引入控制台与输出。
 *
 * 注意：
 * - Windows 专用：`AllocConsole/FreeConsole` 依赖系统 API；跨平台需替换实现。
 */
#ifdef _DEBUG
#define INIT_CONSOLE	init_console
#define RELEASE_CONSOLE	release_console
#define LOG				printf
//#define LOG(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define INIT_CONSOLE()
#define RELEASE_CONSOLE()
#define LOG(...)
#endif // _DEBUG

void init_console();
void release_console();

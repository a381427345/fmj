#include "framework.h"
#include "debug.h"

/*
 * 调试控制台实现：仅在 Windows 下可用
 * - init_console：分配控制台并重定向 stdout 到控制台窗口
 * - release_console：释放控制台
 */
void init_console()
{
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	printf("add console success!\n");
}

void release_console()
{
	FreeConsole();
}
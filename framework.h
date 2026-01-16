// header.h: 标准系统/项目公共头
// 作用：集中包含 Windows 与 CRT 头文件，减少每个源文件的样板代码。
// 说明：定义 WIN32_LEAN_AND_MEAN 以精简 Win32 头体积；
//       同时启用 _CRT_FUNCTIONS_REQUIRED 并包含 math.h 以支持窗口缩放相关计算。

#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <windows.h>
#include <shellapi.h>
// C 运行时头文件
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#define _CRT_FUNCTIONS_REQUIRED 1
#include <math.h>

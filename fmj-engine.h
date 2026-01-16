#pragma once

#include "resource.h"

/*
 * FIXED_WINDOW：窗口缩放时的“固定边/角”策略
 * - 由 WM_SIZING 的方向决定：
 *   - TOP/LEFT/RIGHT/BOTTOM 表示固定对应边，使窗口围绕该边调整
 *   - TOPLEFT/TOPRIGHT/BOTTOMLEFT/BOTTOMRIGHT 表示固定对应角
 *   - LEFTCENTER/RIGHTCENTER/TOPCENTER/BOTTOMCENTER 表示水平/垂直方向居中对齐
 * 该枚举用于 CalcWindowSize()，结合 screen_mult 把客户区尺寸锁定为逻辑分辨率的整数倍。
 */
typedef enum _FIXED_WINDOW {
	FIEXD_NONE = 0,
	FIEXD_TOPLEFT,
	FIEXD_TOPRIGHT,
	FIEXD_BOTTOMLEFT,
	FIEXD_BOTTOMRIGHT,
	FIEXD_LEFTCENTER,
	FIEXD_RIGHTCENTER,
	FIEXD_TOPCENTER,
	FIEXD_BOTTOMCENTER,
}FIXED_WINDOW;
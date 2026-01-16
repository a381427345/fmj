#pragma once
#include "framework.h"

/*
 * 文件用途：系统消息类型与随机数环境定义
 *
 * 说明：
 * - 消息系统：`MsgType` 表示一条消息，`type` 为消息类型，`param` 为消息参数；配合 GUI 输入与键盘扫描使用（见 middle.h / GuiTranslateMsg）。
 * - 消息类型常量：`DICT_WM_*` 标识不同来源的消息（键盘、字符、定时器、串口、功耗、电源、命令等）。
 * - 命令常量：`CMD_*` 为 `DICT_WM_COMMAND` 的子命令，用于打开/关闭输入法、返回首页等系统级操作。
 * - 随机数环境：`RandEnvType` 保存随机状态与上限，供 `SysRand/SysSrand` 使用。
 */

typedef struct tagMsgType
{
	UINT8 type;
	UINT16 param;
}MsgType, * PtrMsg;

/* 系统标志：定时器掩码（用于筛选定时触发） */
#define		SYS_FLAG_TIMER_MASK				0x01

/* 消息类型：无效消息/键盘扫描 */
#define		DICT_WM_DUMMY		0x00
#define		DICT_WM_KEY			0x01

/* 字符消息：ASCII/汉字/数学/功能（功能键） */
#define		DICT_WM_CHAR_ASC	0x02
#define		DICT_WM_CHAR_HZ		0x03
#define		DICT_WM_CHAR_MATH	0x04
#define		DICT_WM_CHAR_FUN	0x05

/* 定时器 / 串口（通信）消息 */
#define		DICT_WM_TIMER		0x06
#define		DICT_WM_COM			0x07

/* 电源消息（功耗/供电状态） */
#define		DICT_WM_POWER		0x08

/* 提示/命令消息 */
#define		DICT_WM_ALERT		0x09
#define		DICT_WM_COMMAND		0x0A

/*-----------------------------------------------------------------------------------------
*            WM_COMMAND 对应的子命令取值
*-----------------------------------------------------------------------------------------*/
#define		CMD_CHN_INPUT_OPEN			1			/* 打开中文输入法 */
#define		CMD_CHN_INPUT_CLOSE			2			/* 关闭中文输入法 */
#define		CMD_RETURN_HOME				3			/* 返回首页（主界面） */
#define		CMD_SCR_NESTED				4			/* 屏幕嵌套（返回上一层） */

typedef struct tagRandEnv
{
	UINT32 next;
	UINT16 randMax;
} RandEnvType, * PtrRandEnv;

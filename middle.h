#pragma once
#include "keytable.h"
#include "dictsys.h"
#include "debug.h"

/*
 * 文件用途：引擎中间层（图形/输入/内存/文件/数据Bank）
 *
 * 说明：
 * - 图形：提供基础的线、矩形、图片绘制、屏幕局部保存/恢复、反色等。
 * - 输入与GUI：键盘扫描与消息转换、消息框、按键音设置与取/恢复键盘状态。
 * - 内存：自定义 MCB（Memory Control Block）堆管理，初始化/分配/释放，最小4字节对齐。
 * - 文件与Flash：文件创建/打开/删除/读写/定位，以及基于 Windows HANDLE 的文件句柄管理，最多 FILE_NUM 个文件。
 * - 数据Bank：逻辑Bank 与物理Bank 的映射与切换，用于从游戏数据中抽取不同资源界的数据块。
 *
 * 注意事项：
 * - Windows 专用 API（HANDLE 等）在跨平台时需要替换。
 * - MCB 分配可能产生碎片，注意及时 SysMemFree 并避免越界访问。
 * - FileInfo 的 filename 与 filehandle 约束：0 表示不存在；请在关闭后再重用句柄。
 */

#define MEM_OK					0
#define MEM_MCB_ERROR           1

#define MCB_BLANK               'b'
#define MCB_USE                 'u'

#define MCB_END                 'e'
#define MCB_NORMAL              'n'

#define MCB_LENGTH              4				/*  内存控制块的大小 */

#define	MIN_BLK_BYTES			4				/*  分配的最小字节 */
#define	MIN_BLK_MASK			0x03
#define	MIN_BLK_NMASK			0xfffc


/*
 * MCB（内存控制块）：用于管理自定义堆的分配/释放。
 * 字段：
 * - use_flag：使用标志（'u' 使用中，'b' 空闲）
 * - end_flag：结束标志（'e' 该块是最后一个控制块，'n' 非末尾）
 * - len：用户数据区长度（不含 MCB_LENGTH）
 * 分配策略：
 * - SysMemInit(start,len) 初始化堆；SysMemAllocate(len) 最小按 4 字节对齐（MIN_BLK_BYTES）。
 * - SysMemFree(ptr) 释放并尝试合并相邻空闲块；返回 MEM_OK/MEM_MCB_ERROR。
 */
typedef struct	tagMCB
{
    UINT8      use_flag;               /*	使用标志 */
    UINT8      end_flag;               /*  结束标志 */
    UINT16 	len;					/*  分配的空间长度，不含MCB长度 */
}MCB;

void fillmem(UINT8*, UINT16, UINT8);

/* 文本：屏幕坐标 (x,y) 打印字符串（支持 ASCII 与汉字编码，详见 middle.c 的映射规则） */
void SysPrintString(UINT8 x, UINT8 y, const UINT8* str);
void SysLine(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2);
void SysRect(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2);
void SysFillRect(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2);
/* 屏幕局部保存/恢复：将矩形区域保存到/从 BuffPoint 还原 */
void SysSaveScreen(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* BuffPoint);
void SysRestoreScreen(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* BuffPoint);
/* 图片绘制：pic/Screen 缓冲区到屏幕矩形，flag 控制透明或合成模式（具体见实现） */
void SysPicture(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* BuffPoint, UINT8 flag);
void SysLcdPartClear(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2);
void SysAscii(UINT8 x, UINT8 y, UINT8 asc);
void SysLcdReverse(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2);
void SysPictureDummy(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* pic, UINT8* Screen, UINT8 flag);
UINT8 SysGetSecond();
void SysTimer1Open(UINT8 times);
void SysTimer1Close();
void SysIconAllClear(void);						/*清除所有icon显示,系统的除外 */
/* 数据Bank：逻辑 Bank → 物理 Bank 的映射与切换。bankNumber 为逻辑跨度，physicalStartBank 为物理起始（单位与实现一致）。 */
void DataBankSwitch(UINT8 logicStartBank, UINT8 bankNumber, UINT16 physicalStartBank);
/* 根据逻辑起始 Bank 取对应的物理 Bank 编号（用于资源定位）。 */
void GetDataBankNumber(UINT8 logicStartBank, UINT16* physicalBankNumber);
/* 输入与按键音：设置/获取按键音开关；读取键盘扫描码（详见 GuiTranslateMsg 的行为）。 */
void SysSetKeySound(UINT8 keySoundFlag);
UINT8   SysGetKeySound();
UINT8	 SysGetKey();
void SysPlayMelody(UINT8 melodyNum);
void SysStopMelody();
/* 内存堆：初始化/分配/释放。分配返回指向数据区的指针；释放返回 MEM_OK/MEM_MCB_ERROR。 */
void SysMemInit(UINT16 start, UINT16 len);
char* SysMemAllocate(UINT16 len);
UINT8 SysMemFree(char* p);
UINT16 SysRand(PtrRandEnv pRandEnv);
void SysSrand(PtrRandEnv pRandEnv, UINT16 seed, UINT16 randMax);
/* 内存工具：拷贝/比较（返回值见实现），用于资源缓冲操作与屏幕合成。 */
void SysMemcpy(UINT8* dest, const UINT8* src, UINT16 len);
UINT8 SysMemcmp(UINT8* dest, const UINT8* src, UINT16 len);
void GuiSetInputFilter(UINT8 filter); /* 键盘屏蔽属性 */
void GuiSetKbdType(UINT8 type); /* 键盘类型 */
UINT8 GuiGetMsg(PtrMsg pMsg);
UINT8 GuiTranslateMsg(PtrMsg pMsg); /* 转换扫描码为字符，或从输入法得到汉字等（可能改写 pMsg 内容） */
UINT8 GuiInit(void);
UINT16 GuiGetKbdState(); /* 取键盘状态 */
void GuiSetKbdState(UINT16 state); /* 恢复键盘状态 */
UINT8 GuiMsgBox(UINT8* strMsg, UINT16 nTimeout);

#define NoOpen			0x00
#define	ReadOnly		0x01
#define	ReadAndWrite	0x02
#define FromTop		0x01
#define	FromCurrent	0x02
#define	FromEnd		0x03
/*
 * 文件信息结构：
 * - filename：文件逻辑编号（0 表示不存在），与 filehandle 对应；
 * - information[10]：文件名/描述信息（ASCII）；
 * - filetype：数据类型（自定义分类，与 DAT_* 常量配合）；
 * - hFile：Windows HANDLE；仅在打开/创建后有效，关闭后需置空。
 */
typedef struct _FileInfo
{
    UINT16 filename;
    UINT8 information[10];
    UINT8 filetype;
    HANDLE hFile;
} FileInfo;
// filename值为0代表没有这个文件。filehandle值与filename相同
// information[10]为字符串的文件名称
#define FILE_NUM 16
extern FileInfo fileinfos[FILE_NUM];

/* 文件系统：创建/打开/删除/读写/关闭/定位/统计/查找。返回值为状态码（0 成功，其余参见实现）。 */
UINT8 FileCreat(UINT8 filetype, UINT32 filelength, UINT8* information, UINT16* filename, UINT8* filehandle);
UINT8 FileOpen(UINT16 filename, UINT8 filetype, UINT8 openmode, UINT8* filehandle, UINT32* filelength);
UINT8 FileDel(UINT8 filehandle);
UINT8 FileWrite(UINT8 filehandle, UINT8 datalength, UINT8* bufadd);
UINT8 FileClose(UINT8 filehandle);
UINT8 FileRead(UINT8 filehandle, UINT8 datalength, UINT8* bufadd);
UINT8 FileSeek(UINT8 filehandle, UINT32 fileoffset, UINT8 origin);
void FlashInit();
UINT8 FileNum(UINT8 filetype, UINT16* filenum);
// information[10]
UINT8 FileSearch(UINT8 filetype, UINT16 fileorder, UINT16* filename, UINT8* information);
#include "middle.h"
#include "framework.h"
#include "font.h"

extern UINT8 MCU_memory_dummy[0x8000];
extern UINT8 MCU_memory[0x10000];
#define Mem_Start				*(UINT32*)(MCU_memory+0x2BAB)
#define Mem_Len					*(UINT32*)(MCU_memory+0x2BAF)
#define Mem_Flag				MCU_memory[0x2BB3]
UINT8* pGameData;
static UINT16 _4BankNumber = 4;
static UINT16 _9BankNumber = 0;
static UINT_PTR timer;
FileInfo fileinfos[FILE_NUM];

void fillmem(UINT8* dst, UINT16 size, UINT8 value)
{
    memset(dst, value, size);
}

/*
 * SysPrintString：在屏幕坐标(x,y)绘制字符串
 * - 参数：x,y 左上角像素位置；str 以0结尾的字符串（支持ASCII与部分双字节汉字区）。
 * - 行为：根据字符编码计算字体数据偏移，从 `font_data` 取字模，用 `SysPicture` 绘制；自动换行。
 * 注意：假定单字节编码+特定双字节区间；超出范围时回退到默认偏移；越界坐标会提前返回。
 */
void SysPrintString(UINT8 x, UINT8 y, const UINT8* str)
{
    //LOG("SysPrintString: %s\n", str);
    UINT32 offset = 0;
    while (str[0] && y<0x51)
    {
        if (str[0] < 0x80)
        {
            // ASCII码
            if (x >= 0x99)
            {
                x = 0x00;
                y += 0x10;
                continue;
            }
        }
        else if (str[0] > 0x80)
        {
            if ((str[0] >= 0xFD) && (str[1] >= 0xA1))
            {
                if (x >= 0x99)
                {
                    x = 0x00;
                    y += 0x10;
                    continue;
                }
            }
            else
            {
                if (x >= 0x91)
                {
                    x = 0x00;
                    y += 0x10;
                    continue;
                }
            }
        }
        if (str[0] < 0x80)
        {
            offset = 0x0003D740 + str[0] * 0x0010;
        }
        else if ((0xB0 <= str[0] && str[0] <= 0xF7) && (0xA1 <= str[1] && str[1] <= 0xFE))
        {
            offset = 0x00000000 + (str[1] - 0xA1 + (str[0] - 0xB0) * 0x5E) * 0x0020;
        }
        else if ((0xA1 <= str[0] && str[0] <= 0xA9) && (0xA1 <= str[1] && str[1] <= 0xFE))
        {
            offset = 0x00034E00 + (str[1] - 0xA1 + (str[0] - 0xA1) * 0x5E) * 0x0020;
        }
        else if ((0xA8 <= str[0] && str[0] <= 0xA9) && (0x40 <= str[1] && str[1] <= 0xA0))
        {
            //                                  跳过0x7F
            offset = 0x0003BD80 + (str[1] - (str[1] < 0x80 ? 0x40 : 0x41) + *(UINT16*)(MCU_memory + 0x20AF)) * 0x0020;
        }
        else if ((0xAA <= str[0] && str[0] <= 0xAF) && (0xA1 <= str[1] && str[1] <= 0xFE))
        {
            offset = 0x0003DF40 + (str[1] - 0xA1 + (str[1] - 0xAA) * 0x5E) * 0x0020;
        }
        else if ((0xF8 <= str[0] && str[0] <= 0xFE) && (0xA1 <= str[1] && str[1] <= 0xFE))
        {
            offset = 0x000425C0 + (str[1] - 0xA1 + (str[0] - 0xF8) * 0x5E) * 0x0020;
        }
        else if ((0xA1 <= str[0] && str[0] <= 0xA7) && (0x40 <= str[1] && str[1] <= 0xA0))
        {
            // 0x7F处字符为空白，不用跳过
            offset = 0x00047800 + (str[1] - 0x40 + (str[0] - 0xA1) * 0x61) * 0x0020;
        }
        else
        {
            offset = 0x00034E00;
        }

        if (str[0] < 0x80)
        {
            // ASCII码
            SysPicture(x, y, x + 7, y + 15, font_data + offset, 0);
            str += 1;
            x += 0x08;
        }
        else if (str[0] > 0x80)
        {
            SysPicture(x, y, x + 15, y + 15, font_data + offset, 0);
            str += 2;
            if ((str[0] >= 0xFD) && (str[1] >= 0xA1))
            {
                x += 0x08;
            }
            else
            {
                x += 0x10;
            }
        }
    }
}

/*
 * SysPutPixel：设置单个像素
 * - 参数：x,y 像素坐标；data=1置位，0清除。
 * - 行为：定位位图缓冲区MCU_memory[0x400...]，按位掩码修改对应bit。
 * 注意：越界则直接返回；屏幕宽160像素，高96像素，1bpp。
 */
void SysPutPixel(UINT8 x, UINT8 y, UINT8 data)
{
    if (x >= 159 || y >= 96)
    {
        return;
    }
    UINT8* p = MCU_memory + 0x400 + 160 / 8 * y + x / 8;
    UINT8 mask = 0x01 << (7-(x & 0x07));
    if (data == 0)
    {
        *p &= ~mask;
    }
    else
    {
        *p |= mask;
    }
}

/*
 * SysLine：画直线
 * - 参数：起点(x1,y1)、终点(x2,y2)。
 * - 行为：使用简单插值/近似算法遍历像素并调用 `SysPutPixel`。
 * 注意：坐标归一化处理会交换起点终点；该算法非抗锯齿。
 */
void SysLine(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2)
{
    if (x1 > x2)
    {
        UINT8 tmp = x1;
        x1 = x2;
        x2 = tmp;
        tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    UINT8 dx = x2 - x1;
    if (y1 > y2)
    {
        UINT8 dy = y1 - y2;
        if (dy > dx)
        {
            for (UINT8 y = y2; y <= y1; y++)
            {
                UINT8 x = x2 - (UINT8)(ceil(dx * (y - y2) * 1.0 / dy));
                SysPutPixel(x, y, 0x01);
            }
        }
        else
        {
            for (UINT8 x = x1; x <= x2; x++)
            {
                UINT8 y = y1 - (UINT8)(ceil(dy * (x - x1) * 1.0 / dx));
                SysPutPixel(x, y, 0x01);
            }
        }
    }
    else
    {
        UINT8 dy = y2 - y1;
        if (dy > dx)
        {
            for (UINT8 y = y1; y <= y2; y++)
            {
                UINT8 x = x1 + (UINT8)(ceil(dx * (y - y1) * 1.0 / dy));
                SysPutPixel(x, y, 0x01);
            }
        }
        else
        {
            for (UINT8 x = x1; x <= x2; x++)
            {
                UINT8 y = y1 + (UINT8)(ceil(dy * (x - x1) * 1.0 / dx));
                SysPutPixel(x, y, 0x01);
            }
        }
    }
}

/*
 * SysRect：画矩形框
 * - 参数：左上(x1,y1)、右下(x2,y2)。
 * - 行为：调用 `SysLine` 依次绘制四条边。
 * 注意：越界直接返回；坐标校正保证x1<=x2,y1<=y2。
 */
void SysRect(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2)
{
    if ((x1 >= 159) || (x2 >= 159) || (y1 >= 96) || (y2 >= 96))
    {
        return;
    }
    SysLine(x1, y1, x1, y2);
    SysLine(x1, y1, x2, y1);
    SysLine(x2, y1, x2, y2);
    SysLine(x1, y2, x2, y2);
}

/*
 * SysFillRect：填充矩形区域
 * - 参数：左上(x1,y1)、右下(x2,y2)。
 * - 行为：逐行按字节块写入，边缘处理掩码对齐位段。
 * 注意：使用位运算处理起止非字节对齐；避免越界访问。
 */
void SysFillRect(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2)
{
    if ((x1 >= 159) || (x2 >= 159) || (y1 >= 96) || (y2 >= 96))
    {
        return;
    }
    if (x1 > x2)
    {
        UINT8 tmp = x1;
        x2 = x1;
        x1 = tmp;
    }
    if (y1 > y2)
    {
        UINT8 tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    for (UINT8 y = y1; y <= y2; y++)
    {
        UINT8* p = MCU_memory + 0x400 + 160 / 8 * y + x1 / 8;
        if (x1 & 0x07)
        {
            if ((x1 & 0xF8) == (x2 & 0xF8))
            {
                *p |= (0xFF >> (x1&0x07)) & (0xFF << (7 - (x2 & 0x07)));
                continue;
            }
            *p |= 0xFF >> (x1 & 0x07);
            p++;
        }
        for (UINT8 x = (x1 + 7) & 0xF8; x < (x2 & 0xF8); x += 8)
        {
            *p = 0xFF;
            p++;
        }
        if (x2 & 0x07)
        {
            *p |= 0xFF << (7 - (x2 & 0x07));
        }
    }
}

/*
 * SysSaveScreen：保存屏幕区域到缓冲
 * - 参数：矩形区域(x1,y1)-(x2,y2)；BuffPoint 目标缓冲。
 * - 行为：将屏幕位图按字节块复制到 BuffPoint，便于后续恢复。
 * 注意：BuffPoint大小需由 `SysCalcScrBufSize` 事先计算；越界与坐标顺序会被校正。
 */
void SysSaveScreen(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* BuffPoint)
{
    if ((x1 >= 159) || (x2 >= 159) || (y1 >= 96) || (y2 >= 96))
    {
        return;
    }
    if (x1 > x2)
    {
        UINT8 tmp = x1;
        x2 = x1;
        x1 = tmp;
    }
    if (y1 > y2)
    {
        UINT8 tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    for (UINT8 y = y1; y <= y2; y++)
    {
        for (UINT8 offset = x1 / 8; offset <= x2 / 8; offset++)
        {
            BuffPoint[(offset - x1 / 8) + (y - y1) * (x2 / 8 - x1 / 8 + 1)] = MCU_memory[0x400 + 160 / 8 * y + offset];
        }
    }
}

/*
 * SysRestoreScreen：从缓冲恢复屏幕区域
 * - 参数：矩形区域与 BuffPoint 源缓冲。
 * - 行为：将 BuffPoint 内容写回屏幕位图，实现临时覆盖后的回滚。
 * 注意：BuffPoint需与保存时的区域/布局一致。
 */
void SysRestoreScreen(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* BuffPoint)
{
    if ((x1 >= 159) || (x2 >= 159) || (y1 >= 96) || (y2 >= 96))
    {
        return;
    }
    if (x1 > x2)
    {
        UINT8 tmp = x1;
        x2 = x1;
        x1 = tmp;
    }
    if (y1 > y2)
    {
        UINT8 tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    for (UINT8 y = y1; y <= y2; y++)
    {
        for (UINT8 offset = x1 / 8; offset <= x2 / 8; offset++)
        {
            MCU_memory[0x400 + 160 / 8 * y + offset] = BuffPoint[(offset - x1 / 8) + (y - y1) * (x2 / 8 - x1 / 8 + 1)];
        }
    }
}

/*
 * SysPicture：绘制位图区域
 * - 参数：目标矩形、位图数据指针 BuffPoint、flag=0正常绘制/1反色绘制。
 * - 行为：按行位偏移提取像素并写入屏幕缓冲；支持反色模式。
 * 注意：位图行宽按 `(x2-x1+1+7)&0xF8` 字节对齐；坐标越界直接返回。
 */
void SysPicture(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* BuffPoint, UINT8 flag)
{
    if ((x1 >= 159) || (x2 >= 159) || (y1 >= 96) || (y2 >= 96))
    {
        return;
    }
    if (x1 > x2)
    {
        UINT8 tmp = x1;
        x2 = x1;
        x1 = tmp;
    }
    if (y1 > y2)
    {
        UINT8 tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    for (UINT8 y = y1; y <= y2; y++)
    {
        for (UINT8 x = x1; x <= x2; x++)
        {
            UINT8* p = MCU_memory + 0x400 + 160 / 8 * y + x / 8;
            UINT16 offset_bits = (y - y1) * ((x2 - x1 + 1+7)&0xF8) + (x - x1);
            UINT8 data = (BuffPoint[offset_bits / 8] >> (7 - (offset_bits & 0x07))) & 0x01;
            UINT8 mask = 1 << (7 - (x & 0x07));
            if (flag == 0)
            {
                if (data > 0)
                {
                    *p |= mask;
                }
                else
                {
                    *p &= ~mask;
                }
            }
            else
            {
                if (data > 0)
                {
                    *p &= ~mask;
                }
                else
                {
                    *p |= mask;
                }
            }
        }
    }
}

/*
 * SysLcdPartClear：清空屏幕区域
 * - 参数：矩形区域。
 * - 行为：将目标区域位图数据按字节清零，边缘使用掩码处理对齐。
 */
void SysLcdPartClear(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2)
{
    if ((x1 >= 159) || (x2 >= 159) || (y1 >= 96) || (y2 >= 96))
    {
        return;
    }
    if (x1 > x2)
    {
        UINT8 tmp = x1;
        x2 = x1;
        x1 = tmp;
    }
    if (y1 > y2)
    {
        UINT8 tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    for (UINT8 y = y1; y <= y2; y++)
    {
        UINT8* p = MCU_memory + 0x400 + 160 / 8 * y + x1 / 8;
        if (x1 & 0x07)
        {
            if ((x1 & 0xF8) == (x2 & 0xF8))
            {
                *p &= ~((0xFF >> (x1 & 0x07)) & (0xFF << (7 - (x2 & 0x07))));
                continue;
            }
            *p &= ~(0xFF >> (x1 & 0x07));
            p++;
        }
        for (UINT8 x = (x1 + 7) & 0xF8; x < (x2 & 0xF8); x += 8)
        {
            *p = 0x00;
            p++;
        }
        if (x2 & 0x07)
        {
            *p &= ~(0xFF << (7 - (x2 & 0x07)));
        }
    }
}

/*
 * SysAscii：绘制单个ASCII字符
 * - 参数：x,y 左上角，asc 0..255。
 * - 行为：从 `font_data` 的ASCII区取字模，调用 `SysPicture` 绘制。
 */
void SysAscii(UINT8 x, UINT8 y, UINT8 asc)
{
    UINT32 offset = 0;
    if (x >= 0x99)
    {
        return;
    }
    if (y >= 0x51)
    {
        return;
    }
    offset = 0x0003D740 + asc * 0x0010;
    SysPicture(x, y, x + 7, y + 15, font_data + offset, 0);
}

/*
 * SysLcdReverse：反色显示区域
 * - 参数：矩形区域。
 * - 行为：按字节/位异或，实现反色效果。
 */
void SysLcdReverse(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2)
{
    if ((x1 >= 159) || (x2 >= 159) || (y1 >= 96) || (y2 >= 96))
    {
        return;
    }
    if (x1 > x2)
    {
        UINT8 tmp = x1;
        x2 = x1;
        x1 = tmp;
    }
    if (y1 > y2)
    {
        UINT8 tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    for (UINT8 y = y1; y <= y2; y++)
    {
        UINT8* p = MCU_memory + 0x400 + 160 / 8 * y + x1 / 8;
        if (x1 & 0x07)
        {
            if ((x1 & 0xF8) == (x2 & 0xF8))
            {
                *p ^= (0xFF >> (x1 & 0x07)) & (0xFF << (7 - (x2 & 0x07)));
                continue;
            }
            *p ^= 0xFF >> (x1 & 0x07);
            p++;
        }
        for (UINT8 x = (x1 + 7) & 0xF8; x < (x2 & 0xF8); x += 8)
        {
            *p ^= 0xFF;
            p++;
        }
        if (x2 & 0x07)
        {
            *p ^= 0xFF << (7 - (x2 & 0x07));
        }
    }
}

/*
 * SysPictureDummy：在指定屏幕缓冲上绘制位图
 * - 参数：目标矩形、位图指针pic、自定义屏幕缓冲指针Screen、flag绘制模式。
 * - 行为：与 `SysPicture` 相同，但不写主屏幕，而写传入的 Screen 缓冲。
 * 用途：离屏绘制/特效处理。
 */
void SysPictureDummy(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* pic, UINT8* Screen, UINT8 flag)
{
    if ((x1 >= 159) || (x2 >= 159) || (y1 >= 96) || (y2 >= 96))
    {
        return;
    }
    if (x1 > x2)
    {
        UINT8 tmp = x1;
        x2 = x1;
        x1 = tmp;
    }
    if (y1 > y2)
    {
        UINT8 tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    for (UINT8 y = y1; y <= y2; y++)
    {
        for (UINT8 x = x1; x <= x2; x++)
        {
            UINT8* p = Screen + 160 / 8 * y + x / 8;
            UINT16 offset_bits = (y - y1) * ((x2 - x1 + 1 + 7)&0xF8) + (x - x1);
            UINT8 data = (pic[offset_bits / 8] >> (7 - (offset_bits & 0x07))) & 0x01;
            UINT8 mask = 1 << (7 - (x & 0x07));
            if (flag == 0)
            {
                if (data > 0)
                {
                    *p |= mask;
                }
                else
                {
                    *p &= ~mask;
                }
            }
            else
            {
                if (data > 0)
                {
                    *p &= ~mask;
                }
                else
                {
                    *p |= mask;
                }
            }
        }
    }
}

/*
 * SysGetSecond：获取当前秒数（UTC）
 * - 行为：调用 `GetSystemTime` 并返回 `wSecond` 的低8位。
 */
UINT8 SysGetSecond()
{
    SYSTEMTIME lt;
    GetSystemTime(&lt);
    return (UINT8)lt.wSecond;
}

/*
 * Timerproc：定时器回调（Windows）
 * - 作用：设置全局定时器标志位 `SYS_FLAG_TIMER_MASK`。
 * 注意：当前未被调用；`SysTimer1Open` 使用此回调注册Windows定时器。
 */
void Timerproc(
    HWND unnamedParam1,
    UINT unnamedParam2,
    UINT_PTR unnamedParam3,
    DWORD unnamedParam4
)
{// 未被调用
    MCU_memory[0x201E] |= SYS_FLAG_TIMER_MASK;
}

/*
 * SysTimer1Open：开启定时器
 * - 参数：times（单位10ms）；内部乘以10传给 Windows `SetTimer`。
 * - 行为：关闭已有定时器后开启新的；回调为 `Timerproc`。
 */
void SysTimer1Open(UINT8 times)
{
    SysTimer1Close();
    timer = SetTimer(NULL, 1, times*10, Timerproc);
}

/*
 * SysTimer1Close：关闭定时器
 * - 行为：若存在则调用 `KillTimer` 并清零句柄。
 */
void SysTimer1Close()
{
    if (timer != 0)
    {
        KillTimer(NULL, timer);
        timer = 0;
    }
}

/*
 * SysIconAllClear：清空图标（占位）
 * - 当前实现为空；可能用于清理状态图标层。
 */
void SysIconAllClear(void)
{

}

/*
 * DataBankSwitch：数据Bank切换
 * - 参数：逻辑起始Bank、Bank数量、物理起始Bank号。
 * - 行为：逻辑Bank 0x04（1个块）在主内存0x4000镜像切换；逻辑Bank 0x09（4个块）从 `pGameData` 读取到0x9000。
 * - 异常：其它组合将触发 `RaiseException`。
 * 注意：依赖 `MCU_memory_dummy` 作为暂存；拷贝字节数为 `bankNumber*0x1000`。
 */
void DataBankSwitch(UINT8 logicStartBank, UINT8 bankNumber, UINT16 physicalStartBank)
{
    if (logicStartBank == 0x04 && bankNumber == 0x01)
    {
        memcpy(MCU_memory_dummy + _4BankNumber * 0x1000, MCU_memory + 0x4000, bankNumber*0x1000);
        _4BankNumber = physicalStartBank;
        memcpy(MCU_memory + 0x4000, MCU_memory_dummy + _4BankNumber * 0x1000, bankNumber*0x1000);
    }
    else if (logicStartBank == 0x09 && bankNumber == 0x04)
    {
        _9BankNumber = physicalStartBank;
        memcpy(MCU_memory + 0x9000, pGameData + physicalStartBank * 0x1000, bankNumber*0x1000);
    }
    else
    {
        RaiseException(logicStartBank, bankNumber, 0, NULL);
    }
}

/*
 * GetDataBankNumber：查询逻辑Bank当前映射的物理Bank号
 * - 支持逻辑Bank 0x09；其它情况抛出异常。
 */
void GetDataBankNumber(UINT8 logicStartBank, UINT16* physicalBankNumber)
{
    if (logicStartBank == 0x09)
    {
        *physicalBankNumber = _9BankNumber;
    }
    else
    {
        RaiseException(logicStartBank, 0xFF, 0, NULL);
    }
}

/*
 * SysSetKeySound：设置按键音开关（占位）
 * - 当前未实现；引擎通过 `SysGetKeySound/SysSetKeySound` 记录/恢复状态。
 */
void SysSetKeySound(UINT8 keySoundFlag)
{

}

/*
 * SysGetKeySound：获取按键音状态（占位）
 * - 当前返回0；与实际设备行为可能不一致，建议后续完善。
 */
UINT8   SysGetKeySound()
{
    return 0;
}

/*
 * SysGetKey：获取键盘输入（Windows）
 * - 行为：PeekMessage读取 `WM_KEYDOWN`，转换为内部键码；处理 `WM_TIMER` 设置定时器标志。
 * 返回：键码或 0xFF 表示无键。
 * 注意：仅映射了部分字母与方向/功能键；可扩展。
 */
UINT8	 SysGetKey()
{
    MSG msg;
    if (PeekMessage(&msg, NULL, WM_NULL, WM_NULL, PM_REMOVE))
    {
        if (msg.message == WM_KEYDOWN)
        {
            UINT8 keycode = 0xFF;
            switch (LOBYTE(msg.wParam))
            {
            case VK_UP:
                keycode = KEY_UP;
                break;
            case VK_DOWN:
                keycode = KEY_DOWN;
                break;
            case VK_LEFT:
                keycode = KEY_LEFT;
                break;
            case VK_RIGHT:
                keycode = KEY_RIGHT;
                break;
            case VK_PRIOR:
                keycode = KEY_PGUP;
                break;
            case VK_NEXT:
                keycode = KEY_PGDN;
                break;
            case VK_RETURN:
                keycode = KEY_ENTER;
                break;
            case VK_ESCAPE:
                keycode = KEY_EXIT;
                break;
            case VK_SPACE:
                keycode = KEY_SPACE;
                break;
            case 0x41:
                keycode = KEY_A;
                break;
            case 0x44:
                keycode = KEY_D;
                break;
            case 0x52:
                keycode = KEY_R;
                break;
            case 0x45:
                keycode = KEY_E;
                break;
            case 0x57:
                keycode = KEY_W;
                break;
            }
            return keycode;
        }
        else if (msg.message == WM_TIMER)
        {
            //SysTimer1Close();
            MCU_memory[0x201E] |= SYS_FLAG_TIMER_MASK;
        }
    }
    return 0xFF;
}

/*
 * SysPlayMelody：播放旋律（占位）
 * - 当前未实现。
 */
void SysPlayMelody(UINT8 melodyNum)
{

}

/*
 * SysStopMelody：停止旋律（占位）
 * - 当前未实现。
 */
void SysStopMelody()
{

}

/*
 * SysMemInit：初始化自定义堆（MCB）
 * - 参数：start 起始地址，len 总长度（字节）。
 * - 行为：对齐到4字节，设置 `Mem_Start/Len/Flag`，初始化首块MCB为空闲并标记结束。
 * 注意：要求堆位于 `MCU_memory` 内，且长度满足至少一个MCB头+数据。
 */
void SysMemInit(UINT16 start, UINT16 len)
{
    fillmem(MCU_memory + start, len, 0x00);
    UINT16 _17CD = start & MIN_BLK_MASK;
    if (_17CD)
    {
        start = (start + MIN_BLK_BYTES) & MIN_BLK_NMASK;
        len -= _17CD;
    }
    len &= MIN_BLK_NMASK;
    Mem_Start = start;
    Mem_Len = len;
    MCB* pMcb = (MCB*)(MCU_memory + start);
    pMcb->use_flag = MCB_BLANK;
    pMcb->end_flag = MCB_END;
    pMcb->len = len - MCB_LENGTH;
    Mem_Flag = MEM_OK;
}

/*
 * _00EA3234（Mem_MCB_Break）：按长度拆分MCB
 * - 参数：MCB指针、分配长度 `_17D2`。
 * - 行为：若剩余长度>4字节，则拆分为已用块与余下块；保持use/end标志。
 */
UINT8 _00EA3234(MCB* _17D0, UINT16 _17D2) // Mem_MCB_Break
{
    if (_17D0->len - _17D2 <= 0x0004)
    {
        return 0x01;
    }
    UINT8 _17C4 = _17D0->use_flag;
    UINT8 _17C5 = _17D0->end_flag;
    UINT16 _17CA = _17D0->len;
    _17D0->len = _17D2;
    _17D0->end_flag = MCB_NORMAL;
    UINT16 _17C6 = _17CA - _17D2 - 0x0004;
    UINT8* _17CC = (UINT8*)_17D0 + _17D2 + 0x0004;
    _17D0 = (MCB*)_17CC;
    _17D0->end_flag = _17C5;
    _17D0->use_flag = _17C4;
    _17D0->len = _17C6;
    return 0x01;
}

/*
 * Mem_MCB_Valid：校验MCB有效性
 * - 行为：检查 `end_flag` 合法与地址范围不越界；错误则设置 `Mem_Flag=MEM_MCB_ERROR`。
 */
UINT8 Mem_MCB_Valid(MCB* _17D0)
{
    if ((_17D0->end_flag == MCB_NORMAL) || (_17D0->end_flag == MCB_END))
    {
        UINT8* _17CC = (UINT8*)_17D0 + 0x0004 + _17D0->len;
        if ((_17CC > MCU_memory + Mem_Start + Mem_Len) || ((UINT8*)_17D0 < MCU_memory + Mem_Start))
        {
            Mem_Flag = MEM_MCB_ERROR;
            return 0x00;
        }
        else
        {
            return 0x01;
        }
    }
    else
    {
        Mem_Flag = MEM_MCB_ERROR;
        return 0x00;
    }
}

/*
 * Mem_MCB_Next：获取下一块MCB
 * - 行为：使用当前块长度与头部跳转到下一块；越界或结束返回NULL。
 */
MCB* Mem_MCB_Next(MCB* _17D0)
{
    MCB* _17CE = _17D0;
    if (((UINT8*)_17D0 < MCU_memory + Mem_Start) || ((UINT8*)_17D0 > MCU_memory + Mem_Start + Mem_Len))
    {
        return NULL;
    }
    if (_17CE->end_flag == MCB_END)
    {
        return NULL;
    }
    if (Mem_MCB_Valid(_17CE) == 0x00)
    {
        return NULL;
    }
    _17D0 = (MCB*)(_17CE->len + (UINT8*)_17D0 + 0x0004);
    if (Mem_MCB_Valid(_17D0) == 0x00)
    {
        return NULL;
    }
    return _17D0;
}

/*
 * Mem_MCB_Merge：合并相邻空闲块
 * - 行为：遍历MCB链，将相邻的空闲块合并，减少碎片。
 */
UINT8 Mem_MCB_Merge()
{
    MCB* _17CA = (MCB*)(MCU_memory + Mem_Start);
    MCB* _17C8 = (MCB*)(MCU_memory + Mem_Start);
    while (_17CA->end_flag == MCB_NORMAL)
    {
        MCB* _17CC = Mem_MCB_Next(_17CA);
        if (_17CC == NULL)
        {
            return 0x01;
        }
        MCB* _17C8 = _17CC;
        if ((_17CA->use_flag == MCB_BLANK) && (_17C8->use_flag == MCB_BLANK))
        {
            _17CA->len = _17C8->len + _17CA->len + 0x0004;
            _17CA->end_flag = _17C8->end_flag;
        }
        else
        {
            _17CA = _17C8;
        }
    }
    return 0x01;
}

/*
 * SysMemAllocate：分配堆内存
 * - 参数：len 请求长度，向上对齐至4字节。
 * - 行为：遍历MCB找到合适空闲块，调用拆分并标记为使用；返回数据指针（跳过MCB头）。
 * 返回：指针或NULL（无可用块）。
 */
char* SysMemAllocate(UINT16 len)
{
    if (len & 0x0003)
    {
        len = (len + 0x0004) & 0xFFFC;
    }
    MCB* _17CE = (MCB*)(MCU_memory + Mem_Start);
    while ((_17CE->use_flag != MCB_BLANK) || (_17CE->len - 0x0004 < len))
    {
        MCB* _17CC = Mem_MCB_Next(_17CE);
        if (_17CC == NULL)
        {
            return NULL;
        }
        else
        {
            _17CE = _17CC;
        }
    }
    _00EA3234(_17CE, len);
    _17CE->use_flag = MCB_USE;
    return (char*)(_17CE + 1);
}

/*
 * SysMemFree：释放堆内存
 * - 参数：p 为分配返回的指针。
 * - 行为：定位对应MCB，若非已空闲则标记为空闲并触发合并；非法地址返回0。
 */
UINT8 SysMemFree(char* p)
{
    if ((p - 0x0004 < (char*)(MCU_memory + Mem_Start))
        || (p - 0x0004 > (char*)(MCU_memory + Mem_Start + Mem_Len)))
    {
        return 0x00;
    }
    for (MCB* _17CC = (MCB*)(MCU_memory + Mem_Start); _17CC != NULL; _17CC = Mem_MCB_Next(_17CC))
    {
        if (p - 0x0004 == (char*)_17CC)
        {
            if (_17CC->use_flag == MCB_BLANK)
            {
                return 0x01;
            }
            else
            {
                _17CC->use_flag = MCB_BLANK;
                Mem_MCB_Merge();
                return 0x01;
            }
        }
    }
    return 0x00;
}

/*
 * SysRand：线性同余随机
 * - 参数：随机环境（包含next与randMax）。
 * - 行为：`next = next*0x41C64E6D + 0x3039`，返回 0..randMax。
 */
UINT16 SysRand(PtrRandEnv pRandEnv)
{
    pRandEnv->next = pRandEnv->next * 0x41C64E6D + 0x00003039;
    return (pRandEnv->next / 0x00010000) % (pRandEnv->randMax + 0x0001);
}

/*
 * SysSrand：设置随机种子与最大值
 */
void SysSrand(PtrRandEnv pRandEnv, UINT16 seed, UINT16  randMax)
{
    pRandEnv->next = seed;
    pRandEnv->randMax = randMax;
}

/*
 * SysMemcpy：包装标准 `memcpy`
 */
void SysMemcpy(UINT8* dest, const UINT8* src, UINT16 len)
{
    memcpy(dest, src, len);
}

/*
 * SysMemcmp：包装标准 `memcmp`
 */
UINT8 SysMemcmp(UINT8* dest, const UINT8* src, UINT16 len)
{
    return memcmp(dest, src, len);
}

void GuiSetInputFilter(UINT8 filter)
{

}

void GuiSetKbdType(UINT8 type)
{

}

/*
 * GuiPushMsg：向消息队列推送消息
 * - 行为：环形缓冲最大8条；更新读写索引与计数。
 * 返回：1 成功；0 队列已满。
 */
UINT8      GuiPushMsg(PtrMsg pMsg)
{
    if (MCU_memory[0x2B0B] < 0x08)
    {
        MCU_memory[0x2B09]--;
        MCU_memory[0x2B09] &= 0x07;
        ((PtrMsg)(MCU_memory + 0x2B0F))[MCU_memory[0x2B09]].param = pMsg->param;
        ((PtrMsg)(MCU_memory + 0x2B0F))[MCU_memory[0x2B09]].type = pMsg->type;
        MCU_memory[0x2B0B]++;
        return 0x01;
    }
    else
    {
        return 0x00;
    }
}

/*
 * GuiGetMsg：拉取消息（阻塞轮询）
 * - 优先级：定时器标志→队列→键盘；有消息则填充并返回1。
 */
UINT8 GuiGetMsg(PtrMsg pMsg)
{
    while (1)
    {
        if (MCU_memory[0x201E] & SYS_FLAG_TIMER_MASK)
        {
            MCU_memory[0x201E] &= ~SYS_FLAG_TIMER_MASK;
            pMsg->type = DICT_WM_TIMER;
            pMsg->param = 0x0000;
            return 0x01;
        }
        if (MCU_memory[0x2B0B])
        {
        _51E6:
            pMsg->type = ((PtrMsg)(MCU_memory + 0x2B0F))[MCU_memory[0x2B09]].type;
            pMsg->param = ((PtrMsg)(MCU_memory + 0x2B0F))[MCU_memory[0x2B09]].param;
            MCU_memory[0x2B0B]--;
            MCU_memory[0x2B0B] &= 0x07;
            MCU_memory[0x2B09]++;
            MCU_memory[0x2B09] &= 0x07;
            return 0x01;
        }
        UINT8 _17CD = SysGetKey();
        if (_17CD != 0xFF)
        {
            pMsg->type = DICT_WM_KEY;
            pMsg->param = _17CD;
            return 0x01;
        }
    }
    return 0x00;
}

/*
 * GuiTranslateMsg：将键盘原始键码映射到字母/功能消息
 * - 使用 0x40 表大小的常量表进行映射；非键消息原样返回。
 */
UINT8 GuiTranslateMsg(PtrMsg pMsg)
{
    UINT8 ConstKeybdMap[0x40][2] = {
        {DICT_WM_CHAR_FUN,CHAR_ON_OFF},{DICT_WM_CHAR_FUN,CHAR_HOME_MENU},{DICT_WM_CHAR_FUN,CHAR_EC_DICT},{DICT_WM_CHAR_FUN,CHAR_CE_DICT},{DICT_WM_DUMMY,0x00},{DICT_WM_DUMMY,0x00},{DICT_WM_DUMMY,0x00},{DICT_WM_DUMMY,0x00},
        {DICT_WM_CHAR_ASC,'1'},{DICT_WM_CHAR_ASC,'2'},{DICT_WM_CHAR_ASC,'3'},{DICT_WM_CHAR_ASC,'4'},{DICT_WM_CHAR_ASC,'5'},{DICT_WM_CHAR_ASC,'6'},{DICT_WM_CHAR_ASC,'7'},{DICT_WM_CHAR_ASC,'8'},
        {DICT_WM_CHAR_ASC,'q'},{DICT_WM_CHAR_ASC,'w'},{DICT_WM_CHAR_ASC,'e'},{DICT_WM_CHAR_ASC,'r'},{DICT_WM_CHAR_ASC,'t'},{DICT_WM_CHAR_ASC,'y'},{DICT_WM_CHAR_ASC,'u'},{DICT_WM_CHAR_ASC,'i'},
        {DICT_WM_CHAR_ASC,'a'},{DICT_WM_CHAR_ASC,'s'},{DICT_WM_CHAR_ASC,'d'},{DICT_WM_CHAR_ASC,'f'},{DICT_WM_CHAR_ASC,'g'},{DICT_WM_CHAR_ASC,'h'},{DICT_WM_CHAR_ASC,'j'},{DICT_WM_CHAR_ASC,'k'},
        {DICT_WM_CHAR_FUN,CHAR_INPUT},{DICT_WM_CHAR_ASC,'z'},{DICT_WM_CHAR_ASC,'x'},{DICT_WM_CHAR_ASC,'c'},{DICT_WM_CHAR_ASC,'v'},{DICT_WM_CHAR_ASC,'b'},{DICT_WM_CHAR_ASC,'n'},{DICT_WM_CHAR_ASC,'m'},
        {DICT_WM_CHAR_FUN,CHAR_ZY},{DICT_WM_DUMMY,0x00},{DICT_WM_DUMMY,0x00},{DICT_WM_DUMMY,0x00},{DICT_WM_DUMMY,0x00},{DICT_WM_CHAR_FUN,CHAR_SHIFT},{DICT_WM_CHAR_FUN,CHAR_EXIT},{DICT_WM_CHAR_FUN,CHAR_ENTER},
        {DICT_WM_CHAR_ASC,'9'},{DICT_WM_CHAR_ASC,'0'},{DICT_WM_CHAR_ASC,'o'},{DICT_WM_CHAR_ASC,'p'},{DICT_WM_CHAR_ASC,'l'},{DICT_WM_CHAR_FUN,CHAR_UP},{DICT_WM_CHAR_ASC,' '},{DICT_WM_CHAR_FUN,CHAR_LEFT},
        {DICT_WM_CHAR_FUN,CHAR_DOWN},{DICT_WM_CHAR_FUN,CHAR_RIGHT},{DICT_WM_CHAR_FUN,CHAR_PGUP},{DICT_WM_CHAR_FUN,CHAR_PGDN},{DICT_WM_DUMMY,0x00},{DICT_WM_DUMMY,0x00},{DICT_WM_DUMMY,0x00},{DICT_WM_DUMMY,0x00}};
    if (pMsg->type != DICT_WM_KEY)
    {
        return 0x01;
    }
    UINT8 _17CD = (UINT8)(pMsg->param);
    pMsg->type = ConstKeybdMap[_17CD & 0x3F][0];
    pMsg->param = ConstKeybdMap[_17CD & 0x3F][1];
    return 0x01;
}

/*
 * GuiInit：初始化GUI层
 * - 行为：将屏幕缓冲区填充为全1（白色），返回0。
 */
UINT8 GuiInit(void)
{
    FillMemory(MCU_memory+0x400, 0xC00, 0xFF);
    return 0;
}

/*
 * GuiGetKbdState：获取键盘状态（占位，返回0）
 */
UINT16 GuiGetKbdState()
{
    return 0;
}

/*
 * GuiSetKbdState：设置键盘状态（占位）
 */
void GuiSetKbdState(UINT16 state)
{

}
/*
 * SysCalcScrBufSize：计算屏幕区域字节数
 * - 参数：矩形区域；返回 `byteNum = 行数 * 每行字节块数`。
 * 注意：按 8 像素对齐列索引计算；越界返回0。
 */
void SysCalcScrBufSize(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT16* byteNum)
{
    if (x2 >= 160)
    {
        *byteNum = 0x00;
        return;
    }
    if (x2 < x1)
    {
        UINT8 x = x1;
        x1 = x2;
        x2 = x;
    }
    if (y2 >= 96)
    {
        *byteNum = 0x00;
        return;
    }
    if (y2 < y1)
    {
        UINT8 x = y1;
        y1 = y2;
        y2 = x;
    }
_7B16:
    y2 -= y1;
    y2++;
    x1 = (x1 & 0xF8) >> 3;
    x2 = (x2 & 0xF8) >> 3;
    x2 -= x1;
    x2++;
    *byteNum = x2 * y2;
}

// nTimeout单位10ms
/*
 * GuiMsgBox：消息框显示（可选超时）
 * - 参数：字符串、超时（单位10ms；0表示不自动关闭）。
 * - 行为：分行排版，保存区域→清屏→绘制文本与边框→等待键或超时→恢复区域→释放缓冲。
 * 返回：0xFF 空字符串；0xFE 缓冲分配失败；0xFD 返回主页指令；其它为正常结束。
 */
UINT8 GuiMsgBox(UINT8* strMsg, UINT16 nTimeout)
{
    //LOG("GuiMsgBox: %s\n", strMsg);
    UINT8 _17BA[20];
    UINT8 _17B9;
    UINT8 _17B8;
    UINT8 _17B7;
    UINT8* _17A9[7];
    UINT8* _17A7;
    UINT8 _17A6;
    UINT8 _17A5;
    UINT8 _17A4;
    UINT8 _17A3;
    UINT8 _17A2;
    UINT8* _17A0 = NULL;
    MsgType _179D;
    UINT16 _179B=0;
    UINT16 _1799;
    UINT8 _1798 = 0x01;
    UINT8 _1797=0;
    _17A4 = (UINT8)strlen((char*)strMsg);
    if (_17A4 == 0x00)
    {
    _81E9:
        return 0xFF;
    }
_81EE:
    _17A6 = 0x00;
    _17A7 = strMsg;
    _17A5 = 0x00;
    _17B7 = 0x00;
    _17A9[_17B7] = _17A7;
    while ((_17B7 < 0x05) && (_17A7[0]))
    {
    _8279:
        if (_17A7[0] > 0x80)
        {
        _8296:
            _17A5++;
        }
        else
        {
        _82A4:
            _17A5 = 0x00;
        }
    _82AA:
        _17A6++;
        if (_17A6 == 0x0010)
        {
        _82CF:
            _17B7++;
            if ((_17A5 % 0x02) == 0x00)
            {
            _82EA:
                _17A9[_17B7] = _17A7 + 0x0001;
                _17A6 = 0x00;
                _17A5 = 0x00;
            }
            else
            {
            _8366:
                _17A9[_17B7] = _17A7;
                _17A6 = 0x01;
                _17A5 = 0x01;
            }
        }
    _83B4:
        _17A7++;
    }
_83E2:
    if (_17B7 < 0x05)
    {
    _83EE:
        _17A9[_17B7+0x01] = _17A7;
    }
_8433:
    if (_17B7 >= 0x05)
    {
    _843F:
        _17B7--;
    }
_844A:
    _17B9 = (0x5A - (0x10 * (_17B7 + 0x01))) / 0x02;
    _17B8 = _17B9 + 0x04 + (0x10 * (_17B7 + 0x01));
_849A:
    SysCalcScrBufSize(0x0B, _17B9, 0x94, _17B8 + 0x02, &_179B);
    _17A0 = (UINT8*)SysMemAllocate(_179B);
    if (_17A0 == 0x0000)
    {
    _853C:
        return 0xFE;
    }
_8541:
    SysSaveScreen(0x0B, _17B9, 0x94, _17B8 + 0x02, _17A0);
_8592:
    SysLcdPartClear(0x0B, _17B9, 0x94, _17B8 + 0x02);
    for (_17A3 = 0x00; _17A3 <= _17B7; _17A3++)
    {
    _85F4:
        _17A2 = (UINT8)(_17A9[_17A3 + 0x0001] - _17A9[_17A3]);
        strncpy((char*)_17BA, (char*)_17A9[_17A3], _17A2);
        _17BA[_17A2] = 0x00;
        SysPrintString(0x0F, _17A3 * 0x10 + _17B9 + 0x02, _17BA);
    }
_8772:
    SysRect(0x0B, _17B9, 0x92, _17B8);
    SysFillRect(0x0E, _17B8, 0x92, _17B8 + 0x02);
    SysFillRect(0x92, _17B9 + 0x03, 0x94, _17B8 + 0x02);
    if (nTimeout)
    {
        if (timer != 0)
        {
            _1797 = 1;
        }
    _8874:
        SysTimer1Open(0x01);
        _1799 = 0x0000;
    }
_888A:
    while (0x01)
    {
    _8891:
        if (GuiGetMsg(&_179D))
        {
        _88C9:
            if (_179D.type == DICT_WM_KEY)
            {
                break;
            }
        _88D8:
            if (_179D.type == DICT_WM_COMMAND)
            {
            _88E4:
                if ((_179D.param & 0xFF) == CMD_RETURN_HOME)
                {
                _88F9:
                    GuiPushMsg(&_179D);
                    _1798 = 0xFD;
                    break;
                }
            }
        _892D:
            if (_179D.type == DICT_WM_TIMER)
            {
            _8939:
                if (nTimeout)
                {
                _8954:
                    _1799 += 0x0001;
                    if (_1799 == nTimeout)
                    {
                    _8995:
                        break;
                    }
                }
            }
        }
    }
_899B:
    if (nTimeout)
    {
    _89B6:
        SysTimer1Close();
        if (_1797 == 0x01)
        {
        _89CD:
            SysTimer1Open(_1797);
        }
    }
_89E8:
    SysRestoreScreen(0x0B, _17B9, 0x94, _17B8 + 0x02, _17A0);
    SysMemFree((char*)_17A0);
_8A62:
    return _1798;
}
/*
;功能    :文件创建函数
;U8 	FileCreat( U8 filetype,U32 filelength, U8 * information,U16 * filename,U8 * filehandle);
;入口    :U8 filetype,U32 filelength, U8 * information;
;出口    :U16 *filename,U8 *filehandle;
;堆栈使用:无
;全局变量:无
; 说明:
; 创建一个文件,分配空间,并打开该文件, 文件指针指向文件的第一的字节.
; U8	information[10],在同类型文件中是唯一的.
*/
/*
 * FileCreat：创建文件并打开
 * - 参数：`filetype` 类型号（用于扩展名），`filelength` 预分配长度，`information[10]` 文件唯一标识（名称部分），
 *   输出 `filename`（内部ID），`filehandle`（与filename相同）。
 * - 行为：在 `pGameData+3` 目录下创建 `<information>.<filetype>`；记录到 `fileinfos` 并返回句柄。
 * 注意：Windows专用；目录可自动创建；返回1成功/0失败。
 */
UINT8 FileCreat(UINT8 filetype, UINT32 filelength, UINT8* information, UINT16* filename, UINT8* filehandle)
{
    int i = 0;
    for (; i < FILE_NUM; i++)
    {
        if (fileinfos[i].filename == 0)
        {
            break;
        }
    }
    if (FILE_NUM == i)
    {
        return 0;
    }
    CHAR cFileName[32];
    sprintf(cFileName, "%s\\%s.%02X", pGameData + 3, information, filetype);
    if (!CreateDirectoryA(pGameData + 3, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return 0;
    }
    LOG("CreateFile: %s\n", cFileName);
    HANDLE hFile = CreateFileA(cFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        return 0;
    }
    SetFilePointer(hFile, filelength, NULL, FILE_BEGIN);
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    fileinfos[i].filename = i + 1;
    memcpy(fileinfos[i].information, information, 10);
    fileinfos[i].filetype = filetype;
    fileinfos[i].hFile = hFile;
    *filename = fileinfos[i].filename;
    *filehandle = fileinfos[i].filename;
    return 1;
}
/*
;功能    :打开一个文件
;U8  FileOpen(U16 filename, U8 filetype,U8 openmode,U8 * filehandle,U8 * filelength);
;入口    :U16 filename, U8 filetype,U8 openmode
;出口    :U8 * filehandle
;堆栈使用:无
;全局变量:无
; 说明:
; 打开一个文件.分配文件句柄filehandle.
; U8 openmode三种打开方式:
;	#define NoOpen		 0x00
;	#define	ReadOnly	 0x01
;	#define	ReadAndWrite	 0x02
; 支持同时打开四个文件; 文件指针指向文件的第一的字节.
*/
/*
 * FileOpen：按内部ID与类型打开文件
 * - 参数：`filename` 内部ID，`filetype` 类型号，`openmode` 打开方式（当前忽略）。
 * - 行为：构造路径，打开现有文件，返回 `filehandle` 与 `filelength`。
 * 注意：当前不支持只读限制；失败返回0。
 */
UINT8 FileOpen(UINT16 filename, UINT8 filetype, UINT8 openmode, UINT8* filehandle, UINT32* filelength)
{
    int i = 0;
    for (; i < FILE_NUM; i++)
    {
        if (fileinfos[i].filename == filename)
        {
            break;
        }
    }
    if (FILE_NUM == i)
    {
        return 0;
    }
    CHAR cFileName[32];
    sprintf(cFileName, "%s\\%s.%02X", pGameData + 3, fileinfos[i].information, filetype);
    LOG("CreateFile: %s\n", cFileName);
    HANDLE hFile = CreateFileA(cFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        return 0;
    }
    fileinfos[i].hFile = hFile;
    *filehandle = fileinfos[i].filename;
    *filelength = GetFileSize(hFile, NULL);
    return 1;
}
/*
;功能    :删除一个文件
;U8 filedel(U8 filehandle);
;入口    :无
;出口    :U8 filehandle
;堆栈使用:无
;全局变量:无
; 说明:
; 删除一个文件,并且关闭此文件.
*/
/*
 * FileDel：删除文件
 * - 参数：`filehandle` 句柄（内部ID）。
 * - 行为：关闭句柄、清理记录，并删除对应磁盘文件。
 */
UINT8 FileDel(UINT8 filehandle)
{
    int i = 0;
    for (; i < FILE_NUM; i++)
    {
        if (fileinfos[i].filename == filehandle)
        {
            break;
        }
    }
    if (FILE_NUM == i)
    {
        return 0;
    }
    LOG("CloseHandle: %s.%02X\n", fileinfos[i].information, fileinfos[i].filetype);
    CloseHandle(fileinfos[i].hFile);
    fileinfos[i].hFile = NULL;
    fileinfos[i].filename = 0;
    CHAR cFileName[32];
    sprintf(cFileName, "%s\\%s.%02X", pGameData + 3, fileinfos[i].information, fileinfos[i].filetype);
    LOG("DeleteFile: %s\n", cFileName);
    DeleteFileA(cFileName);
    return 0;
}
/*
;功能    :向一个文件写数据
;U8 	FileWrite(U8 filehandle,U8 datalength,U8 * bufadd);
;入口    :U8 filehandle,U8 datalength,U8 * bufadd
;出口    :无
;堆栈使用:无
;全局变量:无
; 说明:
; 写完后,文件指针指向说写数据的下一个字节
*/
/*
 * FileWrite：写入数据（顺序写）
 * - 参数：句柄、长度、缓冲指针；写入位置为当前文件指针处。
 * - 行为：调用 `WriteFile`，校验写入字节数；成功返回1。
 */
UINT8 FileWrite(UINT8 filehandle, UINT8 datalength, UINT8* bufadd)
{
    int i = 0;
    for (; i < FILE_NUM; i++)
    {
        if (fileinfos[i].filename == filehandle)
        {
            break;
        }
    }
    if (FILE_NUM == i)
    {
        return 0;
    }
    DWORD nNumberOfBytesWritten = 0;
    WriteFile(fileinfos[i].hFile, bufadd, datalength, &nNumberOfBytesWritten, NULL);
    if (datalength != nNumberOfBytesWritten)
    {
        return 0;
    }
    return 1;
}
/*
;功能    :关闭一个文件
;U8 fileclose(U8 filehandle);
;入口    :U8 filehandle
;出口    :无
;堆栈使用:无
;全局变量:无
; 说明:	关闭一个文件.支持同时打开四个文件.
*/
/*
 * FileClose：关闭文件
 */
UINT8 FileClose(UINT8 filehandle)
{
    int i = 0;
    for (; i < FILE_NUM; i++)
    {
        if (fileinfos[i].filename == filehandle)
        {
            break;
        }
    }
    if (FILE_NUM == i)
    {
        return 0;
    }
    LOG("CloseHandle: %s.%02X\n", fileinfos[i].information, fileinfos[i].filetype);
    CloseHandle(fileinfos[i].hFile);
    fileinfos[i].hFile = NULL;
    return 1;
}
/*
;功能    :读一个文件的数据
;U8 FileRead(U8 filehandle,U8 datalength,U8 * bufadd);
;入口    :U8 filehandle,U8 datalength,U8 * bufadd
;出口    :无
;堆栈使用:无
;全局变量:无
; 说明:
; 写完后,文件指针指向说读数据的下一个字节
*/
/*
 * FileRead：读取数据（顺序读）
 * - 行为：调用 `ReadFile`；成功返回1，否则0。
 */
UINT8 FileRead(UINT8 filehandle, UINT8 datalength, UINT8* bufadd)
{
    int i = 0;
    for (; i < FILE_NUM; i++)
    {
        if (fileinfos[i].filename == filehandle)
        {
            break;
        }
    }
    if (FILE_NUM == i)
    {
        return 0;
    }
    DWORD nNumberOfBytesRead = 0;
    ReadFile(fileinfos[i].hFile, bufadd, datalength, &nNumberOfBytesRead, NULL);
    if (datalength != nNumberOfBytesRead)
    {
        return 0;
    }
    return 1;
}
/*
;功能    :文件定位函数
;U8 FileSeek(U8 filehandle,U32 fileoffset,U8 origin);
;入口    :U8 filehandle,U32 fileoffset,U8 origin
;出口    :无
;堆栈使用:无
;全局变量:无
; 说明:
;      	origin:
;		#define FromTop		0x01
;		#define	FromCurrent	0x02
;		#define	FromEnd		0x03
;   	fileoffset:
; 		可以为负数.
*/
/*
 * FileSeek：文件指针定位
 * - 参数：`origin` 支持 FromTop/FromCurrent/FromEnd；`fileoffset` 可为负（经由强制类型转换）。
 */
UINT8 FileSeek(UINT8 filehandle, UINT32 fileoffset, UINT8 origin)
{
    int i = 0;
    for (; i < FILE_NUM; i++)
    {
        if (fileinfos[i].filename == filehandle)
        {
            break;
        }
    }
    if (FILE_NUM == i)
    {
        return 0;
    }
    DWORD dwMoveMethod = FILE_BEGIN;
    switch (origin)
    {
    case FromTop:
        dwMoveMethod = FILE_BEGIN;
        break;
    case FromCurrent:
        dwMoveMethod = FILE_CURRENT;
        break;
    case FromEnd:
        dwMoveMethod = FILE_END;
        break;
    }
    SetFilePointer(fileinfos[i].hFile, (INT32)fileoffset, NULL, dwMoveMethod);
    return 0;
}
/*
;功能    :初始化FLASH的一些变量
;void	FlashInit();
;入口    :无
;出口    :无
;堆栈使用:无
;全局变量:无
; 说明:
; 开机时调用.(进入个模块时也可以调用,可避免别的模块的干扰).
*/
/*
 * FlashInit：初始化文件索引
 * - 行为：清空 `fileinfos`；枚举 `pGameData+3` 目录下所有文件，解析名称与类型。
 * 注意：Windows专用；仅填充前 `FILE_NUM` 条。
 */
void FlashInit()
{
    for (int i = 0; i < FILE_NUM; i++)
    {
        fileinfos[i].filename = 0;
    }
    CHAR cFileName[32];
    WIN32_FIND_DATAA dFindFileData;
    sprintf(cFileName, "%s\\*", pGameData + 3);
    HANDLE hFile = FindFirstFileA(cFileName, &dFindFileData);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        return;
    }
    int i = 0;
    do {
        UINT8 filetype = 0;
        if (strcmp(dFindFileData.cFileName, ".") == 0 || strcmp(dFindFileData.cFileName, "..") == 0)
        {
            continue;
        }
        fileinfos[i].filename = i + 1;
        memset(fileinfos[i].information, 0, ARRAYSIZE(fileinfos[i].information));
        char* pDot = strrchr(dFindFileData.cFileName, '.');
        if (pDot == NULL)
        {
            strcpy(fileinfos[i].information, dFindFileData.cFileName);
        }
        else
        {
            memcpy(fileinfos[i].information, dFindFileData.cFileName, pDot - dFindFileData.cFileName);
            sscanf(pDot + 1, "%hhX", &filetype);
        }
        fileinfos[i].filetype = filetype;
        i++;
    } while (FindNextFileA(hFile, &dFindFileData));
}
/*
;功能    :返回某类型文件的数量.
;U8 	Filenum( U8 filetype,U16 * filenum);
;入口    :U8 filetype;
;出口    :U16 * filenum
;堆栈使用:无
;全局变量:无
; 说明:
; 返回某类型文件的数量.
*/
/*
 * FileNum：统计某类型文件数量
 */
UINT8 FileNum(UINT8 filetype, UINT16* filenum)
{
    *filenum = 0;
    for (int i = 0; i < FILE_NUM; i++)
    {
        if (fileinfos[i].filetype == filetype)
        {
            *filenum += 1;
        }
    }
    return *filenum>0?1:0;
}
/*
;功能    :浏览 某类型的第fileorder个文件的information.
;U8 	FileSearch(U8 filetype,U16 fileorder,U16 * filename,U8 * information);
;入口    :U8 fls_filetype,U16 fileorder;
;出口    :U16 * filename,U8 * information;
;堆栈使用:无
;全局变量:无
; 说明:
; 通过比较information[10],知道是否是要打开的文件.
*/
/*
 * FileSearch：查询某类型第 `fileorder` 个文件的信息
 * - 输出：内部ID `filename` 与 `information[10]`。
 */
UINT8 FileSearch(UINT8 filetype, UINT16 fileorder, UINT16* filename, UINT8* information)
{
    for (int i = 0; i < FILE_NUM; i++)
    {
        if (fileinfos[i].filetype == filetype && fileorder == 1)
        {
            *filename = fileinfos[i].filename;
            memcpy(information, fileinfos[i].information, 10);
            return 1;
        }
        fileorder--;
        if (fileorder == 0)
        {
            break;
        }
    }
    return 0;
}

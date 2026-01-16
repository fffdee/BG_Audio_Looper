/**
 * @file    upgrade.c
 * @brief   USB/SD卡/U盘 升级功能实现
 * @note    移植自BT_Audio_APP，适用于BG_card_mini项目
 */

#include <stdio.h>
#include <string.h>
#include <nds32_intrinsic.h>
#include "type.h"
#include "watchdog.h"
#include "irqn.h"
#include "remap.h"
#include "flash_boot.h"
#include "gpio.h"
#include "clk.h"
#include "debug.h"

#if FLASH_BOOT_EN

//======================================
// 升级错误码定义
//======================================
// 未检测到升级接口
#define NO_DEVICE_LINK          3
#define NO_UDISK_LINK           4
#define NO_SDCARD_LINK          5
#define NO_PC_LINK              6
#define NO_BT_LINK              7

// 文件系统错误
#define FS_OPEN_MVA_ERR         8
#define FS_SDCARD_ERR           9
#define FS_UDISK_ERR            10

// 升级数据合法性检查失败
#define MVA_HEADER_ERR          11
#define MVA_MAGIC_ERR           12
#define MVA_BOOT_LEN_ERR        13
#define MVA_ENCRYPTION_ERR      14
#define MVA_CODE_LEN_ERR        15
#define MVA_CONST_LEN_ERR       16
#define MVA_CONFIG_LEN_ERR      17
#define MVA_CONST_OFFSET_ERR    18
#define MVA_CONFIG_OFFEST_ERR   19

// 升级过程中数据传输/写入失败
#define PROCESS_BOOT_ERR        21
#define PROCESS_CODE_ERR        22
#define PROCESS_CONST_ERR       23
#define PROCESS_CONFIG_ERR      24
#define FLASH_UNLOCK_ERR        25
#define BT_INFO_ERR             26

//======================================
// Flash Boot标志寄存器定义
//======================================
#define ADR_FLASH_BOOT_FLAGE    (0x4000100C)

typedef struct _ST_FLASH_BOOT_FLAGE {
    volatile unsigned long UDisk        : 1;    /**< U盘升级使能 */
    volatile unsigned long PC           : 1;    /**< PC升级使能 */
    volatile unsigned long sdcard       : 2;    /**< SD卡升级使能 */
    volatile unsigned long bt           : 1;    /**< 蓝牙升级 */
    volatile unsigned long updata       : 1;    /**< 升级触发标志 */
    volatile unsigned long flag         : 2;    /**< 保留标志 */
    volatile unsigned long RSV          : 1;    /**< 保留 */
    volatile unsigned long ERROR_CODE   : 8;    /**< 错误码 */
    volatile unsigned long POR_CODE     : 8;    /**< 复位代码 */
} ST_FLASH_BOOT_FLAGE;

#define SREG_FLASH_BOOT_FLAGE   (*(volatile ST_FLASH_BOOT_FLAGE *)ADR_FLASH_BOOT_FLAGE)

// ROM函数声明
extern uint32_t ROM_UserRegisterGet(void);
extern void ROM_UserRegisterSet(uint32_t val);

// Cache操作函数声明
extern void DataCacheInvalidAll(void);
extern void ICacheInvalidAll(void);
extern void DisableIDCache(void);
extern void SysTickDeInit(void);
extern void SysTimerIntFlagClear(void);
extern void GPIO_RegisterResetMask(void);

/**
 * @brief 报告升级结果
 * @note  在系统启动时调用，打印上次升级的结果
 */
void report_up_grate(void)
{
    uint16_t err_code = 0;
    uint16_t clear_data = 0;
    
    // V2.1.4版本及以上的flashboot，升级结果标志通过ROM_UserRegisterGet获取
    err_code = ROM_UserRegisterGet();
    clear_data = ROM_UserRegisterGet();
    err_code = (err_code & 0x1f);  // 低5bit用于存储升级结果
    
    if(err_code != 0)
    {
        if(err_code == USER_CODE_RUN_START)
        {
            DBG("正常运行\n");
        }
        else if(err_code == UPDAT_OK)
        {
            DBG("升级成功!\n");
        }
        else if(err_code == NEEDLESS_UPDAT)
        {
            DBG("无需升级，版本相同\n");
        }
        else
        {
            DBG("升级失败! error code: %d\n", err_code);
        }
        
        // 清除升级标志
        clear_data = (clear_data & 0xffe0);
        ROM_UserRegisterSet(clear_data);
    }
    else
    {
        // 兼容旧版本bootloader
        if(SREG_FLASH_BOOT_FLAGE.ERROR_CODE == USER_CODE_RUN_START)
        {
            DBG("正常运行\n");
        }
        else if(SREG_FLASH_BOOT_FLAGE.ERROR_CODE == UPDAT_OK)
        {
            DBG("升级成功!\n");
        }
        else if(SREG_FLASH_BOOT_FLAGE.ERROR_CODE == NEEDLESS_UPDAT)
        {
            DBG("无需升级，版本相同\n");
        }
        else
        {
            DBG("升级失败! error code: %lu\n", (uint32_t)SREG_FLASH_BOOT_FLAGE.ERROR_CODE);
        }
    }
}

/**
 * @brief 获取升级错误码
 * @return 错误码
 */
uint8_t Report_Error_Code(void)
{
    return SREG_FLASH_BOOT_FLAGE.ERROR_CODE;
}

/**
 * @brief 清除错误码
 */
void Clear_Error_Code(void)
{
    SREG_FLASH_BOOT_FLAGE.ERROR_CODE = USER_CODE_RUN_START;
}

/**
 * @brief 获取Flash Boot复位标志
 * @return 复位标志
 */
uint8_t Reset_FlagGet_Flash_Boot(void)
{
    return (uint8_t)SREG_FLASH_BOOT_FLAGE.POR_CODE;
}

/**
 * @brief 启动固件升级
 * @param UpdateResource 升级资源类型
 *        - AppResourceCard: SD卡升级
 *        - AppResourceUDisk: U盘升级  
 *        - AppResourceUsbDevice: PC USB升级
 * @note  该函数会重启系统并跳转到bootloader执行升级
 */
void start_up_grate(uint32_t UpdateResource)
{
    int i;
    uint32_t temp = 0;
    typedef void (*fun)();
    fun jump_fun;

    // 清除升级标志
    *(uint32_t *)ADR_FLASH_BOOT_FLAGE = 0;
    
    // 根据升级资源类型设置标志
    if(UpdateResource == AppResourceCard)
    {
        // SD卡升级
        SREG_FLASH_BOOT_FLAGE.updata = 1;
        #if CFG_RES_CARD_GPIO == 1
        SREG_FLASH_BOOT_FLAGE.sdcard = 1;  // A15A16A17
        #else
        SREG_FLASH_BOOT_FLAGE.sdcard = 2;  // A20A21A22
        #endif
        DBG("SD Card Upgrade Start...\n");
    }
    else if(UpdateResource == AppResourceUDisk)
    {
        // U盘升级
        SREG_FLASH_BOOT_FLAGE.updata = 1;
        SREG_FLASH_BOOT_FLAGE.UDisk = 1;
        DBG("U-Disk Upgrade Start...\n");
    }
    else if(UpdateResource == AppResourceUsbDevice)
    {
        // PC USB升级
        SREG_FLASH_BOOT_FLAGE.PC = 1;
        SREG_FLASH_BOOT_FLAGE.updata = 1;
        DBG("PC USB Upgrade Start...\n");
    }

    // 如果设置了升级标志，则跳转到bootloader
    if(SREG_FLASH_BOOT_FLAGE.updata)
    {
        temp = *(uint32_t *)ADR_FLASH_BOOT_FLAGE;
        DBG("Jumping to Flash Boot...\n");
        
        jump_fun = (fun)0;
        
        // 启用看门狗，防止升级过程中死机
        WDG_Enable(WDG_STEP_1S);
        
        // 关闭全局中断
        GIE_DISABLE();
        
        // 关闭并清空Cache
        DisableIDCache();
        DataCacheInvalidAll();
        ICacheInvalidAll();
        
        // 停止系统定时器
        SysTickDeInit();
        SysTimerIntFlagClear();
        
        // 复位DMA
        *(uint32_t *)0x4000D100 = 0;
        for(i = 0x4000D000; i < 0x4000D104;)
        {
            *(uint32_t *)i = 0;
            i = i + 4;
        }

        // 复位外设寄存器 (保留Flash和AUPLL)
        *(uint32_t *)0x40022000 &= ~0x77FFF8;
        *(uint32_t *)0x40022000 |= 0x7FFFF8;

        // 复位功能模块 (保留Flash)
        *(uint32_t *)0x40022004 &= ~0x7FFFF7FF;
        *(uint32_t *)0x40022004 |= 0x7FFFF7FF;

        // 恢复升级标志
        *(uint32_t *)ADR_FLASH_BOOT_FLAGE = temp;
        
        // 清除中断使能
        __nds32__mtsr(0, NDS32_SR_INT_MASK2);
        __nds32__mtsr(__nds32__mfsr(NDS32_SR_HSP_CTL) & 0, NDS32_SR_HSP_CTL);
        __asm("NOP");
        
        // 重置GPIO寄存器
        GPIO_RegisterResetMask();
        
        // 跳转到地址0执行bootloader
        jump_fun();
        
        while(1);  // 不应该执行到这里
    }
}

#endif /* FLASH_BOOT_EN */

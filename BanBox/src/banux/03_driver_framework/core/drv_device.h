/**
 *****************************************************************************
 * @file     drv_device.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    驱动设备注册框架 - 类Linux驱动模型
 *****************************************************************************
 * @attention
 *
 * 本模块实现驱动设备的统一注册管理：
 * 1. 驱动抽象层：定义标准驱动接口（init/open/close/read/write/ioctl）
 * 2. 设备注册：将驱动注册到设备文件系统
 * 3. 参数自动注册：根据参数定义自动创建参数节点
 * 4. 总线类型分类：SPI/I2C/I2S/SDIO
 *
 * 使用示例：
 *   // 1. 定义设备参数
 *   static const FsParamDef_t st7735_params[] = {
 *       FS_PARAM_DEF("name",   "驱动名称", get_name, NULL),
 *       FS_PARAM_DEF("width",  "LCD宽度",  get_width, set_width),
 *       FS_PARAM_DEF("height", "LCD高度",  get_height, set_height),
 *       FS_PARAM_END()
 *   };
 *
 *   // 2. 定义驱动结构
 *   static const DrvDevice_t st7735_drv = {
 *       .name = "st7735",
 *       .bus = DRV_BUS_SPI,
 *       .init = st7735_drv_init,
 *       .params = st7735_params,
 *   };
 *
 *   // 3. 注册驱动
 *   DrvDevice_Register(&st7735_drv);
 *
 *****************************************************************************
 */

#ifndef __DRV_DEVICE_H__
#define __DRV_DEVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"
#include "drv_fs.h"

/*******************************************************************************
 * 总线类型定义
 ******************************************************************************/
typedef enum {
    DRV_BUS_SPI = 0,        /* SPI总线 */
    DRV_BUS_I2C,            /* I2C总线 */
    DRV_BUS_I2S,            /* I2S总线 */
    DRV_BUS_SDIO,           /* SDIO总线 */
    DRV_BUS_GPIO,           /* GPIO直接控制 */
    DRV_BUS_UART,           /* UART总线 */
    DRV_BUS_POWER,          /* 电源管理总线 */
    DRV_BUS_USB,            /* USB总线 */
    DRV_BUS_MAX
} DrvBusType_t;

/*******************************************************************************
 * 驱动操作接口类型定义
 ******************************************************************************/
/**
 * @brief  驱动初始化
 * @param  priv: 设备私有数据
 * @return 0成功，其他失败
 */
typedef int (*DrvInit_t)(void *priv);

/**
 * @brief  驱动去初始化
 * @param  priv: 设备私有数据
 * @return 0成功
 */
typedef int (*DrvDeinit_t)(void *priv);

/**
 * @brief  打开设备
 * @param  priv: 设备私有数据
 * @return 0成功
 */
typedef int (*DrvOpen_t)(void *priv);

/**
 * @brief  关闭设备
 * @param  priv: 设备私有数据
 * @return 0成功
 */
typedef int (*DrvClose_t)(void *priv);

/**
 * @brief  读取设备数据
 * @param  priv: 设备私有数据
 * @param  buf: 数据缓冲区
 * @param  len: 长度
 * @return 实际读取长度，-1错误
 */
typedef int (*DrvRead_t)(void *priv, uint8_t *buf, uint32_t len);

/**
 * @brief  写入设备数据
 * @param  priv: 设备私有数据
 * @param  buf: 数据缓冲区
 * @param  len: 长度
 * @return 实际写入长度，-1错误
 */
typedef int (*DrvWrite_t)(void *priv, const uint8_t *buf, uint32_t len);

/**
 * @brief  设备控制
 * @param  priv: 设备私有数据
 * @param  cmd: 控制命令
 * @param  arg: 参数
 * @return 0成功，其他失败
 */
typedef int (*DrvIoctl_t)(void *priv, uint32_t cmd, void *arg);

/*******************************************************************************
 * 驱动设备结构
 ******************************************************************************/
typedef struct DrvDevice {
    /* 基本信息 */
    const char         *name;           /* 设备名称 */
    const char         *desc;           /* 设备描述 */
    DrvBusType_t        bus;            /* 总线类型 */
    
    /* 驱动操作接口 */
    DrvInit_t           init;           /* 初始化函数 */
    DrvDeinit_t         deinit;         /* 去初始化函数 */
    DrvOpen_t           open;           /* 打开设备 */
    DrvClose_t          close;          /* 关闭设备 */
    DrvRead_t           read;           /* 读取数据 */
    DrvWrite_t          write;          /* 写入数据 */
    DrvIoctl_t          ioctl;          /* 设备控制 */
    
    /* 参数定义列表 */
    const FsParamDef_t *params;         /* 参数数组（NULL结尾） */
    
    /* 私有数据 */
    void               *privData;       /* 设备私有数据 */
    
    /* 运行时状态（由系统管理） */
    FsNode_t           *fsNode;         /* 文件系统节点 */
    bool                isRegistered;   /* 是否已注册 */
    bool                isOpened;       /* 是否已打开 */
} DrvDevice_t;

/*******************************************************************************
 * 驱动注册信息（内部使用）
 ******************************************************************************/
#define DRV_DEVICE_MAX      16          /* 最大注册设备数 */

/*******************************************************************************
 * 公共API
 ******************************************************************************/

/**
 * @brief  初始化驱动管理系统
 * @return 0成功
 * @note   会自动调用 DrvFs_Init()
 */
int DrvDevice_Init(void);

/**
 * @brief  注册驱动设备
 * @param  dev: 驱动设备结构指针
 * @return 0成功，其他失败
 * @note   会自动在对应总线目录下创建设备节点和参数节点
 */
int DrvDevice_Register(DrvDevice_t *dev);

/**
 * @brief  注销驱动设备
 * @param  dev: 驱动设备结构指针
 * @return 0成功
 */
int DrvDevice_Unregister(DrvDevice_t *dev);

/**
 * @brief  根据名称查找设备
 * @param  name: 设备名称
 * @return 设备指针，NULL未找到
 */
DrvDevice_t* DrvDevice_Find(const char *name);

/**
 * @brief  根据路径查找设备
 * @param  path: 设备路径（如 "/driver/spi/st7735"）
 * @return 设备指针，NULL未找到
 */
DrvDevice_t* DrvDevice_FindByPath(const char *path);

/**
 * @brief  获取总线类型对应的目录节点
 * @param  bus: 总线类型
 * @return 目录节点指针
 */
FsNode_t* DrvDevice_GetBusDir(DrvBusType_t bus);

/**
 * @brief  获取总线类型名称
 * @param  bus: 总线类型
 * @return 名称字符串
 */
const char* DrvDevice_GetBusName(DrvBusType_t bus);

/**
 * @brief  列出所有已注册的设备
 * @param  callback: 回调函数
 * @param  userData: 用户数据
 */
typedef void (*DrvDeviceListCallback_t)(DrvDevice_t *dev, void *userData);
void DrvDevice_List(DrvDeviceListCallback_t callback, void *userData);

/**
 * @brief  获取已注册设备数量
 * @return 设备数量
 */
int DrvDevice_GetCount(void);
/**
 * @brief  获取设备列表
 * @param  count: 输出设备数量
 * @return 设备指针数组
 */
DrvDevice_t** DrvDevice_GetList(int *count);
/*******************************************************************************
 * 便捷宏定义
 ******************************************************************************/

/* 定义设备驱动 */
#define DRV_DEVICE_DEF(n, d, b, i) \
    { \
        .name = n, \
        .desc = d, \
        .bus = b, \
        .init = i, \
        .deinit = NULL, \
        .open = NULL, \
        .close = NULL, \
        .read = NULL, \
        .write = NULL, \
        .ioctl = NULL, \
        .params = NULL, \
        .privData = NULL, \
        .fsNode = NULL, \
        .isRegistered = FALSE, \
        .isOpened = FALSE \
    }

/* 简化参数定义 */
#define DRV_PARAM_RO(n, d, g)       FS_PARAM_DEF(n, d, g, NULL)
#define DRV_PARAM_RW(n, d, g, s)    FS_PARAM_DEF(n, d, g, s)

#ifdef __cplusplus
}
#endif

#endif /* __DRV_DEVICE_H__ */

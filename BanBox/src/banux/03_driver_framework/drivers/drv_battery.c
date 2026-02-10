#include "drv_battery.h"
#include "drv_device.h"
#include "drv_fs.h"
#include "battery_drv.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/**
 * @brief Battery management driver private data
 */
typedef struct {
    uint8_t  soc;           // Battery percentage State of Charge (0~100)
    float    voltage;       // Real-time voltage (V)
    bool     initialized;   // Initialization flag
    uint32_t last_update;   // Last update timestamp (for cache optimization)
} BatteryPrivData_t;

// Global private data
static BatteryPrivData_t g_battery_priv = {
    .soc = 0,
    .voltage = 0.0f,
    .initialized = false,
    .last_update = 0,
};

/*****************************************************************************
 * Parameter read callback functions
 *****************************************************************************/

/**
 * @brief Get device name
 */
static int param_get_name(char *buf, uint16_t maxLen, void *userData)
{
    snprintf(buf, maxLen, "Battery_Manager");
    return strlen(buf);
}

/**
 * @brief Get battery percentage
 */
static int param_get_soc(char *buf, uint16_t maxLen, void *userData)
{
    BatteryPrivData_t *priv = (BatteryPrivData_t *)userData;
    
    // Read latest battery level
    priv->soc = battery_get_soc();
    
    snprintf(buf, maxLen, "%u", priv->soc);
    return strlen(buf);
}

/**
 * @brief Get real-time voltage
 */
static int param_get_voltage(char *buf, uint16_t maxLen, void *userData)
{
    BatteryPrivData_t *priv = (BatteryPrivData_t *)userData;
    
    // Read latest voltage
    priv->voltage = battery_get_volt();
    
    // Convert float to integer representation (millivolts)
    int millivolts = (int)(priv->voltage * 1000);
    int volts = millivolts / 1000;
    int decimal = (millivolts % 1000) / 10;  // Keep two decimal places
    
    snprintf(buf, maxLen, "%d.%02d", volts, decimal);
    return strlen(buf);
}

/**
 * @brief Get battery status
 * @note Returns status based on SOC: normal(>20%), low(10~20%), critical(<10%)
 */
static int param_get_status(char *buf, uint16_t maxLen, void *userData)
{
    BatteryPrivData_t *priv = (BatteryPrivData_t *)userData;
    
    // Read latest battery level
    priv->soc = battery_get_soc();
    
    const char *status;
    if (priv->soc > 20) {
        status = "normal";
    } else if (priv->soc >= 10) {
        status = "low";
    } else {
        status = "critical";
    }
    
    snprintf(buf, maxLen, "%s", status);
    return strlen(buf);
}

/**
 * @brief Get full charge voltage
 */
static int param_get_full_volt(char *buf, uint16_t maxLen, void *userData)
{
    // Convert float to integer representation
    int millivolts = (int)(FULL_VOLT * 1000);
    int volts = millivolts / 1000;
    int decimal = (millivolts % 1000) / 100;  // Keep one decimal place
    
    snprintf(buf, maxLen, "%d.%01d", volts, decimal);
    return strlen(buf);
}

/**
 * @brief Get empty battery voltage
 */
static int param_get_empty_volt(char *buf, uint16_t maxLen, void *userData)
{
    // Convert float to integer representation
    int millivolts = (int)(EMPTY_VOLT * 1000);
    int volts = millivolts / 1000;
    int decimal = (millivolts % 1000) / 100;  // Keep one decimal place
    
    snprintf(buf, maxLen, "%d.%01d", volts, decimal);
    return strlen(buf);
}

/*****************************************************************************
 * Parameter write callback functions
 *****************************************************************************/

/**
 * @brief Refresh battery data (triggered by writing any value)
 */
static int param_cmd_refresh(const char *value, void *userData)
{
    BatteryPrivData_t *priv = (BatteryPrivData_t *)userData;
    
    // Force refresh battery level and voltage
    priv->soc = battery_get_soc();
    priv->voltage = battery_get_volt();
    
    return 0;  // Success
}

/*****************************************************************************
 * Parameter definition table
 *****************************************************************************/
static const FsParamDef_t battery_params[] = {
    {
        .name = "name",
        .desc = "Device name",
        .get = param_get_name,
        .set = NULL,  // 只读
    },
    {
        .name = "soc",
        .desc = "Battery percentage (0~100)",
        .get = param_get_soc,
        .set = NULL,  // 只读
    },
    {
        .name = "voltage",
        .desc = "Real-time voltage (V)",
        .get = param_get_voltage,
        .set = NULL,  // 只读
    },
    {
        .name = "status",
        .desc = "Battery status (normal/low/critical)",
        .get = param_get_status,
        .set = NULL,  // 只读
    },
    {
        .name = "full_volt",
        .desc = "Full charge voltage (V)",
        .get = param_get_full_volt,
        .set = NULL,  // 只读
    },
    {
        .name = "empty_volt",
        .desc = "Empty battery voltage (V)",
        .get = param_get_empty_volt,
        .set = NULL,  // 只读
    },
    {
        .name = "refresh",
        .desc = "Refresh battery data (write any value)",
        .get = NULL,  // 只写
        .set = param_cmd_refresh,
    },
    FS_PARAM_END // 结束标记
};

/*****************************************************************************
 * Driver operation functions
 *****************************************************************************/

/**
 * @brief Battery management driver initialization
 */
static int battery_drv_init(void *priv)
{
    BatteryPrivData_t *battery_priv = (BatteryPrivData_t *)priv;
    
    if (battery_priv->initialized) {
        return 0;  // Already initialized
    }
    
    // Delay reading battery level to avoid issues with calling Shell_Printf in main()
    // First read will occur on first parameter access
    battery_priv->soc = 0;
    battery_priv->voltage = 0.0f;
    battery_priv->initialized = true;
    
    return 0;
}

/**
 * @brief Battery management driver deinitialization
 */
static int battery_drv_deinit(void *priv)
{
    BatteryPrivData_t *battery_priv = (BatteryPrivData_t *)priv;
    battery_priv->initialized = false;
    return 0;
}

/**
 * @brief Open battery device (optional implementation)
 */
static int battery_drv_open(void *priv)
{
    // 电池管理无需特殊打开操作
    return 0;
}

/**
 * @brief Close battery device (optional implementation)
 */
static int battery_drv_close(void *priv)
{
    // 电池管理无需特殊关闭操作
    return 0;
}

/**
 * @brief Read battery data
 * @param priv Private data
 * @param buf Read buffer
 * @param len Read length
 * @return Actual number of bytes read
 * @note Returns formatted battery information string
 */
static int battery_drv_read(void *priv, uint8_t *buf, uint32_t len)
{
    BatteryPrivData_t *battery_priv = (BatteryPrivData_t *)priv;
    
    // Refresh data
    battery_priv->soc = battery_get_soc();
    battery_priv->voltage = battery_get_volt();
    
    // Format output
    int written = snprintf((char *)buf, len,
        "Battery Info:\n"
        "  SOC: %u%%\n"
        "  Voltage: %.2fV\n"
        "  Status: %s\n",
        battery_priv->soc,
        battery_priv->voltage,
        (battery_priv->soc > 20) ? "Normal" :
        (battery_priv->soc >= 10) ? "Low" : "Critical"
    );
    
    return (written > 0) ? written : 0;
}

/**
 * @brief IOCTL control command
 * @param priv Private data
 * @param cmd Command code
 * @param arg Argument
 * @return 0 on success, negative on failure
 * 
 * Supported commands:
 *   0x01 - Refresh battery data
 *   0x02 - Get SOC to uint8_t* pointed by arg
 *   0x03 - Get voltage to float* pointed by arg
 */
static int battery_drv_ioctl(void *priv, uint32_t cmd, void *arg)
{
    BatteryPrivData_t *battery_priv = (BatteryPrivData_t *)priv;
    
    switch (cmd) {
        case 0x01:  // 刷新电量数据
            battery_priv->soc = battery_get_soc();
            battery_priv->voltage = battery_get_volt();
            return 0;
            
        case 0x02:  // 获取SOC
            if (arg != NULL) {
                *(uint8_t *)arg = battery_get_soc();
                return 0;
            }
            return -1;
            
        case 0x03:  // 获取电压
            if (arg != NULL) {
                *(float *)arg = battery_get_volt();
                return 0;
            }
            return -1;
            
        default:
            return -1;  // 不支持的命令
    }
}

/*****************************************************************************
 * 驱动设备结构定义
 *****************************************************************************/
/* 注意：不能用const，因为需要在运行时修改isRegistered/fsNode等字�?*/
static DrvDevice_t battery_driver = {
    .name = "battery",
    .bus = DRV_BUS_POWER,  // 电源总线
    .init = battery_drv_init,
    .deinit = battery_drv_deinit,
    .open = battery_drv_open,
    .close = battery_drv_close,
    .read = battery_drv_read,
    .write = NULL,  // 电池不支持写�?    .ioctl = battery_drv_ioctl,
    .params = battery_params,
    .privData = &g_battery_priv,
};

/*****************************************************************************
 * 对外注册接口
 *****************************************************************************/

/**
 * @brief 注册电池管理驱动到驱动框�? * @return 0成功, 负值失�? * 
 * 注册后创建以下文件系统节�?
 *   /driver/power/battery/
 *   ├── name
 *   ├── soc
 *   ├── voltage
 *   ├── status
 *   ├── full_volt
 *   ├── empty_volt
 *   └── refresh
 */
int Battery_DrvRegister(void)
{
    return DrvDevice_Register(&battery_driver);
}

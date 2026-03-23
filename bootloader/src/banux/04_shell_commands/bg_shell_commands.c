/**
 *****************************************************************************
 * @file     bg_shell_commands.c
 * @author   BG Card Team
 * @version  V2.0.0
 * @date     16-December-2025
 * @brief    Shell command module implementation
 *****************************************************************************
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "bg_shell.h"

#include "shell_io_cdc.h"
#include "shell_io_ble.h"
#include "shell_io_manager.h"
#include "BG_FlashMgr.h"
#include "flash_bus.h"

#include "gpio.h"
#include "adc.h"
#include "dac.h"
#include "bt_a2dp_api.h"

#include "drv_init.h"  /* 椹卞姩妗嗘灦鍒濆鍖�*/
#include "vfs.h"       /* 铏氭嫙鏂囦欢绯荤粺API */
#include "drv_fs.h"    /* 椹卞姩鏂囦欢绯荤粺API */
#include "drv_device.h" /* 椹卞姩璁惧绠＄悊 */
#include "chip_info.h"  /* 鑺墖ID璇诲彇 */
#include "FreeRTOS.h"
#include "task.h"




/*============================================================================
 * ble module - BLE controller
 *===========================================================================*/

static int ble_get_status(int argc, char *argv[])
{
    (void)argc; (void)argv;
    /* TODO: 浠嶣LE绠＄悊鍣ㄨ幏鍙栧疄闄呯姸鎬�*/
    Shell_Print("BLE Status: Idle\r\n");
    return 0;
}

static const ShellOpt_t ble_opts[] = {
    OPT("s", "state",    NULL,      "Show BLE status",    ble_get_status),
    OPT_END()
};

DEFINE_MODULE(ble, "BLE controller", MOD_CAT_HARDWARE, ble_opts);

/*============================================================================
 * File system navigation commands (ls, pwd, cd, cat)
 *===========================================================================*/



static int cmd_ble_send(int argc, char *argv[])
{
    if (argc < 1) {
        Shell_Print("ble_send: missing string operand\r\n");
        Shell_Print("Usage: ble_send <string>\r\n");
        return -1;
    }

    // 检查BLE连接状态 (使用att_server_can_send作为连接指示)
    extern int att_server_can_send(void);
    if (att_server_can_send() == 0) {
        Shell_Print("BLE not connected or CCCD not enabled\r\n");
        return -1;
    }

    // 合并所有参数为字符串
    char send_str[256] = "";
    int i;
    for (i = 0; i < argc; i++) {
        if (i > 0) strcat(send_str, " ");
        strncat(send_str, argv[i], sizeof(send_str) - strlen(send_str) - 1);
    }

    // 发送到BLE
    BLE_Send((uint8_t *)send_str, strlen(send_str));

    Shell_Printf("Sent to BLE: %s\r\n", send_str);
    return 0;
}

static const ShellOpt_t ble_send_opts[] = {
    OPT("", "", "<string>", "Send string via BLE when connected", cmd_ble_send),
    OPT_END()
};

DEFINE_MODULE(ble_send, "Send string via BLE", MOD_CAT_SYSTEM, ble_send_opts);

/*============================================================================
 * Module registration
 *===========================================================================*/
    /* 鍙傛暟淇濆瓨鍛戒护 */
    extern void ShellCmd_Param_Init(void);
    /* 音源管理命令 */
    extern int ShellCmdSoundbank_Register(void);
void Shell_RegisterAllModules(void)
{

    REGISTER_MODULE(ble);
    REGISTER_MODULE(ble_send);

    /* Soundbank management command (BANGTSYNTH_EN) */
    ShellCmdSoundbank_Register();



}

/**
 *****************************************************************************
 * @file     shell_cmd_hwtest.c
 * @brief    硬件测试Shell命令
 *****************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bg_shell.h"
#include "product_def.h"
#include "FreeRTOS.h"
#include "task.h"

#if HW_DRV_SSD1306_EN

#include "ssd1306.h"
#include "rotary_encoder.h"
#include "battery_drv.h"
#include "adc.h"
#include "gpio.h"

static int hwtest_start(int argc, char *argv[])
{
    (void)argc; (void)argv;

    Shell_Print("Starting hardware test...\r\n");
    Shell_Print("Long-press encoder button to exit\r\n");

    extern void HW_Test_Run(void);
    HW_Test_Run();

    Shell_Print("HW test finished\r\n");
    return 0;
}

static const ShellOpt_t hwtest_opts[] = {
    OPT("s", "start",  NULL,  "Start hardware test (long-press encoder to exit)",  hwtest_start),
    OPT_END()
};

DEFINE_MODULE(hwtest, "Hardware test", MOD_CAT_DEBUG, hwtest_opts);

void ShellCmdHwtest_Register(void)
{
    REGISTER_MODULE(hwtest);
}

#endif /* HW_DRV_SSD1306_EN */

/**
 * battery_calib.c - Battery discharge curve calibration
 *
 * Records time per 0.1V voltage band from full charge to hardware shutdown.
 * Saves data after every step crossing so partial data survives a manual
 * power-off. On the next boot the saved curve is used for accurate SOC.
 */

#include "battery_calib.h"
#include "battery_drv.h"
#include "ble_protocol.h"
#include "bg_low_power.h"
#include "spi_flash.h"
#include "debug.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdbool.h>

/* BLE 连接状态检测: 居与 CCCD 状态共地 */
extern int att_server_can_send(void);

/* ---- Default discharge curve ----
 * Typical LiPo at moderate standby drain (~10 h total = 36000 s).
 * Used until user performs a calibration run.
 * Units: seconds per 0.1 V band, step 0 = 4.2→4.1 V, step 1 = 4.1→4.0 V… */
static const uint32_t c_def_dur_s[BATT_CALIB_MAX_STEPS] = {
    3600u,  /* 4.2 -> 4.1 V */
    3600u,  /* 4.1 -> 4.0 V */
    3600u,  /* 4.0 -> 3.9 V */
    3600u,  /* 3.9 -> 3.8 V */
    9000u,  /* 3.8 -> 3.7 V  (flat plateau region) */
    5400u,  /* 3.7 -> 3.6 V */
    2520u,  /* 3.6 -> 3.5 V */
    1800u,  /* 3.5 -> 3.4 V */
    1080u,  /* 3.4 -> 3.3 V */
     720u,  /* 3.3 -> 3.2 V */
     720u,  /* 3.2 -> 3.1 V */
     360u,  /* 3.1 -> 3.0 V */
       0u, 0u, 0u, 0u, 0u, 0u   /* below 3.0 V — hardware auto-shuts down */
};

/* ---- Module state ---- */
static BattCalibData_t g_calib;         /* active curve (flash or defaults)  */
static uint8_t         g_state;         /* 0 = IDLE, 1 = RUNNING             */
static uint8_t         g_cur_step;      /* voltage step we're currently in   */
static uint32_t        g_step_start_tk; /* tick when we entered g_cur_step   */
static volatile uint8_t g_notify_pending; /* deferred BLE notify: ATT not ready at call site */

/* ---- Internal helpers ---- */

static uint16_t calib_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    uint8_t  j;
    for (i = 0u; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (j = 0u; j < 8u; j++) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* CRC covers all fields except magic (first 2 B) and crc (last 2 B). */
static uint16_t calib_compute_crc(const BattCalibData_t *d)
{
    return calib_crc16(
        (const uint8_t *)d + 2u,
        (uint16_t)(sizeof(BattCalibData_t) - 4u));
}

static void calib_rebuild_total(void)
{
    uint8_t  i;
    uint32_t total = 0u;
    for (i = 0u; i < g_calib.valid_steps && i < BATT_CALIB_MAX_STEPS; i++) {
        total += g_calib.step_duration_s[i];
    }
    g_calib.total_duration_s = total;
}

static void calib_load_defaults(void)
{
    uint8_t i;
    memset(&g_calib, 0, sizeof(g_calib));
    g_calib.magic   = BATT_CALIB_MAGIC;
    g_calib.version = BATT_CALIB_VERSION;
    for (i = 0u; i < BATT_CALIB_MAX_STEPS; i++) {
        g_calib.step_duration_s[i] = c_def_dur_s[i];
        if (c_def_dur_s[i] > 0u) {
            g_calib.valid_steps = (uint8_t)(i + 1u);
        }
    }
    calib_rebuild_total();
}

static void calib_save(void)
{
    g_calib.crc = calib_compute_crc(&g_calib);
    SpiFlashIOCtrl(IOCTL_FLASH_UNPROTECT, "\x35\xBA\x69", 3);
    SpiFlashErase(SECTOR_ERASE, BATT_CALIB_FLASH_SECTOR, 1);
    SpiFlashWrite(BATT_CALIB_FLASH_ADDR,
                  (uint8_t *)&g_calib,
                  sizeof(BattCalibData_t),
                  100);
    DBG("[BattCalib] Saved: steps=%d total=%lus\n",
        (int)g_calib.valid_steps, (unsigned long)g_calib.total_duration_s);
}

/* Notify Android app of current calibration state + full curve via BLE.
 *
 * Payload layout (matches BluetoothHelper.java parser):
 *   [0]    sub          = CALIB_CMD_STATUS_RSP (0x83)
 *   [1]    state        = 0 (idle) | 1 (running)
 *   [2]    num_points   = number of (time_s, mv) pairs that follow
 *   [3]    calib_step   = current step index (meaningful only when state==1)
 *   [4-5]  calib_mv     = current measured voltage in mV, little-endian
 *   [6..]  num_points × { time_s_lo(1B), time_s_hi(1B), mv_lo(1B), mv_hi(1B) }
 *
 * Curve point i represents the top boundary of voltage step band i:
 *   voltage     = BATT_CALIB_V_TOP_MV - i * BATT_CALIB_V_STEP_MV
 *   time_s      = cumulative standby time from full charge to this voltage
 */
static void calib_notify_ble(void)
{
    /* 6 header bytes + up to (BATT_CALIB_MAX_STEPS+1) points × 4 bytes each.
     * static: avoids stack pressure; only one notification in-flight at a time. */
    static uint8_t payload[6u + (BATT_CALIB_MAX_STEPS + 1u) * 4u];
    uint16_t mv;
    uint8_t  num_points;
    uint32_t consumed;
    uint32_t total;
    uint8_t  i;
    uint8_t  max_steps;
    uint16_t pt_mv;
    uint16_t pt_time_s;
    uint8_t  idx;

    /* 若 ATT 尚未就绪（通常是在 write handler 内部被调用），记录 pending
     * 标志，由 BattCalib_Tick() 在下一个 50ms 轮询周期补发。 */
    if (!att_server_can_send()) {
        g_notify_pending = 1u;
        return;
    }
    g_notify_pending = 0u;

    mv    = battery_get_volt_mv();
    total = g_calib.total_duration_s;

    /* Build curve point list from time-based discharge table.
     * Generate valid_steps+1 boundary points: top of each 0.1V band. */
    num_points = 0u;
    consumed   = 0u;
    if (total > 0u) {
        max_steps = (g_calib.valid_steps < (uint8_t)BATT_CALIB_MAX_STEPS)
                  ? g_calib.valid_steps
                  : (uint8_t)BATT_CALIB_MAX_STEPS;
        for (i = 0u; i <= max_steps; i++) {
            pt_mv  = (uint16_t)((uint32_t)BATT_CALIB_V_TOP_MV
                                - (uint32_t)i * (uint32_t)BATT_CALIB_V_STEP_MV);
            /* 累积时间（秒），最大 65535s 约 18h */
            pt_time_s = (consumed > 65535u) ? 65535u : (uint16_t)consumed;
            idx    = (uint8_t)(6u + num_points * 4u);

            payload[idx + 0u] = (uint8_t)(pt_time_s & 0xFFu);
            payload[idx + 1u] = (uint8_t)(pt_time_s >> 8u);
            payload[idx + 2u] = (uint8_t)(pt_mv & 0xFFu);
            payload[idx + 3u] = (uint8_t)(pt_mv >> 8u);
            num_points++;

            /* Accumulate consumed time AFTER storing the point for band i,
             * so the next point reflects the time after this band drains. */
            if (i < max_steps) {
                consumed += g_calib.step_duration_s[i];
            }
        }
    }

    payload[0u] = (uint8_t)CALIB_CMD_STATUS_RSP;
    payload[1u] = g_state;
    payload[2u] = num_points;
    payload[3u] = g_cur_step;
    payload[4u] = (uint8_t)(mv & 0xFFu);
    payload[5u] = (uint8_t)(mv >> 8u);

    BleProto_SendOnce(BLE_CMD_BATTERY_CALIB, payload,
                      (uint8_t)(6u + num_points * 4u));
}

/* ---- Public API ---- */

void BattCalib_Init(void)
{
    BattCalibData_t tmp;
    uint16_t        crc_calc;
    int             ret;

    g_state           = 0u;
    g_cur_step        = 0u;
    g_step_start_tk   = 0u;

    memset(&tmp, 0, sizeof(tmp));
    ret = SpiFlashRead(BATT_CALIB_FLASH_ADDR,
                       (uint8_t *)&tmp,
                       sizeof(BattCalibData_t),
                       100);

    if (ret == 0
        && tmp.magic   == BATT_CALIB_MAGIC
        && tmp.version == BATT_CALIB_VERSION) {
        crc_calc = calib_crc16(
            (const uint8_t *)&tmp + 2u,
            (uint16_t)(sizeof(BattCalibData_t) - 4u));
        if (crc_calc == tmp.crc) {
            memcpy(&g_calib, &tmp, sizeof(g_calib));
            DBG("[BattCalib] Loaded from flash: steps=%d total=%lus\n",
                (int)g_calib.valid_steps,
                (unsigned long)g_calib.total_duration_s);
            return;
        }
        DBG("[BattCalib] CRC mismatch, using defaults\n");
    } else {
        DBG("[BattCalib] No valid flash data, using defaults\n");
    }
    calib_load_defaults();
}

void BattCalib_Start(void)
{
    uint16_t current_mv;
    uint8_t  init_step;

    if (g_state == 1u) {
        DBG("[BattCalib] Already running\n");
        return;
    }

    current_mv = battery_get_volt_mv();

    init_step = 0u;
    if (current_mv < (uint16_t)BATT_CALIB_V_TOP_MV) {
        init_step = (uint8_t)(
            ((uint16_t)BATT_CALIB_V_TOP_MV - current_mv) / BATT_CALIB_V_STEP_MV);
        if (init_step >= (uint8_t)BATT_CALIB_MAX_STEPS) {
            init_step = (uint8_t)(BATT_CALIB_MAX_STEPS - 1u);
        }
    }

    g_state         = 1u;
    g_cur_step      = init_step;
    g_step_start_tk = xTaskGetTickCount();

    DBG("[BattCalib] Started at step %d (%umV)\n", (int)init_step, (unsigned)current_mv);
    calib_notify_ble();
}

void BattCalib_Stop(void)
{
    if (g_state == 0u) return;
    g_state = 0u;
    DBG("[BattCalib] Stopped\n");
    calib_notify_ble();
}

void BattCalib_Tick(void)
{
    static uint32_t s_last_tick = 0u;
    uint32_t now_tk;
    uint16_t current_mv;
    uint8_t  new_step;
    uint32_t elapsed_tk;
    uint32_t elapsed_s;
    uint8_t  step_count;
    uint8_t  crossed;

    /* 补发被 ATT write handler 延迟的通知（无论校准是否正在进行） */
    if (g_notify_pending && att_server_can_send()) {
        calib_notify_ble();
    }

    if (g_state != 1u) return;

    /* Suppress low-power mode while calibrating */
    LowPower_FeedActivity(LP_ACT_BATT_CALIB);

    /* Check voltage every ~60 seconds (60000 ticks at 1 ms/tick default) */
    now_tk = xTaskGetTickCount();
    if ((now_tk - s_last_tick) < 60000u) return;
    s_last_tick = now_tk;

    current_mv = battery_get_volt_mv();

    if (current_mv >= (uint16_t)BATT_CALIB_V_TOP_MV) {
        /* Still at full charge – reset step anchor */
        g_cur_step      = 0u;
        g_step_start_tk = now_tk;
        return;
    }

    new_step = (uint8_t)(
        ((uint32_t)BATT_CALIB_V_TOP_MV - current_mv) / BATT_CALIB_V_STEP_MV);
    if (new_step >= (uint8_t)BATT_CALIB_MAX_STEPS) {
        new_step = (uint8_t)(BATT_CALIB_MAX_STEPS - 1u);
    }

    if (new_step <= g_cur_step) return; /* No step boundary crossed */

    /* Calculate elapsed time for the crossed bands */
    elapsed_tk = now_tk - g_step_start_tk;
    elapsed_s  = elapsed_tk / (uint32_t)configTICK_RATE_HZ;
    step_count = (uint8_t)(new_step - g_cur_step);

    /* Distribute time equally across simultaneously-crossed steps.
     * Normally step_count == 1; > 1 only if device was held at intermediate
     * voltage without Tick running (e.g. paused debugger). */
    for (crossed = g_cur_step;
         crossed < new_step && crossed < (uint8_t)BATT_CALIB_MAX_STEPS;
         crossed++) {
        g_calib.step_duration_s[crossed] =
            (step_count > 0u) ? (elapsed_s / (uint32_t)step_count) : elapsed_s;
        if (g_calib.valid_steps <= crossed) {
            g_calib.valid_steps = (uint8_t)(crossed + 1u);
        }
    }

    calib_rebuild_total();

    DBG("[BattCalib] Step %d -> %d completed (%lus elapsed), volt=%umV\n",
        (int)g_cur_step, (int)new_step,
        (unsigned long)elapsed_s, (unsigned)current_mv);

    g_cur_step      = new_step;
    g_step_start_tk = now_tk;

    /* Persist immediately so data survives a manual power-off */
    calib_save();
    calib_notify_ble();
}

uint8_t BattCalib_IsRunning(void)
{
    return g_state;
}

uint8_t BattCalib_GetSOC(void)
{
    uint16_t current_mv;
    uint32_t step;
    uint32_t step_top_mv;
    uint32_t frac_mv;
    uint32_t consumed;
    uint32_t total;
    uint32_t i;

    /* Fall back to simple voltage lookup if no curve data is loaded */
    if (g_calib.total_duration_s == 0u) {
        return battery_get_soc();
    }

    current_mv = battery_get_volt_mv();

    if (current_mv >= (uint16_t)BATT_CALIB_V_TOP_MV) return 100u;

    step = ((uint32_t)BATT_CALIB_V_TOP_MV - (uint32_t)current_mv) / BATT_CALIB_V_STEP_MV;

    /* Below the lowest calibrated step — battery essentially empty */
    if (step >= (uint32_t)g_calib.valid_steps) {
        return 1u;
    }
    if (step >= BATT_CALIB_MAX_STEPS) {
        step = BATT_CALIB_MAX_STEPS - 1u;
    }

    /* Time already consumed in fully-completed steps above current */
    consumed = 0u;
    for (i = 0u; i < step; i++) {
        consumed += g_calib.step_duration_s[i];
    }

    /* Fraction consumed within current step — pure integer math.
     * frac_mv = millivolts into this step (0..V_STEP_MV).
     * consumed += step_duration * frac_mv / V_STEP_MV */
    step_top_mv = (uint32_t)BATT_CALIB_V_TOP_MV - step * (uint32_t)BATT_CALIB_V_STEP_MV;
    if (current_mv < (uint16_t)step_top_mv) {
        frac_mv = step_top_mv - (uint32_t)current_mv;
        if (frac_mv > (uint32_t)BATT_CALIB_V_STEP_MV) {
            frac_mv = (uint32_t)BATT_CALIB_V_STEP_MV;
        }
        consumed += g_calib.step_duration_s[step] * frac_mv / (uint32_t)BATT_CALIB_V_STEP_MV;
    }

    total = g_calib.total_duration_s;
    if (consumed >= total) return 0u;

    return (uint8_t)((total - consumed) * 100u / total);
}

void BattCalib_HandleBleCmd(const uint8_t *payload, uint8_t len)
{
    if (len < 1u) return;

    switch (payload[0]) {
    case CALIB_CMD_START:
        DBG("[BattCalib] BLE cmd: START\n");
        BattCalib_Start();
        break;

    case CALIB_CMD_STOP:
        DBG("[BattCalib] BLE cmd: STOP\n");
        BattCalib_Stop();
        break;

    case CALIB_CMD_STATUS:
        DBG("[BattCalib] BLE cmd: STATUS\n");
        calib_notify_ble();
        break;

    case CALIB_CMD_CLEAR:
        DBG("[BattCalib] BLE cmd: CLEAR\n");
        BattCalib_ClearData();
        break;

    default:
        break;
    }
}

void BattCalib_ClearData(void)
{
    g_state = 0u;
    calib_load_defaults();
    calib_save();
    DBG("[BattCalib] Cleared, reverted to defaults\n");
}

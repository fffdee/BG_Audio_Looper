/**
 * battery_calib.h - Battery discharge curve calibration module
 *
 * Records the real time each 0.1V voltage band takes to discharge during
 * standby. The resulting curve replaces the default linear voltage→SOC
 * mapping with an accurate time-based SOC estimate.
 *
 * Workflow:
 *   1. BattCalib_Init()  - called once on boot; loads saved curve from flash
 *   2. BattCalib_Start() - triggered via BLE/USB command; disables LP mode
 *   3. BattCalib_Tick()  - called periodically (hardware_check, ~50ms);
 *                          checks voltage and writes incremental step data
 *   4. BattCalib_GetSOC()- returns accurate SOC using the loaded curve
 *
 * Flash storage: one dedicated 4KB sector at BATT_CALIB_FLASH_ADDR.
 * Each completed voltage step is saved immediately so partial data survives
 * a manual power-off mid-calibration.
 *
 * BLE command: BLE_CMD_BATTERY_CALIB (0x33), sub-commands defined below.
 */

#ifndef __BATTERY_CALIB_H__
#define __BATTERY_CALIB_H__

#include <stdint.h>

/* ---- Flash storage (one sector, adjacent to sys_param region) ---- */
/* sys_param lives at 0x150000 (sector 336); calib is in the next sector.    */
/* Both are above the firmware binary end (~0x13C000) and below audio data   */
/* (CONST_DATA_ADDR = 0x198000).  Update together with SYS_PARAM_SECTOR_NUM. */
#define BATT_CALIB_FLASH_ADDR    0x151000u                        /* 0x151000 */
#define BATT_CALIB_FLASH_SECTOR  (0x151000u / 4096u)              /* 337     */

/* ---- Calibration curve configuration ---- */
#define BATT_CALIB_MAGIC         0xBA77u
#define BATT_CALIB_VERSION       1u
#define BATT_CALIB_V_TOP_MV      4200u   /* 4.20V - fully charged             */
#define BATT_CALIB_V_STEP_MV     100u    /* 0.10V per step                    */
#define BATT_CALIB_MAX_STEPS     18u     /* covers 4.2V down to 2.4V          */

/* ---- BLE sub-command bytes for BLE_CMD_BATTERY_CALIB ---- */
/* App → MCU */
#define CALIB_CMD_START          0x01u   /* Start new calibration run         */
#define CALIB_CMD_STOP           0x02u   /* Stop / abort calibration          */
#define CALIB_CMD_STATUS         0x03u   /* Query current state (MCU replies) */
#define CALIB_CMD_CLEAR          0x04u   /* Clear saved data, revert defaults */
/* MCU → App */
#define CALIB_CMD_STATUS_RSP     0x83u   /* Status notification / reply       */

/**
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
 * @brief Register a low-power activity feed callback (called by 06_app layer)
 * @param feed_fn  Function pointer to LowPower_FeedActivity or equivalent.
 *                 Pass NULL to disable.
 * @note  Decouples 02_device_drivers from 06_app dependency.
 */
void BattCalib_RegisterFeedActivity(void (*feed_fn)(uint8_t mask));

/**
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
 * Persisted calibration record (stored in single flash sector).
 *
 * step_duration_s[n] = seconds spent in the voltage band:
 *   band n covers  V_TOP - n*V_STEP  ..  V_TOP - (n+1)*V_STEP
 *   e.g. step 0: 4.20V – 4.10V, step 1: 4.10V – 4.00V, …
 *
 * Unrecorded steps remain 0 (default curve fills them before first save).
 * valid_steps tracks the highest step index that has any data.
 */
typedef struct {
    uint16_t magic;                                  /* BATT_CALIB_MAGIC       */
    uint8_t  version;                                /* BATT_CALIB_VERSION     */
    uint8_t  valid_steps;                            /* steps with data (0-18) */
    uint16_t v_shutdown_mv;                          /* actual HW shutdown mV  */
    uint16_t reserved;
    uint32_t step_duration_s[BATT_CALIB_MAX_STEPS]; /* seconds per 0.1V band  */
    uint32_t total_duration_s;                       /* cached sum of steps    */
    uint16_t crc;                                    /* CRC16 of all above     */
} BattCalibData_t;

/* ---- Public API ---- */

/**
 * Load saved calibration data from flash (or fall back to built-in defaults).
 * Must be called once on boot before any other function.
 */
void    BattCalib_Init(void);

/**
 * Start a calibration run.
 * - Determines current voltage step from measured voltage
 * - Enables periodic voltage tracking in BattCalib_Tick()
 * - Keeps already-recorded steps for higher voltage bands (partial safe)
 * - Suppresses low-power mode while running
 */
void    BattCalib_Start(void);

/**
 * Stop calibration run without saving the in-progress step.
 * Already-written steps remain in flash.
 */
void    BattCalib_Stop(void);

/**
 * Periodic maintenance tick (~50ms call rate from hardware_check).
 * - Feeds LP activity mask to prevent sleep while calibrating
 * - Checks voltage every 60 seconds and records completed steps
 * - Saves to flash after each step boundary crossing
 */
void    BattCalib_Tick(void);

/**
 * Returns 1 if calibration is currently running, 0 otherwise.
 */
uint8_t BattCalib_IsRunning(void);

/**
 * Returns battery SOC (0-100) using the calibration curve.
 * Falls back to raw voltage-based SOC if no calibration data available.
 */
uint8_t BattCalib_GetSOC(void);

/**
 * Handle incoming BLE_CMD_BATTERY_CALIB frame from Android app.
 * Dispatches START / STOP / STATUS / CLEAR based on payload[0].
 */
void    BattCalib_HandleBleCmd(const uint8_t *payload, uint8_t len);

/**
 * Erase saved calibration data and reload built-in default curve.
 */
void    BattCalib_ClearData(void);

#endif /* __BATTERY_CALIB_H__ */

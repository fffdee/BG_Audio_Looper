#ifndef DRV_BATTERY_H
#define DRV_BATTERY_H

#include <stdint.h>

/**
 * @file drv_battery.h
 * @brief Battery management driver framework adaptation layer
 * 
 * Adapt the underlying battery driver (battery_drv.c) to the driver framework, providing unified access interface
 * 
 * File system path after registration:
 *   /driver/power/battery/
 *   ├── name           (read-only) - Device name "Battery_Manager"
 *   ├── soc            (read-only) - Battery percentage 0~100
 *   ├── voltage        (read-only) - Real-time voltage(V)
 *   ├── status         (read-only) - Working status "normal"/"low"/"critical"
 *   ├── full_volt      (read-only) - Full charge voltage 4.2V
 *   ├── empty_volt     (read-only) - Empty voltage 3.0V
 *   └── refresh        (write-only) - Force refresh battery level (write any value to trigger)
 * 
 * Shell command examples:
 *   cat /driver/power/battery/soc        # View battery percentage
 *   cat /driver/power/battery/voltage    # View real-time voltage
 *   cat /driver/power/battery/status     # View battery status
 *   echo 1 > /driver/power/battery/refresh  # Refresh battery data
 */

/**
 * @brief Register battery management driver to driver framework
 * @return 0 success, negative value failure
 */
int Battery_DrvRegister(void);

#endif /* DRV_BATTERY_H */

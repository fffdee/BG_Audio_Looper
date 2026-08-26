/**
 * @file usb_identity.h
 * @brief BanDataHub APP USB 身份（与 bootloader 区分）
 *
 * Bootloader: VID=0x0001 PID=0x4247（upgrade 工具识别）
 * APP:       VID/PID=0x1234/0x1234（CDC + 模拟 U 盘）
 */
#ifndef USB_IDENTITY_H
#define USB_IDENTITY_H

#define APP_USB_VID     0x1234u
#define APP_USB_PID     0x1234u

#endif

/**
 * @file    usb_identity.h
 * @brief   Bootloader USB VID/PID 身份（与 host_tool/update_tool 协议一致）
 *
 * 协议约定：
 *   PID = 0x4247 ('BG')  — BG Bootloader 家族
 *   VID = 产品编号：
 *         0x0001 BanBox
 *         0x0002 BanAirBundy
 *
 * 本仓库产品为 BanBox → VID=0x0001 / PID=0x4247
 */

#ifndef USB_IDENTITY_H
#define USB_IDENTITY_H

#define BG_USB_PID              0x4247u
#define BG_USB_VID_BANBOX       0x0001u
#define BG_USB_VID_BANAIRBUNDY  0x0002u

/* BanBox Bootloader 枚举身份 */
#define BOOTLOADER_USB_VID      BG_USB_VID_BANBOX
#define BOOTLOADER_USB_PID      BG_USB_PID

#endif /* USB_IDENTITY_H */

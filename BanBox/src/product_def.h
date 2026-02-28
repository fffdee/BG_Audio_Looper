#ifndef PRODUCT_DEF_H
#define PRODUCT_DEF_H

// product_def.h
// Author: [Your Name]
// Created: [Date]
// Description: Product definitions and macros for BanBox project.

#ifdef __cplusplus
extern "C" {
#endif

// Add your definitions, enums, structs, and function prototypes here.

#define BANBOX_1_0


#ifdef BANBOX_1_0

#define LINEIN_EN
#define MIC_EN
#define LINE1_INPUT_DETECT_EN
#define LINE2_INPUT_DETECT_EN
#define MIC_INPUT_DETECT_EN
#define VFS_EN
#define EFFECT_GRAPHICS_EN
#define USB_EN

/* BanGTsynth MIDI 合成器模块 (注释掉此行可移除合成器功能) */
#define BANGTSYNTH_EN

#endif // BANBOX_1_0

#ifdef BANBOX_1_1

#define SOFT_POWER_MGR_EN
#define LINE1_EN
#define LINE2_EN
#define MIC_EN
#define LINE1_INPUT_DETECT_EN
#define LINE2_INPUT_DETECT_EN
#define MIC_INPUT_DETECT_EN
#define VFS_EN
#define EFFECT_GRAPHICS_EN
#define USB_EN

#endif // BANBOX_1_1    





#ifdef __cplusplus
}
#endif

#endif // PRODUCT_DEF_H

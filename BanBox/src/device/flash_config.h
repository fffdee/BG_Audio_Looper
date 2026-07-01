#ifndef __FLASH_CONFIG_H__
#define __FLASH_CONFIG_H__

/* APP firmware starts at 0x000000 (no bootloader mode) */
#define	CODE_ADDR				0x000000
#define CONST_DATA_ADDR    		0x198000

#ifdef CFG_CHIP_BP1048P4
#define AUDIO_EFFECT_ADDR  		(0x1C8000 + 0x200000)
#define FLASHFS_ADDR			(0x1D0000 + 0x200000)

#define USER_DATA_ADDR       (0x1F0000 + 0x200000)
#define BP_DATA_ADDR         (0x1F3000 + 0x200000)
#define BT_DATA_ADDR         (0x1FB000 + 0x200000)

#define USER_CONFIG_ADDR     (0x1FE000 + 0x200000)
#define BT_CONFIG_ADDR       (0x1FF000 + 0x200000)
#else
#define AUDIO_EFFECT_ADDR  		0x1C8000
#define FLASHFS_ADDR			0x1D0000

#define USER_DATA_ADDR     		0x1F0000
#define BP_DATA_ADDR     		0x1F3000
#define BT_DATA_ADDR     		0x1FB000

#define USER_CONFIG_ADDR		0x1FE000
#define BT_CONFIG_ADDR			0x1FF000
#endif

#define	 CFG_SDK_VER_CHIPID			(0xB1)
#define  CFG_SDK_MAJOR_VERSION		(0)
#define  CFG_SDK_MINOR_VERSION		(1)
#define  CFG_SDK_PATCH_VERSION	    (12)

#endif

#ifndef __BG_FLASH_MANAGER_H__
#define __BG_FLASH_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"

#define DEV_NOR 	0
#define DEV_NAND 	1

#define FLASH_CS_INIT()        GPIO_RegOneBitClear(GPIO_A_IE, GPIOA21);\
		                       GPIO_RegOneBitSet(GPIO_A_OE, GPIOA21);\
		                       GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA21);

#define NAND_CS_INIT()         GPIO_RegOneBitClear(GPIO_A_IE, GPIOA22);\
		                       GPIO_RegOneBitSet(GPIO_A_OE, GPIOA22);\
		                       GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA22);

#define FLASH_CS_ENABLE()     GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA21);
#define FLASH_CS_DISABLE()    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA21);

#define NAND_CS_ENABLE()     GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA22);
#define NAND_CS_DISABLE()    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA22);


//#define FLASH_HOLD_INIT()      GPIO_RegOneBitClear(GPIO_A_IE, GPIOA22);\
//		                       GPIO_RegOneBitSet(GPIO_A_OE, GPIOA22);\
//		                       GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA22);
//
//#define FLASH_HOLD_ENABLE()     GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA22);
//#define FLASH_HOLD_DISABLE()    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA22);


#define FLASH_WP_INIT()      GPIO_RegOneBitClear(GPIO_A_IE, GPIOA17);\
		                       GPIO_RegOneBitSet(GPIO_A_OE, GPIOA17);\
		                       GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA17);

#define FLASH_WP_ENABLE()     GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA17);
#define FLASH_WP_DISABLE()    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA17);

#define FLASH_STATUS_OK 0

#define SECTOR_SIZE (4 * 1024*8) // 每个扇区的大小是32KBit

#define DEVICE_ID_64MBIT  0x17
#define DEVICE_ID_128MBIT 0x16
#define DEVICE_ID_256MBIT 0x14
#define DEVICE_ID_512MBIT 0x13
#define DEVICE_ID_1GBIT   0x12

#define PAGE_SIZE 256
// FLASH命令定义
#define FLASH_CMD_JEDEC_ID 	   0x9F
#define FLASH_CMD_WRITE_ENABLE     0x06
#define FLASH_CMD_WRITE_DISABLE    0x04
#define FLASH_CMD_READ_STATUS_REG  0x05
#define FLASH_CMD_WRITE_STATUS_REG 0x01
#define FLASH_CMD_READ_DATA        0x03
#define FLASH_CMD_FAST_READ        0x0B
#define FLASH_CMD_PAGE_PROGRAM     0x02
#define FLASH_CMD_SECTOR_ERASE     0xD8
#define FLASH_CMD_BLOCK_ERASE_32K  0x52
#define FLASH_CMD_BLOCK_ERASE_64K  0xD8
#define FLASH_CMD_CHIP_ERASE       0xC7
#define FLASH_CMD_POWER_DOWN       0xB9
#define FLASH_CMD_RELEASE_POWER_DOWN 0xAB
#define FLASH_CMD_JEDEC_ID         0x9F
#define NAND_CMD_JEDEC_ID         0x90

typedef struct{

	void (*Init)(void);
	uint8_t (*PageProgram)(uint32_t,uint8_t*,uint16_t,uint8_t);
	void (*WriteEnable)(uint8_t,uint8_t);
	void (*SectorErase)(uint32_t,uint8_t);
	void (*ReadData)(uint32_t,uint8_t*,uint16_t,uint8_t);
	void (*ReadID)(uint8_t*, uint8_t* , uint8_t*,uint8_t);
	uint32_t (*GetRemainingCapacity)(uint8_t);
	uint32_t (*GetTotalByte)(uint8_t);
	void (*EraseAll)(uint8_t);

}BG_Flash_Manager;

extern BG_Flash_Manager BG_flash_manager;


#endif

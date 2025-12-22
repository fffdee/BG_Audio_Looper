#include "bg_flash_manager.h"
#include "spim.h"
#include "spi_flash.h"
#include "debug.h"
#include "spim_interface.h"
#include "dma.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// Bad block management related definitions
#define BAD_BLOCK_MARKER 0x00  // Bad block marker, normal block is 0xFF
#define BAD_BLOCK_TABLE_MAGIC 0x42424242  // Bad block table magic "BBBB"
#define BAD_BLOCK_TABLE_VERSION 0x0101    // Bad block table version

// Bad block table structure
typedef struct {
    uint32_t magic;               // Magic value, used to verify bad block table
    uint16_t version;             // Version number
    uint16_t count;               // Number of bad blocks
    uint32_t bad_blocks[128];     // Bad block address list, supports up to 128 bad blocks
    uint32_t reserved[4];         // Reserved space
} BadBlockTable;

// Bad block manager status
typedef struct {
    BadBlockTable table;          // Bad block table
    uint32_t table_address;       // Storage address of bad block table
    bool initialized;             // Whether bad block manager is initialized
} BadBlockManager;

// Internal function declarations
void flash_init(void);
void flash_ReadID(uint8_t* manufacturerID, uint8_t* memoryType, uint8_t* deviceID, uint8_t dev);
void flash_WriteEnable(uint8_t enable,uint8_t dev);
uint8_t flash_ReadStatusReg(uint8_t dev);
void flash_WriteStatusReg(uint8_t data,uint8_t dev);
void flash_WaitForWriteEnd(uint8_t dev);
void flash_SectorErase(uint32_t sectorAddress,uint8_t dev);
uint8_t flash_PageProgram(uint32_t address, uint8_t* data, uint16_t size,uint8_t dev);
void flash_ReadData(uint32_t address, uint8_t* data, uint16_t size,uint8_t dev);
uint32_t flash_GetRemainingCapacity(uint8_t dev);
uint32_t flash_GetTotalByte(uint8_t dev);
void flash_EraseAll(uint8_t dev);
void flash_write_byte(uint8_t data);
uint8_t flash_read_byte(void);
void flash_write(uint8_t* data,uint16_t size);
void flash_read(uint8_t* data,uint16_t size);

// Bad block management functions
static void bad_block_manager_init(uint8_t dev);
static bool is_block_bad(uint32_t block_address, uint8_t dev);
static bool mark_block_as_bad(uint32_t block_address, uint8_t dev);
static uint32_t find_next_good_block(uint32_t start_block, uint8_t dev);
static void save_bad_block_table(uint8_t dev);
static void load_bad_block_table(uint8_t dev);
static uint32_t address_to_block(uint32_t address, uint8_t dev);
static uint32_t block_to_address(uint32_t block, uint8_t dev);

// Debug function for NAND audio flush
static uint8_t nand_audio_flush_buffer(uint8_t dev);

// Flash manager instance
BG_Flash_Manager BG_flash_manager = {
	.Init = flash_init,
	.PageProgram = flash_PageProgram,
	.SectorErase = flash_SectorErase,
	.WriteEnable = flash_WriteEnable,
	.ReadData = flash_ReadData,
	.ReadID = flash_ReadID,
	.GetRemainingCapacity = flash_GetRemainingCapacity,
	.GetTotalByte = flash_GetTotalByte,
	.EraseAll = flash_EraseAll,
};

// Bad block manager instance
static BadBlockManager bad_block_manager = {0};

void flash_init(void)
{
	// Initialize NOR Flash CS pins
	FLASH_CS_INIT();   // NOR1 (GPIOA21)
	NAND_CS_INIT();    // NOR2 (GPIOA22)
	FLASH_WP_INIT();
	FLASH_CS_DISABLE();
	NAND_CS_DISABLE();
	FLASH_WP_DISABLE();

	DBG("Dual NOR Flash initialized (CS=A21, A22)\n");
}

// Function to read manufacturer, memory type, and device ID
void flash_ReadID(uint8_t* manufacturerID, uint8_t* memoryType, uint8_t* deviceID, uint8_t dev) {
  // For NOR Flash, use JEDEC ID command to read ID
  if(dev==DEV_NOR1){
	FLASH_CS_ENABLE();
	flash_write_byte(FLASH_CMD_JEDEC_ID);

	*manufacturerID = flash_read_byte();
	*memoryType = flash_read_byte();
	*deviceID = flash_read_byte();

	FLASH_CS_DISABLE();
  }
  else if(dev==DEV_NOR2){
	// For second NOR Flash, also use JEDEC ID command to read ID
	NAND_CS_ENABLE();  // Enable A22 chip select
	flash_write_byte(FLASH_CMD_JEDEC_ID);

	*manufacturerID = flash_read_byte();
	*memoryType = flash_read_byte();
	*deviceID = flash_read_byte();

	NAND_CS_DISABLE();
  }
}


uint32_t flash_GetTotalByte(uint8_t dev) {
    uint32_t capacity = 0;
    uint8_t manufacturerID, memoryType, deviceID;
    flash_ReadID(&manufacturerID, &memoryType, &deviceID,dev);

    // Log the read Flash ID for debugging
    DBG("%s Flash ID: Manufacturer=0x%02X, MemoryType=0x%02X, DeviceID=0x%02X\n",
        dev == DEV_NOR1 ? "NOR1" : "NOR2", manufacturerID, memoryType, deviceID);

    // For NOR Flash, map device ID to capacity
    switch (deviceID) {
            case DEVICE_ID_64MBIT:
                capacity = 64 * 1024 * 1024; // 64 Mbit
                break;
            case DEVICE_ID_128MBIT:
                capacity = 128 * 1024 * 1024; // 128 Mbit
                break;
            case DEVICE_ID_256MBIT:
                capacity = 256 * 1024 * 1024; // 256 Mbit
                break;
            case DEVICE_ID_512MBIT:
                capacity = 512 * 1024 * 1024; // 512 Mbit
                break;
            case DEVICE_ID_1GBIT:
                capacity = 1024 * 1024 * 1024; // 1 Gbit
                break;
            case DEVICE_ID_2GBIT:
                capacity = 2048 * 1024 * 1024; // 2 Gbit (W25N02)
                break;
            case DEVICE_ID_W25N02:  // 0xAA is also a valid device ID
                capacity = 256 * 1024 * 1024; // W25N02 256MB
                break;
            default:
                // For unknown device IDs, assume W25N02 for Winbond NAND Flash
                if (dev == DEV_NAND && manufacturerID == 0xEF) {
                    DBG("Unknown Winbond NAND device ID: 0x%02X, assuming W25N02\n", deviceID);
                    capacity = 256 * 1024 * 1024; // Default to W25N02 capacity
                } else {
                    DBG("Unknown device ID: 0x%02X\n", deviceID);
                    capacity = 0;
                }
                break;
        }


        // Capacity returned is in bytes, divide by 8 for byte-addressable
        return capacity/8;

}

uint32_t Windbond_GetCapacity(uint8_t deviceID,uint8_t dev) {
    uint32_t capacity = 0;

    switch (deviceID) {
        case DEVICE_ID_64MBIT:
            capacity = 64 * 1024 * 1024; // 64 Mbit
            break;
        case DEVICE_ID_128MBIT:
            capacity = 128 * 1024 * 1024; // 128 Mbit
            break;
        case DEVICE_ID_256MBIT:
            capacity = 256 * 1024 * 1024; // 256 Mbit
            break;
        case DEVICE_ID_512MBIT:
            capacity = 512 * 1024 * 1024; // 512 Mbit
            break;
        case DEVICE_ID_1GBIT:
            capacity = 1024 * 1024 * 1024; // 1 Gbit
            break;
        case DEVICE_ID_2GBIT:
            capacity = 2048 * 1024 * 1024; // 2 Gbit (W25N02)
            break;
        default:
            capacity = 0;
            break;
    }

    return capacity;
}

// Check if a sector is erased (all bytes = 0xFF)
bool flash_IsSectorErased(uint32_t sectorAddress,uint8_t dev) {
    uint8_t data;
    // Read the first byte of the sector
    flash_ReadData(sectorAddress, &data, 1,dev);
    // If it's 0xFF, we consider the sector erased
    return data == 0xFF;
}

uint32_t flash_GetRemainingCapacity(uint8_t dev) {
	uint32_t remainingCapacity = 0;
    uint32_t sectorAddress = 0;
    uint32_t i;
    uint8_t manufacturerID, memoryType, deviceID;
    flash_ReadID(&manufacturerID, &memoryType, &deviceID,dev);
    for (i = 0; i < Windbond_GetCapacity(deviceID,dev)/SECTOR_SIZE ; ++i) {
        // For NOR Flash, check each sector if it's erased

        if (flash_IsSectorErased(sectorAddress,dev)) {
            remainingCapacity += 1 ;
        }
        // Move to the next sector
        sectorAddress += SECTOR_SIZE;
    }
    DBG("Total is:%d KByte,Remain is:%d KByte\n",(Windbond_GetCapacity(deviceID,dev)/SECTOR_SIZE)*4,remainingCapacity*4);
    return remainingCapacity;
}

void flash_WriteEnable(uint8_t enable,uint8_t dev) {
	if(dev==DEV_NOR1){
		FLASH_CS_ENABLE();
		if(enable){
			flash_write_byte(FLASH_CMD_WRITE_ENABLE);
		}else{
			flash_write_byte(FLASH_CMD_WRITE_DISABLE);
		}
		FLASH_CS_DISABLE();
	}
	else if(dev==DEV_NOR2){
		// Second NOR Flash write enable/disable
		NAND_CS_ENABLE();
		if(enable){
			flash_write_byte(FLASH_CMD_WRITE_ENABLE);
		}else{
			flash_write_byte(FLASH_CMD_WRITE_DISABLE);
		}
		NAND_CS_DISABLE();
	}


}

// Read status register
uint8_t flash_ReadStatusReg(uint8_t dev) {
    uint8_t data;
    if(dev==DEV_NOR1){
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_READ_STATUS_REG);
		data = flash_read_byte();
		FLASH_CS_DISABLE();
    }
    else if(dev==DEV_NOR2){
		// Second NOR Flash status register read
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_READ_STATUS_REG);
		data = flash_read_byte();
		NAND_CS_DISABLE();
    }
    return data;
}

// Write status register
void flash_WriteStatusReg(uint8_t data,uint8_t dev) {
	if(dev==DEV_NOR1){
		flash_WriteEnable(1,dev);
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_WRITE_STATUS_REG);
		flash_write_byte(data);
		FLASH_CS_DISABLE();
		flash_WaitForWriteEnd(dev);
	}
	else if(dev==DEV_NOR2){
		// Second NOR Flash status register write
		flash_WriteEnable(1,dev);
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_WRITE_STATUS_REG);
		flash_write_byte(data);
		NAND_CS_DISABLE();
		flash_WaitForWriteEnd(dev);
	}
}

// Wait for write to complete (BUSY flag in status register)
void flash_WaitForWriteEnd(uint8_t dev) {
    // For NOR Flash, wait until the BUSY flag is cleared
    while ((flash_ReadStatusReg(dev) & 0x01) == 0x01);
}

// Erase a sector
void flash_SectorErase(uint32_t sectorAddress,uint8_t dev) {
	if(dev==DEV_NOR1){
		flash_WriteEnable(1,dev);
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_SECTOR_ERASE);
		flash_write_byte((sectorAddress & 0xFF0000) >> 16);
		flash_write_byte((sectorAddress & 0x00FF00) >> 8);
		flash_write_byte(sectorAddress & 0x0000FF);
		FLASH_CS_DISABLE();
		flash_WaitForWriteEnd(dev);
	}
	else if(dev==DEV_NOR2){
		// Second NOR Flash sector erase
		flash_WriteEnable(1,dev);
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_SECTOR_ERASE);
		flash_write_byte((sectorAddress & 0xFF0000) >> 16);
		flash_write_byte((sectorAddress & 0x00FF00) >> 8);
		flash_write_byte(sectorAddress & 0x0000FF);
		NAND_CS_DISABLE();
		flash_WaitForWriteEnd(dev);
	}
}

// Erase the entire chip
void flash_EraseAll(uint8_t dev) {
	if(dev==DEV_NOR){
    flash_WriteEnable(1,dev);
    FLASH_CS_ENABLE();
    flash_write_byte(FLASH_CMD_CHIP_ERASE);
    FLASH_CS_DISABLE();
    flash_WaitForWriteEnd(dev);
	}
	else if(dev==DEV_NOR2){
		// Second NOR Flash chip erase
		flash_WriteEnable(1,dev);
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_CHIP_ERASE);
		NAND_CS_DISABLE();
		flash_WaitForWriteEnd(dev);
	}
}

// Program a page (write data)
uint8_t flash_PageProgram(uint32_t address, uint8_t* data, uint16_t size,uint8_t dev) {
	if(dev==DEV_NOR1){
 	 	flash_WriteEnable(1,dev);  // Enable write for W25Q64
        FLASH_CS_ENABLE();     // Select W25Q64
        flash_write_byte(FLASH_CMD_PAGE_PROGRAM);  // Send Page Program command
        flash_write_byte((address >> 16) & 0xFF);  // Send address (24-bit)
        flash_write_byte((address >> 8) & 0xFF);
        flash_write_byte(address & 0xFF);

        flash_write(data,size);
        FLASH_CS_DISABLE();
        flash_WaitForWriteEnd(dev);  // Wait for write to complete
	}
	else if(dev==DEV_NOR2){
		// Second NOR Flash page program
 	 	flash_WriteEnable(1,dev);
        NAND_CS_ENABLE();
        flash_write_byte(FLASH_CMD_PAGE_PROGRAM);
        flash_write_byte((address >> 16) & 0xFF);
        flash_write_byte((address >> 8) & 0xFF);
        flash_write_byte(address & 0xFF);

        flash_write(data,size);
        NAND_CS_DISABLE();
        flash_WaitForWriteEnd(dev);
	}
	return FLASH_STATUS_OK;  // Return success
}

// Read data from flash
void flash_ReadData(uint32_t address, uint8_t* data, uint16_t size ,uint8_t dev) {
	if(dev==DEV_NOR1){
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_READ_DATA);
		flash_write_byte((address & 0xFF0000) >> 16);
		flash_write_byte((address & 0xFF00) >> 8);
		flash_write_byte(address & 0xFF);

		flash_read(data,size);
		FLASH_CS_DISABLE();
	}
	else if(dev==DEV_NOR2){
		// Second NOR Flash read data
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_READ_DATA);
		flash_write_byte((address & 0xFF0000) >> 16);
		flash_write_byte((address & 0xFF00) >> 8);
		flash_write_byte(address & 0xFF);

		flash_read(data,size);
		NAND_CS_DISABLE();
	}
}

// W25N02 NAND Flash register test function
void flash_test_w25n02_registers(void) {
	int i;
	uint8_t status, prot_reg, conf_reg, stat_reg, drv_reg;
	uint8_t mfg, type, dev, nand_status, protection, config;
	uint32_t capacity;
	uint8_t test_data[16] = {0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA,
							 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
	uint8_t read_buffer[16];
	uint32_t test_address = 0x1000;
	bool test_passed = true;
	bool all_zero;
	
	DBG("Start W25N02 NAND Flash register test...\n");
	DBG("========== W25N02 Register Test ==========\n");
	
	// Hardware connection test
	DBG("=== Hardware Connection Test ===\n");
	DBG("NAND CS Pin State Test:\n");
	NAND_CS_ENABLE();
	DBG("CS Enabled\n");
	NAND_CS_DISABLE(); 
	DBG("CS Disabled\n");
	
	// SPI communication test
	DBG("=== SPI Communication Test ===\n");
	DBG("Testing basic SPI response...\n");
	
	// Continuous status register read test
	DBG("Testing status register multiple times...\n");
	for(i = 1; i <= 5; i++) {
		status = flash_ReadStatusReg(DEV_NAND);
		DBG("Status test %d: 0x%02X\n", i, status);
	}
	
	// Test reading different feature registers
	DBG("Testing different feature registers...\n");
	NAND_CS_ENABLE();
	flash_write_byte(0x0F); // Get Features command
	flash_write_byte(0xA0); // Protection register
	prot_reg = flash_read_byte();
	NAND_CS_DISABLE();
	DBG("Register 0xA0: 0x%02X\n", prot_reg);
	
	NAND_CS_ENABLE();
	flash_write_byte(0x0F);
	flash_write_byte(0xB0); // Configuration register
	conf_reg = flash_read_byte();
	NAND_CS_DISABLE();
	DBG("Register 0xB0: 0x%02X\n", conf_reg);
	
	NAND_CS_ENABLE();
	flash_write_byte(0x0F);
	flash_write_byte(0xC0); // Status register
	stat_reg = flash_read_byte();
	NAND_CS_DISABLE();
	DBG("Register 0xC0: 0x%02X\n", stat_reg);
	
	NAND_CS_ENABLE();
	flash_write_byte(0x0F);
	flash_write_byte(0xD0); // Driver strength register
	drv_reg = flash_read_byte();
	NAND_CS_DISABLE();
	DBG("Register 0xD0: 0x%02X\n", drv_reg);
	
	// ID read test
	flash_ReadID(&mfg, &type, &dev, DEV_NAND);
	
	// Display device ID and status register information
	DBG("Device ID Test: Mfg=0x%02X, Type=0x%02X, Dev=0x%02X\n", mfg, type, dev);
	DBG("NAND Status Register (0xC0): 0x%02X\n", stat_reg);
	DBG("Status Register: 0x%02X (BUSY=%d, E_FAIL=%d, P_FAIL=%d)\n", 
	    stat_reg, (stat_reg&0x01)?1:0, (stat_reg&0x04)?1:0, (stat_reg&0x08)?1:0);
	DBG("Protection Register (0xA0): 0x%02X\n", prot_reg);
	DBG("Configuration Register (0xB0): 0x%02X (ECC_EN=%d, BUF=%d)\n", 
	    conf_reg, (conf_reg&0x10)?1:0, (conf_reg&0x08)?1:0);
	
	// Capacity detection
	capacity = flash_GetTotalByte(DEV_NAND);
	DBG("NAND Flash ID: Manufacturer=0x%02X, MemoryType=0x%02X, DeviceID=0x%02X\n", mfg, type, dev);
	
	if(dev == 0x22) {
		DBG("Detected W25N02 NAND Flash (Winbond 2Gbit)\n");
	} else if(mfg == 0xEF) {
		DBG("Detected Winbond NAND Flash (Unknown model)\n");
	} else {
		DBG("Unknown NAND Flash device\n");
	}
	
	DBG("Total Capacity: %lu bytes (%.2f MB)\n", (unsigned long)capacity, capacity/1024.0/1024.0);
	
	// Bad block management test
	DBG("=== Bad Block Management Test ===\n");
	DBG("Total bad blocks detected: %d\n", bad_block_manager.table.count);
	if (bad_block_manager.table.count > 0) {
		DBG("Bad block list: ");
		for (i = 0; i < bad_block_manager.table.count; i++) {
			DBG("%d ", bad_block_manager.table.bad_blocks[i]);
		}
		DBG("\n");
	}
	
	// Read/Write test at a specific address
	DBG("Performing read/write test at address 0x%04lX...\n", test_address);
	
	// Erase, program, and read back test
	flash_SectorErase(test_address, DEV_NAND);
	flash_PageProgram(test_address, test_data, 16, DEV_NAND);
	flash_ReadData(test_address, read_buffer, 16, DEV_NAND);
	
	// Compare the written and read data
	test_passed = true;
	for(i = 0; i < 16; i++) {
		if(test_data[i] != read_buffer[i]) {
			test_passed = false;
			break;
		}
	}
	
	if(test_passed) {
		DBG("Read/Write Test: PASSED\n");
	} else {
		DBG("Read/Write Test: FAILED\n");
		DBG("Expected: ");
		for(i = 0; i < 16; i++) DBG("%02X ", test_data[i]);
		DBG("\nActual:   ");
		for(i = 0; i < 16; i++) DBG("%02X ", read_buffer[i]);
		DBG("\n");
		
		// Check if the read data is all zeros
		all_zero = true;
		for(i = 0; i < 16; i++) {
			if(read_buffer[i] != 0x00) {
				all_zero = false;
				break;
			}
		}
		if(all_zero) {
			DBG("All read data is 0x00 - possible hardware issue or wrong commands\n");
		}
	}
	
	DBG("========== W25N02 Test Complete ==========\n");
}

// Bad block management initialization
static void bad_block_manager_init(uint8_t dev) {
    if (bad_block_manager.initialized) {
        return;
    }
    
    uint8_t manufacturerID, memoryType, deviceID;
    flash_ReadID(&manufacturerID, &memoryType, &deviceID, dev);
    
    // Set the bad block table storage address based on Flash size
    uint32_t total_size = flash_GetTotalByte(dev);
    
    // For NAND Flash, reserve space at the beginning of the block
    if (dev == DEV_NAND) {
        bad_block_manager.table_address = 64 * 2048;  // Default to first block
    } else {
        // For NOR Flash, reserve space at the end of the chip
        bad_block_manager.table_address = total_size - sizeof(BadBlockTable);
    }
    
    DBG("Initializing bad block manager...\n");
    DBG("Total Flash size: 0x%08X bytes\n", total_size);
    DBG("Bad block table stored at: 0x%08X\n", bad_block_manager.table_address);
    
    // Initial setting, skip the initial scan for bad blocks
    bad_block_manager.initialized = true;
    
    // Load existing bad block table from Flash
    load_bad_block_table(dev);
    
    // If the loaded table is not valid, create a new one
    if (bad_block_manager.table.magic != BAD_BLOCK_TABLE_MAGIC || 
        bad_block_manager.table.version != BAD_BLOCK_TABLE_VERSION) {
        DBG("No valid bad block table found, creating new one\n");
        
        // Initialize a new bad block table
        memset(&bad_block_manager.table, 0, sizeof(BadBlockTable));
        bad_block_manager.table.magic = BAD_BLOCK_TABLE_MAGIC;
        bad_block_manager.table.version = BAD_BLOCK_TABLE_VERSION;
        bad_block_manager.table.count = 0;
        
        // Skip the initial bad block scan for now
        DBG("Skipping initial bad block scan for now, will detect during runtime\n");
        
        // Save the empty bad block table to Flash
        save_bad_block_table(dev);
    } else {
        DBG("Loaded existing bad block table, %d bad blocks found\n", bad_block_manager.table.count);
    }
    
    // Mark the manager as initialized
    bad_block_manager.initialized = true;
}

// Check if a block is bad
static bool is_block_bad(uint32_t block_address, uint8_t dev) {
    uint16_t i;
    
    if (!bad_block_manager.initialized) {
        bad_block_manager_init(dev);
    }
    
    // Check if the block is in the bad block table
    for (i = 0; i < bad_block_manager.table.count; i++) {
        if (bad_block_manager.table.bad_blocks[i] == block_address) {
            return true;
        }
    }
    return false;
}

// Mark a block as bad
static bool mark_block_as_bad(uint32_t block_address, uint8_t dev) {
    if (!bad_block_manager.initialized) {
        bad_block_manager_init(dev);
    }
    
    // If already marked as bad, do nothing
    if (is_block_bad(block_address, dev)) {
        return true;
    }
    
    // If the bad block table is full, cannot mark more blocks
    if (bad_block_manager.table.count >= sizeof(bad_block_manager.table.bad_blocks)/sizeof(bad_block_manager.table.bad_blocks[0])) {
        DBG("Cannot mark block %d as bad - bad block table is full\n", block_address);
        return false;
    }
    
    // Add the bad block to the table
    bad_block_manager.table.bad_blocks[bad_block_manager.table.count++] = block_address;
    DBG("Block %d marked as bad\n", block_address);
    
    // Optionally, write a marker to the bad block's address in Flash
    uint32_t address = block_to_address(block_address, dev);
    uint8_t marker = BAD_BLOCK_MARKER;
    flash_PageProgram(address, &marker, 1, dev);
    
    // Save the updated bad block table to Flash
    save_bad_block_table(dev);
    return true;
}

// Find the next good block starting from a given block
static uint32_t find_next_good_block(uint32_t start_block, uint8_t dev) {
    uint32_t total_blocks;
    uint32_t block;
    
    if (!bad_block_manager.initialized) {
        bad_block_manager_init(dev);
    }
    
    total_blocks = flash_GetTotalByte(dev) / (64 * 2048);
    
    // Search forward from the start block
    for (block = start_block; block < total_blocks; block++) {
        if (!is_block_bad(block, dev)) {
            return block;
        }
    }
    
    // If not found, wrap around and search from the beginning
    for (block = 0; block < start_block; block++) {
        if (!is_block_bad(block, dev)) {
            return block;
        }
    }
    
    // No good block found, return an invalid address
    return 0xFFFFFFFF; // Invalid block address
}

// Save the bad block table to Flash
static void save_bad_block_table(uint8_t dev) {
    // Erase the sector where the bad block table is stored
    flash_SectorErase(bad_block_manager.table_address, dev);
    
    // Program the bad block table data to Flash
    flash_PageProgram(bad_block_manager.table_address, 
                     (uint8_t*)&bad_block_manager.table, 
                     sizeof(BadBlockTable), 
                     dev);
                     
    DBG("Bad block table saved, %d bad blocks recorded\n", bad_block_manager.table.count);
}

// Load the bad block table from Flash
static void load_bad_block_table(uint8_t dev) {
    flash_ReadData(bad_block_manager.table_address,
                  (uint8_t*)&bad_block_manager.table,
                  sizeof(BadBlockTable),
                  dev);
}

// Convert a Flash address to a block number
static uint32_t address_to_block(uint32_t address, uint8_t dev) {
    // For W25N02, each block is 64K and contains 2048 bytes per page
    return address / (64 * 2048);
}

// Convert a block number to a Flash address
static uint32_t block_to_address(uint32_t block, uint8_t dev) {
    // For W25N02, each block is 64K and contains 2048 bytes per page
    return block * 64 * 2048;
}

// Check if a block is bad (for external use)
uint8_t nand_check_bad_block(uint32_t block_address, uint8_t dev) {
    return is_block_bad(block_address, dev) ? 1 : 0;
}

// Mark a block as bad (for external use)
uint8_t nand_mark_bad_block(uint32_t block_address, uint8_t dev) {
    return mark_block_as_bad(block_address, dev) ? 1 : 0;
}

// Find the next good block (for external use)
uint32_t nand_find_next_good_block(uint32_t start_block, uint8_t dev) {
    return find_next_good_block(start_block, dev);
}

// Get a safe write address, skipping bad blocks (for external use)
uint32_t nand_get_safe_write_address(uint32_t current_address, uint32_t bytes_to_write, uint8_t dev) {
    if (dev != DEV_NAND) {
        return current_address;  // For NOR Flash, return the current address
    }
    
    uint32_t current_block = address_to_block(current_address, dev);
    
    // If the current block is bad, find the next good block
    if (is_block_bad(current_block, dev)) {
        uint32_t next_good_block = find_next_good_block(current_block + 1, dev);
        if (next_good_block == 0xFFFFFFFF) {
            DBG("ERROR: No good blocks available!\n");
            return 0xFFFFFFFF;
        }
        return block_to_address(next_good_block, dev);
    }
    
    return current_address;
}

// NAND Flash audio buffer management
#define NAND_PAGE_SIZE 2048
#define NAND_AUDIO_BUFFER_SIZE NAND_PAGE_SIZE

static struct {
    uint8_t buffer[NAND_AUDIO_BUFFER_SIZE];
    uint32_t current_page_address;
    uint16_t buffer_pos;
    bool initialized;
} nand_audio_buffer = {{0}, 0, 0, false};

uint8_t nand_audio_write_buffered(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev) {
    if (dev != DEV_NAND) {
        // For NOR Flash, write directly
        return flash_PageProgram(address, data, size, dev);
    }
    
    // Initialize the audio buffer management
    if (!nand_audio_buffer.initialized) {
        nand_audio_buffer.current_page_address = (address / NAND_PAGE_SIZE) * NAND_PAGE_SIZE;
        nand_audio_buffer.buffer_pos = address % NAND_PAGE_SIZE;
        nand_audio_buffer.initialized = true;
    }
    
    uint16_t data_pos = 0;
    
    while (data_pos < size) {
        uint32_t target_page = (address + data_pos) / NAND_PAGE_SIZE * NAND_PAGE_SIZE;
        
        // If the target page is different, flush the current buffer
        if (target_page != nand_audio_buffer.current_page_address) {
            if (nand_audio_buffer.buffer_pos > 0) {
                uint8_t result = nand_audio_flush_buffer(dev);
                if (result != FLASH_STATUS_OK) {
                    return result;
                }
            }
            
            // Switch to the new target page
            nand_audio_buffer.current_page_address = target_page;
            nand_audio_buffer.buffer_pos = 0;
        }
        
        // Calculate the number of bytes to copy to the buffer
        uint16_t remaining_in_page = NAND_PAGE_SIZE - nand_audio_buffer.buffer_pos;
        uint16_t remaining_data = size - data_pos;
        uint16_t bytes_to_copy = (remaining_in_page < remaining_data) ? remaining_in_page : remaining_data;
        
        // Copy data to the buffer
        memcpy(&nand_audio_buffer.buffer[nand_audio_buffer.buffer_pos], 
               &data[data_pos], bytes_to_copy);
        
        nand_audio_buffer.buffer_pos += bytes_to_copy;
        data_pos += bytes_to_copy;
        
        // If the buffer is full, flush it to Flash
        if (nand_audio_buffer.buffer_pos >= NAND_PAGE_SIZE) {
            uint8_t result = nand_audio_flush_buffer(dev);
            if (result != FLASH_STATUS_OK) {
                return result;
            }
        }
    }
    
    return FLASH_STATUS_OK;
}

// Initialize the smart audio buffer for NAND
void nand_smart_audio_init(void) {
    nand_audio_buffer.buffer_pos = 0;
    nand_audio_buffer.current_page_address = 0;
    nand_audio_buffer.initialized = true;
    DBG("NAND smart audio buffer initialized\n");
}

// Get the current write address for the NAND audio buffer
uint32_t nand_smart_audio_get_address(void) {
    if (!nand_audio_buffer.initialized) {
        return 0;
    }
    return nand_audio_buffer.current_page_address + nand_audio_buffer.buffer_pos;
}

// Flush the NAND audio buffer to Flash
uint8_t nand_audio_flush_buffer(uint8_t dev) {
    if (!nand_audio_buffer.initialized || nand_audio_buffer.buffer_pos == 0) {
        return FLASH_STATUS_OK;
    }
    
    // Check if the current page is bad, find a new good page if necessary
    uint32_t block = address_to_block(nand_audio_buffer.current_page_address, dev);
    if (is_block_bad(block, dev)) {
        uint32_t next_good_block = find_next_good_block(block + 1, dev);
        if (next_good_block == 0xFFFFFFFF) {
            DBG("ERROR: No good blocks available for audio buffer flush!\n");
            return FLASH_STATUS_ERROR;
        }
        nand_audio_buffer.current_page_address = block_to_address(next_good_block, dev);
    }
    
    // Program the audio buffer data to Flash
    uint8_t result = flash_PageProgram(nand_audio_buffer.current_page_address, 
                                      nand_audio_buffer.buffer, 
                                      nand_audio_buffer.buffer_pos, 
                                      dev);
    
    if (result == FLASH_STATUS_OK) {
        // Reset the buffer position and move to the next page
        nand_audio_buffer.buffer_pos = 0;
        nand_audio_buffer.current_page_address += NAND_PAGE_SIZE;
        return FLASH_STATUS_OK;
    } else {
        // If the write fails, mark the block as bad
        mark_block_as_bad(block, dev);
        DBG("Audio buffer flush failed, marked block %lu as bad\n", (unsigned long)block);
        return FLASH_STATUS_ERROR;
    }
}

// NAND smart audio write
uint8_t nand_smart_audio_write(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev) {
    // Use buffered write for NAND
    return nand_audio_write_buffered(address, data, size, dev);
}

// NAND smart audio read
uint8_t nand_smart_audio_read(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev) {
    // Direct read from Flash
    BG_flash_manager.ReadData(address, data, size, dev);
    return FLASH_STATUS_OK;
}

// NAND smart audio flush
uint8_t nand_smart_audio_flush(uint8_t dev) {
    return nand_audio_flush_buffer(dev);
}

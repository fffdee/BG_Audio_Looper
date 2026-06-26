/*
 * init-default.c
 *
 *  Created on: Mar 8, 2017
 *      Author: peter
 */

#include <nds32_intrinsic.h>
#include "debug.h"

/* Linker-defined symbol: start of executable (vector table base) */
extern char __executable_start;

/* It will use Default_Handler if you don't have one */
//#pragma weak ExceptionCommHandler = Default_Handler

//#pragma weak SystickInterrup   =  Default_Handler
#pragma weak WakeupInterrupt   =  Default_Handler
#pragma weak GpioInterrupt     =  Default_Handler
#pragma weak RtcInterrupt      =  Default_Handler
#pragma weak SpdifInterrupt    =  Default_Handler
#pragma weak SWInterrupt       =  Default_Handler
#pragma weak I2C_Interrupt     =  Default_Handler
#pragma weak UART0_Interrupt   =  Default_Handler
#pragma weak Timer2Interrupt   =  Default_Handler
#pragma weak DMA0_Interrupt    =  Default_Handler
#pragma weak DMA1_Interrupt    =  Default_Handler
#pragma weak DMA2_Interrupt    =  Default_Handler
#pragma weak DMA3_Interrupt    =  Default_Handler
#pragma weak DMA4_Interrupt    =  Default_Handler
#pragma weak DMA5_Interrupt    =  Default_Handler
#pragma weak DMA6_Interrupt    =  Default_Handler
#pragma weak DMA7_Interrupt    =  Default_Handler
#pragma weak BT_Interrupt      =  Default_Handler
#pragma weak BLE_Interrupt      =  Default_Handler
#pragma weak I2sInterrupt      =  Default_Handler
#pragma weak Timer3Interrupt   =  Default_Handler
#pragma weak Timer4Interrupt   =  Default_Handler
#pragma weak Timer5Interrupt   =  Default_Handler
#pragma weak Timer6Interrupt   =  Default_Handler
#pragma weak SDIO0_Interrupt   =  Default_Handler
#pragma weak UsbInterrupt      =  Default_Handler
#pragma weak SPIM_Interrupt    =  Default_Handler
#pragma weak SPIS_Interrupt    =  Default_Handler
#pragma weak FFTInterrupt      =  Default_Handler
#pragma weak IRInterrupt      =  Default_Handler
#pragma weak OS_Trap_Interrupt_SWI =  Default_Handler

__attribute__((unused))
static void Default_Handler()
{
	while (1) ;
}

__attribute__((unused))
void ExceptionCommHandler(unsigned stack, unsigned exception_num)
{

	unsigned int mask_itype,mask_ipc;
	unsigned int *pstack;

	  pstack = (unsigned int *)stack;
	  DBG("Error exception happened\r

	  mask_itype = __nds32__mfsr(NDS32_SR_ITYPE);
	  mask_ipc = __nds32__mfsr(NDS32_SR_IPC);
	  mask_itype &= 0x0F;

	  if(exception_num == 7)
	  {
		  DBG("Error Type:");
		  if(mask_itype == 0)
		  {
			  DBG("Alignment check (+I/D bit) or branch target alignment\r\n");
		  }
		  else if(mask_itype == 1)
		  {
			  DBG("Reserved instruction\r\n");
		  }
		  else if(mask_itype == 2)
		  {
			  DBG("Trap\r\n");
		  }
		  else if(mask_itype == 3)
		  {
			  DBG("Arithmetic\r\n");
		  }
		  else if(mask_itype == 4)
		  {
			  DBG("Precise bus error\r\n");
		  }
		  else if(mask_itype == 5)
		  {
			  DBG("Imprecise bus error\r\n");
		  }
		  else if(mask_itype == 6)
		  {
			  DBG("Coprocessor\r\n");
		  }
		  else if(mask_itype == 7)
		  {
			  DBG("Privileged instruction\r\n");
		  }
		  else if(mask_itype == 8)
		  {
			  DBG("Reserved value\r\n");
		  }
		  else if(mask_itype == 9)
		  {
			  DBG("Nonexistent memory address (+I/D bit)\r\n");
		  }
		  else if(mask_itype == 10)
		  {
			  DBG("MPZIU Control (+I/D bit)\r\n");
		  }
		  else if(mask_itype == 11)
		  {
			  DBG("Next precise stack overflow\r\n");
		  }
		  else
		  {
			  DBG("Unknown\r\n");
		  }
	  }
	  else if(exception_num == 1)
	  {

	  }
	  else if(exception_num == 2)
	  {

	  }
	  else if(exception_num == 3)
	  {

	  }
	  else if(exception_num == 4)
	  {

	  }
	  else if(exception_num == 5)
	  {

	  }
	  else if(exception_num == 6)
	  {

	  }
	  else if(exception_num == 8)
	  {

	  }
	  else
	  {

	  }

	  DBG("PC  = 0x%08x\r\n",mask_ipc);
	  DBG("R0  = 0x%08x\r\n",*pstack++);
	  DBG("R1  = 0x%08x\r\n",*pstack++);
	  DBG("R2  = 0x%08x\r\n",*pstack++);
	  DBG("R3  = 0x%08x\r\n",*pstack++);
	  DBG("R4  = 0x%08x\r\n",*pstack++);
	  DBG("R5  = 0x%08x\r\n",*pstack++);
	  DBG("R6  = 0x%08x\r\n",*pstack++);
	  DBG("R7  = 0x%08x\r\n",*pstack++);
	  DBG("R8  = 0x%08x\r\n",*pstack++);
	  DBG("R9  = 0x%08x\r\n",*pstack++);
	  DBG("R10 = 0x%08x\r\n",*pstack++);
	  DBG("R11 = 0x%08x\r\n",*pstack++);
	  DBG("R12 = 0x%08x\r\n",*pstack++);
	  DBG("R13 = 0x%08x\r\n",*pstack++);
	  DBG("R14 = 0x%08x\r\n",*pstack++);
	  DBG("R15 = 0x%08x\r\n",*pstack++);
	  DBG("R16 = 0x%08x\r\n",*pstack++);
	  DBG("R17 = 0x%08x\r\n",*pstack++);
	  DBG("R18 = 0x%08x\r\n",*pstack++);
	  DBG("R19 = 0x%08x\r\n",*pstack++);
	  DBG("R20 = 0x%08x\r\n",*pstack++);
	  DBG("lp  = 0x%08x\r\n",*pstack++);
	  DBG("sp  = 0x%08x\r\n",*pstack++);
	  DBG("exception num = %d\n", exception_num);
	  DBG("Error type num: %d\r\n", mask_itype);
	//DBG("Error exception happened\r\n");
	//NVIC_SystemReset();
	while(1) ;
}

/* Bootloader handoff: bootloader writes this magic to SRAM before jumping
 * to signal that .data has already been copied.  The APP's __c_init() will
 * check this magic and skip its own .data copy.
 *
 * Why this is needed: the APP cannot safely copy .data from flash to SRAM
 * by itself because of the SBus/IBus conflict on the BP10 flash controller.
 * When the APP's __c_init() executes from flash (IBus) while reading .data
 * LMA from flash (SBus), the two buses conflict and the CPU hangs.
 *
 * The bootloader's code is I-Cache resident (warmed up during boot), so
 * the copy loop in Boot_JumpTo() executes from cache (no IBus) while
 * reading .data from flash (SBus) — no bus conflict. */
#define BOOT_HANDOFF_ADDR  0x20000000UL
#define BOOT_HANDOFF_MAGIC 0xDEADBEEFUL

/* Direct UART1 write for early diagnostics — bypasses DBG/printf entirely.
 * UART1 is already initialized by the bootloader, so we can write directly
 * to the MMIO registers.  This works even before __c_init() and with
 * interrupts disabled.
 *   UART1 base:   0x40006000
 *   Status reg:   base+0x14, bit 9 = TX FIFO ready
 *   TX data reg:  base+0x18 */
#define UART1_STATUS  (*(volatile uint32_t *)0x40006014)
#define UART1_TX      (*(volatile uint32_t *)0x40006018)
static inline void diag_putc(char c)
{
	while (!(UART1_STATUS & (1u << 9))) ;  /* wait for TX FIFO ready */
	UART1_TX = (uint32_t)(unsigned char)c;
}

/* Returns 1 if this is the APP (linked at 0x040000+), 0 if bootloader */
static inline int is_app(void)
{
	return ((uint32_t)&__executable_start >= 0x040000UL);
}

void __c_init()
{

/* Use compiler builtin memcpy and memset */
#define MEMCPY(des, src, n) __builtin_memcpy ((des), (src), (n))
#define MEMSET(s, c, n) __builtin_memset ((s), (c), (n))

	extern char _end;
	extern char __bss_start;
	int size;

	/* data section will be copied before we remap.
	 * We don't need to copy data section here. */
	extern char __data_lmastart;
	extern char __data_start;
	extern char _edata;

	if (is_app())
	{
		diag_putc('c');  /* __c_init entered */

		/* Check if bootloader already did .data copy + .bss clear.
		 * Handoff magic at 0x20000000 signals bootloader has already done
		 * all SRAM init.  If magic is NOT present, we fall through to the
		 * legacy path below to do .bss clear ourselves.
		 *
		 * NOTE: __c_init runs BEFORE HardwareStackProtectEnable so that
		 * reading 0x20000000 (below SP_BOUND=0x20003000) does not trigger
		 * a Hardware Stack Protection fault. */
		if (*(volatile uint32_t *)BOOT_HANDOFF_ADDR == BOOT_HANDOFF_MAGIC)
		{
			diag_putc('h');  /* bootloader handoff OK — skip .data copy & .bss clear */
			diag_putc('r');  /* __c_init returning (skip path) */
			return;
		}
		/* Magic NOT present: fall through to clear .bss ourselves */
	}
	else
	{
		/* Non-APP (standalone/bootloader) path: copy .data from flash to SRAM.
		 * NO diag_putc here — bootloader UART may not be initialized yet. */
		size = &_edata - &__data_start;
		MEMCPY(&__data_start, &__data_lmastart, size);
	}

	/* Clear bss section — safe: only writes to SRAM, no flash access */
	size = &_end - &__bss_start;
	MEMSET(&__bss_start, 0, size);

	return;
}

void __cpu_init()
{
	unsigned int tmp;

	/* turn on BTB */
//	tmp = 0x0;
//	__nds32__mtsr(tmp, NDS32_SR_MISC_CTL);

	tmp = __nds32__mfsr(NDS32_SR_MMU_CTL);
	tmp |= 0x800000;
	__nds32__mtsr(tmp,NDS32_SR_MMU_CTL);
	
	/* disable all hardware interrupts */
	__nds32__mtsr(0x0, NDS32_SR_INT_MASK);
#if (defined(__NDS32_ISA_V3M__) || defined(__NDS32_ISA_V3__))
	if (__nds32__mfsr(NDS32_SR_IVB) & 0x01)
		__nds32__mtsr(0x0, NDS32_SR_INT_MASK);
#endif

#if defined(CFG_EVIC)
	/* set EVIC, vector size: 4 bytes, base: APP vector table */
	__nds32__mtsr((0x1<<13) | ((uint32_t)&__executable_start & 0xFFFF0000UL), NDS32_SR_IVB);
#else
# if defined(USE_C_EXT)
	/* If we use v3/v3m toolchain and want to use
	 * C extension please use USE_C_EXT in CFLAGS
	 */
#ifdef __NDS32_ISA_V3__
	/* set IVIC, vector size: 4 bytes, base: APP vector table */
	__nds32__mtsr((uint32_t)&__executable_start & 0xFFFF0000UL, NDS32_SR_IVB);
#else
	/* set IVIC, vector size: 16 bytes, base: APP vector table */
	__nds32__mtsr((0x1<<14) | ((uint32_t)&__executable_start & 0xFFFF0000UL), NDS32_SR_IVB);
#endif
# else
	/* set IVIC, vector size: 4 bytes, base: APP vector table
	 * If we use v3/v3m toolchain and want to use
	 * assembly version please don't use USE_C_EXT
	 * in CFLAGS */
	__nds32__mtsr((uint32_t)&__executable_start & 0xFFFF0000UL, NDS32_SR_IVB);
# endif
#endif
	/* Set PSW INTL to 0 */
	tmp = __nds32__mfsr(NDS32_SR_PSW);
	tmp = tmp & 0xfffffff9;
#if (defined(__NDS32_ISA_V3M__) || defined(__NDS32_ISA_V3__))
	/* Set PSW CPL to 7 to allow any priority */
	tmp = tmp | 0x70008;
#endif
	__nds32__mtsr_dsb(tmp, NDS32_SR_PSW);
#if (defined(__NDS32_ISA_V3M__) || defined(__NDS32_ISA_V3__))
	/* Check interrupt priority programmable*
	* IVB.PROG_PRI_LVL
	*      0: Fixed priority       -- no exist ir18 1r19
	*      1: Programmable priority
	*/
	if (__nds32__mfsr(NDS32_SR_IVB) & 0x01) {
		/* Set PPL2FIX_EN to 0 to enable Programmable
	 	* Priority Level */
		__nds32__mtsr(0x0, NDS32_SR_INT_CTRL);
		/* Check IVIC numbers (IVB.NIVIC) */
		if ((__nds32__mfsr(NDS32_SR_IVB) & 0x0E)>>1 == 5) {	// 32IVIC
			/* set priority HW9: 0, HW13: 1, HW19: 2,
			* HW#-: 0 */
			__nds32__mtsr(~0x0, NDS32_SR_INT_PRI);
			__nds32__mtsr(~0x0, NDS32_SR_INT_PRI2);
		} else {
			/* set priority HW0: 0, HW1: 1, HW2: 2, HW3: 3
			* HW4-: 0 */
			__nds32__mtsr(0x000000e4, NDS32_SR_INT_PRI);
		}
	}
#endif
	/* enable FPU if the CPU support FPU */
#if defined(__NDS32_EXT_FPU_DP__) || defined(__NDS32_EXT_FPU_SP__)
	tmp = __nds32__mfsr(NDS32_SR_FUCOP_EXIST);
	if ((tmp & 0x80000001) == 0x80000001) {
		tmp = __nds32__mfsr(NDS32_SR_FUCOP_CTL);
		__nds32__mtsr_dsb((tmp | 0x1), NDS32_SR_FUCOP_CTL);

		/* Denormalized flush-to-Zero mode on */
		tmp =__nds32__fmfcsr();
		tmp |= (1 << 12);
		__nds32__fmtcsr(tmp);
		__nds32__dsb();
	}
#endif

	__nds32__mtsr(__nds32__mfsr(NDS32_SR_INT_PEND2), NDS32_SR_INT_PEND2);  //���pending

	return;
}

#define HSP_CTL_offHSPEN        0
#define HSP_CTL_offSCHM         1
#define HSP_CTL_offSUSER        2
#define HSP_CTL_offUSER         3
#define HSP_ENABLE              (1 << HSP_CTL_offHSPEN)
#define HSP_DISABLE             (0 << HSP_CTL_offHSPEN)
#define HSP_SCHM_OVERFLOW       (0 << HSP_CTL_offSCHM)
#define HSP_SCHM_TOPRECORD      (1 << HSP_CTL_offSCHM)
#define HSP_SUPERUSER           (1 << HSP_CTL_offSUSER)
#define HSP_USER                (1 << HSP_CTL_offUSER)
void HardwareStackProtectEnable(void)
{
	if (!(__nds32__mfsr(NDS32_SR_MSC_CFG) & (1 << 27)))
	{
		//DBG("CPU does NOT support HW Stack protection/recording.\n");
	}
	else
	{
		__nds32__mtsr(__nds32__mfsr(NDS32_SR_HSP_CTL) & ~0x0f, NDS32_SR_HSP_CTL);

		__nds32__mtsr(0x20003000, NDS32_SR_SP_BOUND);

		__nds32__mtsr(HSP_ENABLE | HSP_SCHM_OVERFLOW | HSP_SUPERUSER, NDS32_SR_HSP_CTL);

		//DBG("CPU support HW Stack protection/recording.\n");
	}
}

void EnableIDCache(void);
void Chip_MemInit(void);
/* Re-initialize UART driver after __c_init() clears .bss.
 * The .bss clear zeroes all global variables including the UART driver's
 * software state (ring buffer pointers, baud rate config, etc.).  Without
 * re-init, subsequent DBG/printf output is garbled because the driver
 * operates with zeroed state.  DbgUartInit only resets the software side;
 * the hardware is already configured by the bootloader.
 * (Declaration is in debug.h, included above.) */
void __init()
{
/*----------------------------------------------------------
   !!  Users should NOT add any code before this comment  !!
------------------------------------------------------------*/
	if (is_app()) diag_putc('A');  /* __init entered */
	__cpu_init();
	if (is_app()) diag_putc('B');  /* __cpu_init done */

	/* If bootloader already did .data copy + .bss clear (handoff magic
	 * present), __c_init() has nothing to do.  Moreover, __c_init()'s
	 * return epilogue crashes on APP side — skip the call entirely. */
	if (!is_app() || *(volatile uint32_t *)BOOT_HANDOFF_ADDR != BOOT_HANDOFF_MAGIC)
	{
		if (is_app()) diag_putc('y');  /* need __c_init (no handoff) */
		__c_init();     //copy data section, clean bss
		if (is_app()) diag_putc('z');  /* __c_init done */
	}
	else
	{
		if (is_app()) diag_putc('x');  /* skipping __c_init (bootloader handoff OK) */
	}

	/* TEMPORARY: Skip EnableIDCache to isolate TLB hang */
	if (is_app()) diag_putc('!');  /* SKIPPING EnableIDCache & DataCacheInvalidAll */
	// EnableIDCache(); // SKIPPED — test if TLB programming is the root cause
	// DataCacheInvalidAll(); // SKIPPED

	HardwareStackProtectEnable();  // AFTER __c_init to avoid blocking SRAM reads below SP_BOUND
	if (is_app()) diag_putc('D');  /* HardwareStackProtectEnable done */
	Chip_MemInit();
	if (is_app()) diag_putc('E');  /* Chip_MemInit done */

	/* Re-init UART driver software state — .bss was just cleared above,
	 * zeroing the driver's internal variables.  Only needed when started
	 * by bootloader (the standalone cold-start path initializes UART later
	 * in main(), before any .bss-dependent code runs). */
	if (is_app()) {
		DbgUartInit(1, 115200, 8, 0, 1);
		diag_putc('S');  /* UART driver re-initialized */
	}
}

__attribute__ ((section(".stub_section"),used)) __attribute__((naked))
void stub(void)
{
__asm__ __volatile__(

    		".long 0x42475046 \n\n"	//0xA4 FW_VALID_MAGIC "BGPF" — bootloader checks this
    		".long 0xFFFFFFFF \n\n" //0xA8
    		".long 0xFFFFFFFF \n\n" //0xAC
    		".long 0x100000 \n\n"		//0xB0 constant data @ 0x8C
    		".long 0x1D0000 \n\n"		//0xB4 user data
    		".byte 0x01 \n\n"				//0xB8 0x01����B1X
    		".byte 0 \n\n"					//0xB9
    		".byte 0 \n\n"					//0xBA
    		".byte 1 \n\n"					//0xBB
    		".long 0xFFFFFFFF \n\n"	//0xBC code crc
    		".long 0xB0BEBDC9 \n\n"	//0xC0 magic number
    		".long 0x00000706 \n\n"	//0xC4 32KHz external oscillator input/output capacitance calibration value
    		".long 0xFFFFFFFF \n\n"	//0xC8 fast code crc	@ 0xA4
    		".long 0xFFFFFFFF \n\n"	//0xCC fast code crc	@ 0xA4
    		".long 0xFFFFFFFF \n\n"	//0xD0 fast code crc	@ 0xA4
    		".long 0x03444846 \n\n"	//0xD4 fast code crc	@ 0xA4
		    ".rept (0xFC-0xD8)/4 \n\n"
    		".long 0xFFFFFFFF \n\n"
		    ".endr \n\n"
			".long 0x00FFFFFF \n\n"
		    ".short 0xFFFF \n\n"
		    /* ---- Boot info for bootloader (offset 0x104 from image base) ---- */
		    ".balign 4 \n\n"
		    ".long 0x42474F46 \n\n"    /* "BGOF" magic — Boot Info */
		    ".long __data_lmastart \n\n"  /* .data LMA in flash */
		    ".long __data_start \n\n"     /* .data VMA in SRAM   */
		    ".long _edata \n\n"           /* .data end VMA       */
		    ".long __bss_start \n\n"     /* .bss VMA in SRAM    */
		    ".long _end \n\n"             /* .bss end VMA        */
    );
}

#include "core_d1088.h"
const uint32_t MPUConfigTable[8][7] =
{
	{MPU_ENTRY_ENABLE, 0,  		     0x1FFFFFFF,	0, 				CACHEABILITY_WRITE_BACK, 		EXECUTABLE_USER,	ACCESS_READ 	},
	{MPU_ENTRY_ENABLE, 0x20000000, 	 0x48000,		0x20000000,		CACHEABILITY_WRITE_THROUGH, 	EXECUTABLE_USER,	ACCESS_RW		},
	{MPU_ENTRY_ENABLE, 0x40000000, 	 0x1FFFFFFF,	0x40000000,		CACHEABILITY_DEVICE,			EXECUTABLE_USER,	ACCESS_RW		},
	{MPU_ENTRY_ENABLE, 0x60000000, 	 0x1FFFFFFF,	0x60000000,		CACHEABILITY_WRITE_THROUGH,		EXECUTABLE_USER,	ACCESS_RW		},
	{MPU_ENTRY_ENABLE, 0x80000000,	 0x8000,		0x20048000, 	CACHEABILITY_DEVICE,			EXECUTABLE_USER,	ACCESS_RW		},
	{MPU_ENTRY_DISABLE},
	{MPU_ENTRY_DISABLE},
	{MPU_ENTRY_DISABLE},
};

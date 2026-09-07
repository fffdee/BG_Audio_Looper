/*
 * init-default.c
 *
 *  Created on: Mar 8, 2017
 *      Author: peter
 */

#include <nds32_intrinsic.h>
#include "debug.h"
#include "app_config.h"

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

/* ★裸 MMIO UART 诊断输出：不依赖 DBG 驱动，冷/热态都能工作。
 * 上移到文件前部，让 ExceptionCommHandler 也能用它打印可读故障码。★ */
#define APP_DIAG_UART1_STATUS  (*(volatile uint32_t *)0x40006014)
#define APP_DIAG_UART1_TX      (*(volatile uint32_t *)0x40006018)
static inline void app_diag_putc(char c)
{
	while (!(APP_DIAG_UART1_STATUS & (1u << 9)))
		;
	APP_DIAG_UART1_TX = (uint32_t)(unsigned char)c;
}
static inline void app_diag_puthex(uint32_t v)
{
	int i;
	for (i = 28; i >= 0; i -= 4) {
		unsigned n = (v >> i) & 0xFu;
		app_diag_putc((char)(n < 10u ? ('0' + n) : ('A' + n - 10)));
	}
}

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
	  DBG("Error exception happened\r\n");

	  mask_itype = __nds32__mfsr(NDS32_SR_ITYPE);
	  mask_ipc = __nds32__mfsr(NDS32_SR_IPC);
	  mask_itype &= 0x0F;

	  /* ★用裸 MMIO UART 打印可读故障码（DBG 此刻未初始化=乱码）：
	   *   格式 E<trap号>.<itype>@<出错PC 8位hex>
	   *   trap号: 1=TLB_Fill 2=PTE_Not_Present 3=TLB_Misc 4=TLB_VLPT_Miss
	   *           5=Machine_Error 6=Debug 7=General_Exception 8=Syscall
	   *   itype(trap7 才有意义): 0=对齐 1=保留指令 4=精确总线错
	   *           5=非精确总线错 9=不存在内存地址 11=栈溢出
	   *   例 E7.5@00041234 = General_Exception/非精确总线错, PC=0x00041234 ★ */
	  app_diag_putc('E');
	  app_diag_putc("0123456789ABCDEF"[exception_num & 0xFu]);
	  app_diag_putc('.');
	  app_diag_putc("0123456789ABCDEF"[mask_itype & 0xFu]);
	  app_diag_putc('@');
	  app_diag_puthex(mask_ipc);

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

void __c_init()
{

/* Use compiler builtin memcpy and memset */
#define MEMCPY(des, src, n) __builtin_memcpy ((des), (src), (n))
#define MEMSET(s, c, n) __builtin_memset ((s), (c), (n))

#if HAS_BOOTLOADER
	/* 由 bootloader 跳转启动：Boot_JumpTo() Phase 3 已按 BootInfo 完成 .data
	 * 拷贝(日志 'd') 与 .bss 清零(日志 'z')，且刻意不写 0x20000000 handoff
	 * 魔数(见 upgrade.c 第 337 行注释)。因此 APP 必须整体跳过拷贝：
	 *   1) 旧的运行期魔数检查已失效——bootloader 根本不写魔数，
	 *      "if(*0x20000000==0xDEADBEEF) return" 永远落空，会继续往下拷贝；
	 *   2) __c_init 的拷贝循环是冷代码(不在 I-Cache)，从 Flash 取指(IBus)
	 *      同时读 .data LMA(SBus) 会在单口 XIP Flash 上互斥死锁，
	 *      日志停在 'B' 之后再无输出。
	 * .data/.bss 已由 bootloader 备好，这里直接返回。 */
	return;
#else
	/* 独立启动(无 bootloader)：自行拷贝 .data、清零 .bss。
	 * 此路径下 __init 会先调用 EnableIDCache() 开启 I-Cache，
	 * 拷贝循环取指走 Cache、读 .data 走 SBus，不会死锁。 */
	extern char _end;
	extern char __bss_start;
	extern char __data_lmastart;
	extern char __data_start;
	extern char _edata;
	int size;

	/* Copy data section to RAM */
	size = &_edata - &__data_start;
	MEMCPY(&__data_start, &__data_lmastart, size);

	/* Clear bss section */
	size = &_end - &__bss_start;
	MEMSET(&__bss_start, 0, size);
	return;
#endif
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
	/* set EVIC, vector size: 4 bytes, base: 0x0 */
	__nds32__mtsr(0x1<<13, NDS32_SR_IVB);
#else
# if defined(USE_C_EXT)
	/* If we use v3/v3m toolchain and want to use
	 * C extension please use USE_C_EXT in CFLAGS
	 */
#ifdef __NDS32_ISA_V3__
	/* set IVIC, vector size: 4 bytes, base: 0x0 */
	__nds32__mtsr(0x0, NDS32_SR_IVB);
#else
	/* set IVIC, vector size: 16 bytes, base: 0x0 */
	__nds32__mtsr(0x1<<14, NDS32_SR_IVB);
#endif
# else
	/* set IVIC, vector size: 4 bytes
	 * Base = __executable_start (from linker script).
	 * APP linked at 0x040000 → IVB=0x40000 so interrupts
	 * use the APP's vector table, not the bootloader's.
	 * With IVB=0 the first interrupt after vTaskStartScheduler()
	 * enters the bootloader's vector table and crashes. */
	{
		extern char __executable_start;
		__nds32__mtsr((uint32_t)&__executable_start & 0xFFFF0000UL, NDS32_SR_IVB);
	}
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

	__nds32__mtsr(__nds32__mfsr(NDS32_SR_INT_PEND2), NDS32_SR_INT_PEND2);  //清除pending

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

/* app_diag_putc / app_diag_puthex 已上移到文件前部（ExceptionCommHandler 之前），
 * 以便异常处理器也能用裸 MMIO UART 打印故障码。 */

void __init()
{
/*----------------------------------------------------------
   !!  Users should NOT add any code before this comment  !!
------------------------------------------------------------*/
	app_diag_putc('A');
	app_diag_putc('k');         /* ★固件版本标记：APP 入口第二个字符。看到 'k'=烧的是最新编译；旧固件是 'Ap' 没有 'k'★ */
	__cpu_init();               /* 重定向 IVB→APP 向量表(0x040000)、配置 PSW/FPU */
	app_diag_putc('p');         /* __cpu_init 完成 */

	/* ★临时诊断：定位 "p 之后无 B" 的卡死点（定位后可删除）★
	 * 预期正常字符流：p 1 <IVB> <MMU_CTL> <PSW> 2 B C
	 * 判读：
	 *   停在 p（无 1）     → app_diag_putc 自身卡死（UART 状态位 bit9 不成立）
	 *   IVB != 00040000    → 中断向量未指向 APP，p 之后任何中断都会跳飞
	 *   有 1 无 2          → 卡在寄存器打印（mfsr 取指或 UART 输出）
	 *   有 2 无 B          → p 与 B 之间存在未预期的代码路径 */
	app_diag_putc('1');
	app_diag_puthex(__nds32__mfsr(NDS32_SR_IVB));       /* 期望 00040000 */
	app_diag_puthex(__nds32__mfsr(NDS32_SR_MMU_CTL));
	app_diag_puthex(__nds32__mfsr(NDS32_SR_PSW));
	app_diag_putc('2');

#if !HAS_BOOTLOADER
	/* 仅独立启动（无 bootloader）才自己开 Cache：复位后 Cache 全关，
	 * 且 __c_init 的 .data 拷贝依赖 I-Cache 已开（取指走 Cache、读数据走 SBus
	 * 才不会在单口 XIP Flash 上死锁）。跳转启动时 bootloader 已开好 Cache，
	 * 这里绝不能重复调用（会在冷 XIP 代码里重新编 Cache/TLB 而挂死，见 doc §5）。 */
	EnableIDCache();
#endif
	/* 跳转启动（HAS_BOOTLOADER=1）不调用 EnableIDCache()：
	 * bootloader 的 Boot_JumpTo() Phase 2 已 DataCacheInvalidAll() 并保留
	 * I-Cache 使能，跳转进来时 I/D-Cache 均已开启、SRAM 为 write-through。
	 * 在 APP 冷代码里再次 invalidate+enable Cache/TLB，会在单口 XIP Flash 上
	 * 触发 TLB/Cache 重编程挂死(日志停在 'A' 之后)——这正是 SDK
	 * init-default.c 刻意跳过 EnableIDCache 的原因。 */
#if !HAS_BOOTLOADER
	/* 冷启动(无 bootloader)：CPU 复位后自己建立栈溢出保护。 */
	HardwareStackProtectEnable();
#else
	/* 跳转启动(HAS_BOOTLOADER=1)：跳过 HSP 使能。
	 * 诊断日志 'phS123' 已证明——HSP 的三条 SR 写(清 HSP_CTL、写 SP_BOUND、
	 * 使能 HSP)全部执行完毕('3' 已打出)，但在“使能 HSP”这一步硬件把栈溢出
	 * 检查 arm 起来后，紧接着('B' 之前)就触发一个延迟异常 → 进异常向量
	 * while(1) 挂死(串口表现为 '3' 之后一串乱码再无输出)。
	 * HSP 与 EnableIDCache / Chip_MemInit 同属“冷启动专属的硬件初始化”：
	 * bootloader 自身早已配好并运行过 HSP，且在 Boot_JumpTo Phase 1 主动关掉了它；
	 * APP 在继承来的暖态下重新 arm HSP 会触发上述延迟异常。故跳转路径一律跳过。 */
#endif
	app_diag_putc('B');         /* HSP 处理完成（跳转路径为跳过）*/
	/* Chip_MemInit() 刻意不调用：MPU 已由 bootloader 的 Chip_MemInit() 配好并
	 * 跨跳转保留，SRAM/Flash/外设映射均有效；main() 再按需重配。 */
	__c_init();                 /* HAS_BOOTLOADER 下直接 return，.data/.bss 已备好 */
	app_diag_putc('C');         /* __init 完成，即将进入 main() */
}

__attribute__ ((section(".stub_section"),used)) __attribute__((naked))
void stub(void)
{
__asm__ __volatile__(

    		".long 0x42475046 \n\n"	//0xA4 FW_VALID_MAGIC "BGPF" (bootloader checks this)
    		".long 0xFFFFFFFF \n\n" //0xA8
    		".long 0xFFFFFFFF \n\n" //0xAC
    		".long 0x100000 \n\n"		//0xB0 constant data @ 0x8C
    		".long 0x1D0000 \n\n"		//0xB4 user data
    		".byte 0x01 \n\n"				//0xB8 0x01代表B1X
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
		    ".short 0xFFFF \n\n"	//0x102 pad to align BootInfo at 0x104
		    /* BootInfo_t at 0x104 — bootloader reads this before jump
		     * to copy .data and clear .bss, avoiding NDS32 IBus/SBus
		     * deadlock. Fields use linker-script symbols. */
		    ".long 0x42474F46 \n\n"		//0x104 magic "BGOF"
		    ".long __data_lmastart \n\n"	//0x108 data_lma (.data in flash)
		    ".long __data_start \n\n"		//0x10C data_vma (.data in SRAM)
		    ".long _edata \n\n"			//0x110 data_end
		    ".long __bss_start \n\n"		//0x114 bss_vma
		    ".long _end \n\n"			//0x118 bss_end

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

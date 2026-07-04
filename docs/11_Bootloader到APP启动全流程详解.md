# Bootloader 到 APP 启动全流程详解

> **文档目标**：详细描述 BP10 (NDS32 D1088) SoC 上从上电复位到 BanBox APP 进入 `main()` 的完整流程，包含每一行关键代码的作用、寄存器配置细节、内存布局、错误处理与设计权衡。
> **对应代码**：
> - `bootloader/src/main.c` — Bootloader 主入口
> - `bootloader/src/upgrade.c` — 分区决策、跳转、固件升级协议
> - `bootloader/src/upgrade.h` — 协议与分区布局定义
> - `BanBox/startup/crt0.S` — APP 向量表与启动入口
> - `BanBox/startup/init-default.c` — APP C 初始化（`__init`/`__c_init`/`__cpu_init`/`stub`）
> - `BanBox/nds32-ae210p.ld` — APP 链接脚本
> - `MVsB1_Base_SDK/startup/init-default.c` — SDK 通用初始化版本

---

## 1. 系统总览

### 1.1 Flash 分区布局

```
Flash 地址          大小        分区            说明
───────────────────────────────────────────────────────────────
0x00000000         256 KB      Bootloader      USB CDC 升级 + 跳转引导
0x00040000         2 MB        Partition A     主应用固件 (BanBox APP)
0x00240000         2 MB        Partition B     备份应用固件 (A/B 双区)
0x00440000         4 KB        Partition Flag  PartFlag_t 结构体
0x00441000         ~3.75 MB    System Data     用户参数/音频数据
```

定义见 [`upgrade.h:33-40`](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.h#L33-L40)：
```c
#define BOOTLOADER_SIZE      0x00040000UL  /* 256 KB                         */
#define PART_A_BASE          0x00040000UL  /* Partition A base               */
#define PART_A_SIZE          0x00200000UL  /* Partition A: 2 MB              */
#define PART_B_BASE          0x00240000UL  /* Partition B base               */
#define PART_B_SIZE          0x00200000UL  /* Partition B: 2 MB              */
#define PART_FLAG_ADDR       0x00440000UL  /* Partition flag sector (4 KB)   */
```

### 1.2 启动决策流程图

```
┌─────────────┐
│  上电复位    │  PC = 0x0, IVB = 0x0
│  Boot ROM   │  → 跳转到 Flash 起始处的 Bootloader
└──────┬──────┘
       │
       ▼
┌──────────────────────────────────┐
│  Bootloader main()               │
│  - Chip_Init / Clock / UART / SPI│
│  - Timer2 启动 (1ms tick)        │
└──────┬───────────────────────────┘
       │
       ▼
┌──────────────────────────────────┐
│  Boot_CheckAndJumpIfNeeded()     │
│  1. 读取 PartFlag @ 0x440000     │
│  2. 判断 active_part / fail_cnt  │
│  3. 选定 jump_addr (A=0x40000)   │
│  4. 校验 FW_VALID_MAGIC @0xA4    │
└──────┬───────────────────────────┘
       │
       ├── 找到有效固件 ────► Boot_JumpTo(0x40000)
       │                         │
       │                         ▼
       │                    Phase 1: 关中断/停 Timer
       │                    Phase 2: D-Cache Invalidate
       │                    Phase 3: 复制 .data / 清 .bss
       │                            写 HANDOFF_MAGIC @0x20000000
       │                    Phase 4: 跳转到 0x40000 (APP 入口)
       │                         │
       │                         ▼
       │                    APP crt0.S _start
       │                    → ___start → __init()
       │                    → __cpu_init (设 IVB=0x40000)
       │                    → __c_init (检测 HANDOFF, 跳过 copy)
       │                    → main()
       │
       └── 无有效固件 ─────► UpgradeTask (USB CDC 升级模式)
```

---

## 2. Bootloader 启动阶段

### 2.1 硬件初始化 — `main()` 

代码位置：[`bootloader/src/main.c:131-180`](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/main.c#L131-L180)

```c
int main(void) {
    Chip_Init(1);                                    // ① 芯片基础初始化
    WDG_Disable();                                   // ② 关闭看门狗

    Clock_Module1Enable(ALL_MODULE1_CLK_SWITCH);     // ③ 开启所有模块时钟
    Clock_Module2Enable(ALL_MODULE2_CLK_SWITCH);
    Clock_Module3Enable(ALL_MODULE3_CLK_SWITCH);

    Clock_Config(1, 24000000);                       // ④ 配置主时钟 24MHz
    Clock_PllLock(288000);                           // ⑤ PLL 锁定到 288MHz
    Clock_APllLock(240000);                          // ⑥ APLL 锁定到 240MHz

    Clock_SysClkSelect(PLL_CLK_MODE);                // ⑦ 系统时钟切到 PLL
    Clock_UARTClkSelect(APLL_CLK_MODE);              // ⑧ UART 时钟用 APLL
    Clock_Timer3ClkSelect(SYSTEM_CLK_MODE);

    Clock_USBClkDivSet(4);                           // ⑨ USB 时钟分频
    Clock_USBClkSelect(APLL_CLK_MODE);

    GPIO_PortAModeSet(GPIOA9, 1);    /* UART1 RX  */ // ⑩ 配置 UART 引脚
    GPIO_PortAModeSet(GPIOA10, 3);   /* UART1 TX  */
    DbgUartInit(1, 115200, 8, 0, 1);                 // ⑪ 调试串口 115200

    Remap_InitTcm(0, 12);                            // ⑫ 初始化 TCM
    SpiFlashInit(80000000, MODE_4BIT, 0, 1);         // ⑬ SPI Flash 80MHz 4线
    DMA_ChannelAllocTableSet(DmaChannelMap);         // ⑭ DMA 通道表

    GIE_ENABLE();                                    // ⑮ 开全局中断

    Timer_Config(TIMER2, 1000, 0);                   // ⑯ Timer2 = 1ms tick
    Timer_Start(TIMER2);
    NVIC_EnableIRQ(Timer2_IRQn);

    DBG("...BG_CARD SDK Bootloader V0.2.1...\n");

    Boot_CheckAndJumpIfNeeded();                     // ⑰ 启动决策（可能不返回）

    /* 无有效固件 — 进入升级模式 */
    prvInitialiseHeap();
    NVIC_EnableIRQ(SWI_IRQn);
    xTaskCreate(UpgradeTask, "UpgTask", 2048, NULL, 1, NULL);
    vTaskStartScheduler();
    while (1);
}
```

**DMA 通道分配表**（[main.c:65-93](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/main.c#L65-L93)）：bootloader 只用 UART1 RX/TX (DMA6/7)，所有音频 DMA 置 255（未用）。

**Timer2 ISR**（[main.c:54-57](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/main.c#L54-L57)）：
```c
void Timer2Interrupt(void) {
    Timer_InterruptFlagClear(TIMER2, UPDATE_INTERRUPT_SRC);
    OTG_PortLinkCheck();    // 仅做 USB 端口检测，不处理音频
}
```

### 2.2 启动决策 — `Boot_CheckAndJumpIfNeeded()`

代码位置：[`upgrade.c:181-249`](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.c#L181-L249)

#### 2.2.1 读取分区标志

```c
int PartFlag_Read(PartFlag_t *flag)
{
    memcpy(flag, (const void *)PART_FLAG_ADDR, sizeof(PartFlag_t));
    return part_flag_valid(flag) ? 1 : 0;
}
```

`PartFlag_t` 结构（[upgrade.h:96-104](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.h#L96-L104)）：
```c
typedef struct {
    uint32_t magic;           /* PART_FLAG_MAGIC = "BGPW" (0x42475057) */
    uint8_t  active_part;     /* 0 = A 激活, 1 = B 激活              */
    uint8_t  reserved1;
    uint8_t  boot_fail_cnt;   /* 启动失败计数，达到 BOOT_FAIL_MAX(3) 切换 */
    uint8_t  reserved2;
    uint32_t crc32;           /* 前 12 字节的 CRC32                  */
} PartFlag_t;
```

CRC32 校验算法（IEEE 802.3，poly=0xEDB88320）见 [upgrade.c:43-54](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.c#L43-L54)。

#### 2.2.2 决策逻辑

```c
if (!PartFlag_Read(&flag)) {
    /* 首次开机或标志损坏 → 默认启动 A */
    jump_addr = PART_A_BASE;            // 0x40000
} else {
    /* 正常启动 */
    jump_addr = (flag.active_part == 1) ? PART_B_BASE : PART_A_BASE;
    
    if (flag.boot_fail_cnt >= BOOT_FAIL_MAX) {
        /* 当前分区失败 3 次 → 切换到另一分区 */
        flag.active_part ^= 1;
        flag.boot_fail_cnt = 1;
        PartFlag_Write(&flag);
        jump_addr = (flag.active_part == 1) ? PART_B_BASE : PART_A_BASE;
    } else {
        /* 递增失败计数（APP 运行成功后会清零）*/
        flag.boot_fail_cnt++;
        PartFlag_Write(&flag);
    }
}
```

#### 2.2.3 固件有效性校验

检查 APP 偏移 `0xA4` 处的 magic 字段（[`upgrade.h:62-63`](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.h#L62-L63)）：
```c
#define FW_VALID_MAGIC        0x42475046UL  /* "BGPF" */
#define FW_VALID_MAGIC_OFFSET 0x000000A4UL  /* 分区内的偏移 */
```

```c
volatile const uint32_t *magic_ptr =
    (volatile const uint32_t *)(jump_addr + FW_VALID_MAGIC_OFFSET);

if (*magic_ptr != FW_VALID_MAGIC) {
    /* 尝试回退到 A 分区，仍无效则留在 bootloader */
    if (jump_addr != PART_A_BASE) {
        flag.active_part = 0;
        PartFlag_Write(&flag);
        jump_addr = PART_A_BASE;
        magic_ptr = (volatile const uint32_t *)
                    (PART_A_BASE + FW_VALID_MAGIC_OFFSET);
    }
    if (*magic_ptr != FW_VALID_MAGIC) {
        return;    /* 无有效固件，留在 bootloader 升级模式 */
    }
}
```

**此 magic 由 APP 在 `stub()` 函数中静态嵌入**，见第 3.1 节。

#### 2.2.4 Partition B 地址重映射

如果启动 B 分区，需要硬件 remap 将 0x40000 映射到 0x240000：
```c
if (jump_addr == PART_B_BASE) {
    Remap_AddrRemapSet(ADDR_REMAP0, PART_A_BASE, PART_B_BASE,
                       (uint32_t)(PART_A_SIZE / 1024UL));
    jump_addr = PART_A_BASE;    /* 跳转地址统一为 0x40000 */
}
```
这样 APP 代码无需感知自己位于 B 分区，链接脚本始终以 0x40000 为基址。

---

## 3. Bootloader 跳转阶段 — `Boot_JumpTo()`

代码位置：[`upgrade.c:139-168`](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.c#L139-L168)

跳转分 4 个 Phase，每个 Phase 通过 `diag_putc()` 输出诊断字符（直接写 UART1 MMIO，绕过驱动状态）：

| 字符 | 含义 |
|------|------|
| `J`  | 即将跳转（Phase 1 入口）|
| `P`  | Phase 3 入口（开始复制 .data）|
| `d`  | .data 已复制到 SRAM |
| `z`  | .bss 已清零 |
| `H`  | HANDOFF_MAGIC 已写入 0x20000000 |
| `?`  | BootInfo magic 校验失败 |

### 3.1 Phase 1: 静默所有硬件

```c
WDG_Disable();                                    // 关看门狗

Timer_Pause(TIMER2, 1);                           // 停止 Timer2
Timer_InterruptFlagClear(TIMER2, UPDATE_INTERRUPT_SRC);

__nds32__mtsr(0x0, NDS32_SR_INT_MASK2);           // 屏蔽所有 NVIC 中断
__nds32__setgie_dis();                            // 关全局中断 (GIE=0)
__nds32__dsb();                                   // 同步屏障
```

### 3.2 Phase 2: 失效 D-Cache，保留 I-Cache

```c
DataCacheInvalidAll();
```

**关键**：不能失效 I-Cache。因为 bootloader 代码本身可能正在 I-Cache 中执行，失效会导致取指从 Flash 重新读取，与后续 .data 复制操作产生 SBus/IBus 冲突。

### 3.3 Phase 3: 复制 .data + 清 .bss（关键设计）

#### 3.3.1 为什么需要 Bootloader 预复制？

**NDS32 BP10 的 SBus/IBus 互斥问题**：
- Flash 控制器有两条总线：IBus（取指）和 SBus（数据读写）
- 两者**互斥**，不能同时访问 Flash
- APP 启动时若自己复制 .data（`memcpy(&__data_start, &__data_lmastart, size)`），CPU 同时在 Flash 取指 + 读 .data，会导致总线死锁

**解决方案**：bootloader 在跳转前预复制 .data，并通过 SRAM magic 通知 APP 跳过复制。

#### 3.3.2 BootInfo 结构

APP 通过 `stub()` 函数在链接时静态嵌入 BootInfo（[`init-default.c:370-410`](file:///e:/project_and_dataset/project/BG_Audio_Looper/BanBox/startup/init-default.c#L370-L410)）：

```c
__attribute__((section(".stub_section"), used)) __attribute__((naked))
void stub(void)
{
__asm__ __volatile__(
    ".long 0x42475046 \n\n"     //0xA4  FW_VALID_MAGIC "BGPF"
    /* ... 其他 stub 字段 ... */
    ".short 0xFFFF \n\n"        //0x102 pad
    /* BootInfo_t at 0x104 */
    ".long 0x42474F46 \n\n"     //0x104 magic "BGOF"
    ".long __data_lmastart \n\n" //0x108 data_lma (.data 在 Flash 中的地址)
    ".long __data_start \n\n"    //0x10C data_vma (.data 在 SRAM 中的地址)
    ".long _edata \n\n"          //0x110 data_end
    ".long __bss_start \n\n"     //0x114 bss_vma
    ".long _end \n\n"            //0x118 bss_end
);
}
```

BootInfo 结构定义（[`upgrade.h:73-81`](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.h#L73-L81)）：
```c
#define BOOT_INFO_MAGIC       0x42474F46UL  /* "BGOF" */
#define BOOT_INFO_OFFSET      0x00000104UL  /* 分区内的偏移 */

typedef struct {
    uint32_t magic;
    uint32_t data_lma;       /* .data Load Memory Address (Flash) */
    uint32_t data_vma;       /* .data Virtual Memory Address (SRAM) */
    uint32_t data_end;       /* .data 结束 VMA */
    uint32_t bss_vma;        /* .bss VMA */
    uint32_t bss_end;        /* .bss 结束 VMA */
} BootInfo_t;
```

#### 3.3.3 复制实现

```c
const BootInfo_t *info = (const BootInfo_t *)(addr + BOOT_INFO_OFFSET);

if (info->magic == BOOT_INFO_MAGIC) {
    uint32_t i;
    uint32_t nwords;
    volatile uint32_t *dst;
    const volatile uint32_t *src;

    /* 复制 .data: Flash → SRAM */
    nwords = (info->data_end - info->data_vma + 3u) / 4u;
    dst    = (volatile uint32_t *)info->data_vma;
    src    = (const volatile uint32_t *)info->data_lma;
    for (i = 0; i < nwords; i++)
        dst[i] = src[i];

    /* 清 .bss */
    nwords = (info->bss_end - info->bss_vma + 3u) / 4u;
    dst    = (volatile uint32_t *)info->bss_vma;
    for (i = 0; i < nwords; i++)
        dst[i] = 0u;

    /* 写 handoff magic，通知 APP 跳过复制 */
    *(volatile uint32_t *)BOOT_HANDOFF_ADDR = BOOT_HANDOFF_MAGIC;
}
```

**Handoff 约定**（[`upgrade.h:84-85`](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.h#L84-L85)）：
```c
#define BOOT_HANDOFF_ADDR     0x20000000UL     /* SRAM 起始 */
#define BOOT_HANDOFF_MAGIC    0xDEADBEEFUL
```

### 3.4 Phase 4: 跳转到 APP

```c
{
    typedef void (*Entry_t)(void);
    Entry_t entry = (Entry_t)addr;    // addr = 0x40000
    diag_putc('J');
    entry();                          // 函数指针调用 → PC = 0x40000
}
while (1);    /* 永不到达 */
```

跳转到 0x40000 即 APP 的 `.vector` 段起始，第一条指令是 `j ___start`（见第 4 节）。

---

## 4. APP 启动阶段

### 4.1 向量表 — `crt0.S`

代码位置：[`BanBox/startup/crt0.S:25-44`](file:///e:/project_and_dataset/project/BG_Audio_Looper/BanBox/startup/crt0.S#L25-L44)

```asm
.section .vector, "ax"
.align 2
exception_vector:
_start:
    j ___start                    !  (0) Trap Reset — 跳转到 C 启动代码
    vector TLB_Fill               !  (1) Trap TLB fill
    vector PTE_Not_Present        !  (2) Trap PTE not present
    vector TLB_Misc               !  (3) Trap TLB misc
    vector TLB_VLPT_Miss          !  (4) Trap TLB VLPT miss
    vector Machine_Error          !  (5) Trap Machine error
    vector Debug_Related          !  (6) Trap Debug related
    vector General_Exception      !  (7) Trap General exception
    vector Syscall                !  (8) Syscall
    hal_hw_vectors                !  HW 中断向量布局
exception_vector_end:
```

**向量宏**（crt0.S:13-16）：
```asm
.macro vector name
.align 2
j OS_Trap_\name
.endm
```

每个向量条目是**单条 `j` 指令（4 字节）**。这决定了 IVB 寄存器的向量间距配置（见 4.3）。

### 4.2 链接脚本 — APP 基址

[`BanBox/nds32-ae210p.ld:5-9`](file:///e:/project_and_dataset/project/BG_Audio_Looper/BanBox/nds32-ae210p.ld#L5-L9)：
```
PROVIDE (__executable_start = 0x040000);
. = 0x040000;
.vector         : { KEEP(*(.vector)) }
.stub_section   : { KEEP(*(.stub_section)) }
```

APP 始终链接在 0x40000（Partition A 基址），B 分区通过硬件 remap 透明处理。

### 4.3 启动入口 — `___start`

[`crt0.S:345-361`](file:///e:/project_and_dataset/project/BG_Audio_Looper/BanBox/startup/crt0.S#L345-L361)：
```asm
___start:
    nds32_init       ! NDS32 启动宏（不可修改）
    
    movi55 $r0,#0x0
    mtsr $r0,$misc_ctl
    
    bal __init       ! 调用 C 初始化函数
    bal main         ! 调用 main()
1:  b 1b
```

### 4.4 `__init()` — C 初始化总入口

[`init-default.c:354-365`](file:///e:/project_and_dataset/project/BG_Audio_Looper/BanBox/startup/init-default.c#L354-L365)：
```c
void __init()
{
    __cpu_init();                // ① CPU 寄存器配置（IVB 等）
    EnableIDCache();             // ② 使能 I/D-Cache
    HardwareStackProtectEnable();// ③ 硬件栈保护
    /* BanBox: Chip_MemInit() 不在此调用，由 main() 的 Chip_Init(1) 处理 MPU */
    __c_init();                  // ④ 复制 .data + 清 .bss（或跳过）
}
```

### 4.5 `__cpu_init()` — IVB 寄存器配置（关键）

[`init-default.c:225-280`](file:///e:/project_and_dataset/project/BG_Audio_Looper/BanBox/startup/init-default.c#L225-L280)

#### 4.5.1 IVB 寄存器位域

NDS32 IVB (Interrupt Vector Base) 寄存器：
- **Bit 31:16** — 向量基址（必须 64KB 对齐，即低 16 位为 0）
- **Bit 14** — 向量间距：0=4字节，1=16字节
- **Bit 13** — EVIC 模式使能

#### 4.5.2 实际生效的配置路径

BanBox 工程未定义 `CFG_EVIC` 和 `USE_C_EXT`，走 `#else` 路径：
```c
#else
    /* set IVIC, vector size: 4 bytes
     * Base = __executable_start (from linker script).
     * APP linked at 0x040000 → IVB=0x40000 */
    {
        extern char __executable_start;
        __nds32__mtsr((uint32_t)&__executable_start & 0xFFFF0000UL, 
                      NDS32_SR_IVB);
    }
#endif
```

计算结果：
- `&__executable_start` = `0x00040000`
- `& 0xFFFF0000UL` = `0x00040000`（清除低 16 位控制位）
- 即 **base=0x40000, 4字节向量间距**（bit 14=0）

#### 4.5.3 与 BT_Audio_APP 的对比

BT_Audio_APP 使用 `FLASH_BOOT_EN` 宏切换（[`BT_Audio_APP/startup/init-default.c:257`](file:///e:/project_and_dataset/project/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/startup/init-default.c#L257)）：
```c
#if FLASH_BOOT_EN
    __nds32__mtsr(0x10000, NDS32_SR_IVB);   // APP base = 0x10000
#else
    __nds32__mtsr(0x0, NDS32_SR_IVB);       // 独立运行 base = 0x0
#endif
```

BanBox 使用链接脚本符号 `__executable_start` 自动适应，更灵活。

#### 4.5.4 错误教训：IVB=0 导致调度器崩溃

**曾出现的 Bug**：BanBox 早期代码硬编码 `__nds32__mtsr(0x0000, NDS32_SR_IVB)`，导致：
1. `__init()` 阶段中断被禁用，IVB=0 无影响
2. `main()` 初始化期间中断仍禁用，SPI/Flash 初始化正常
3. `vTaskStartScheduler()` 开启中断
4. 第一个 SysTick 中断从 IVB=0x0 读取向量 → 进入 **bootloader 的向量表**（地址 0x0 附近）→ 崩溃

现象：输出 `c[Task] Har` 后死机（`c` 是 `vTaskStartScheduler` 前的诊断字符，任务刚运行一小会儿就被中断触发崩溃）。

### 4.6 `__c_init()` — Handoff 检测

[`init-default.c:186-219`](file:///e:/project_and_dataset/project/BG_Audio_Looper/BanBox/startup/init-default.c#L186-L219)：
```c
void __c_init()
{
    extern char __data_lmastart, __data_start, _edata;
    extern char __bss_start, _end;
    int size;

    /* Bootloader handoff 检测 */
    {
        volatile uint32_t *handoff = (volatile uint32_t *)0x20000000UL;
        if (*handoff == 0xDEADBEEFUL) {
            *handoff = 0;        /* 清除 magic，冷启动仍能正常复制 */
            return;              /* 跳过 .data 复制和 .bss 清零 */
        }
    }

    /* 独立运行模式（无 bootloader）— 自行复制 */
    size = &_edata - &__data_start;
    MEMCPY(&__data_start, &__data_lmastart, size);

    size = &_end - &__bss_start;
    MEMSET(&__bss_start, 0, size);
}
```

**设计要点**：
- bootloader 预复制后写 `0xDEADBEEF` 到 0x20000000
- APP 检测到 magic → 跳过复制 → 避免 SBus/IBus 死锁
- 清除 magic → 冷启动（无 bootloader）时仍能自行复制

### 4.7 APP main() 执行

`__init()` 返回后，crt0.S 调用 `bal main`，进入 BanBox 的 `main()` 函数。此时：
- IVB = 0x40000（中断使用 APP 向量表）
- .data 已在 SRAM，.bss 已清零
- I/D-Cache 已使能
- 中断仍关闭（PSW.INTL=3，由 `vTaskStartScheduler()` 开启）

---

## 5. USB CDC 升级模式（无有效固件时）

### 5.1 UpgradeTask

[`main.c:99-121`](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/main.c#L99-L121)：
```c
static void UpgradeTask(void)
{
    /* 与 BanBox 一致的 AUDIO_MIC_CDC 复合设备模式 */
    OTG_DeviceModeSel(AUDIO_MIC_CDC, 0x1234, 0x1234);
    UsbDevicePlayInit();
    UsbDeviceEnable();    /* 内部调用 OTG_DeviceInit() + NVIC_EnableIRQ(Usb_IRQn) */

    Upgrade_Init();

    while (1) {
        OTG_DeviceRequestProcess();   // USB 枚举/控制请求
        OTG_DeviceCDC_Task();         // CDC RX/TX 环形缓冲
        Upgrade_Process();            // 升级状态机
    }
}
```

### 5.2 升级协议

**帧格式**（[`upgrade.h:23-27`](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.h#L23-L27)）：
```
[SOF:1][CMD:1][SEQ:2][LEN:2][DATA:len][CRC16:2]
SOF = 0xAA, 多字节字段大端序
CRC16-CCITT (poly=0x1021, init=0xFFFF) 覆盖 CMD+SEQ+LEN+DATA
```

**命令集**（[`upgrade.h:65-73`](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.h#L65-L73)）：
| CMD | 值 | 说明 |
|-----|----|----|
| SYNC | 0x01 | 握手，返回协议版本 |
| START | 0x02 | 开始升级会话 |
| DATA | 0x03 | 数据块（最大 256 字节）|
| FINISH | 0x04 | 结束会话 |
| JUMP | 0x05 | 跳转到新固件 |
| ERASE | 0x06 | 擦除指定区域 |
| QUERY_INFO | 0x07 | 查询设备/分区信息 |
| SET_PART | 0x08 | 设置激活分区 |
| REBOOT | 0x09 | 重启 |

升级始终写入 Partition B，然后设置 `active_part=1`，下次启动切换到 B（见 [upgrade.c:587](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.c#L587)）。

---

## 6. 关键设计权衡与坑点

### 6.1 SBus/IBus 死锁问题

**问题**：NDS32 BP10 的 Flash 控制器 IBus（取指）和 SBus（数据）互斥，APP 自行复制 .data 时会死锁。

**解决方案**：
- Bootloader 在 Phase 3 预复制 .data（此时 bootloader 代码已在 I-Cache）
- 通过 SRAM magic (0xDEADBEEF @ 0x20000000) 通知 APP 跳过复制
- APP 的 `__c_init()` 检测 magic，若存在则直接 return

### 6.2 IVB 寄存器配置陷阱

**陷阱**：APP 链接在 0x40000，若 IVB 仍设为 0x0，中断会进入 bootloader 向量表。

**正确做法**：
```c
__nds32__mtsr((uint32_t)&__executable_start & 0xFFFF0000UL, NDS32_SR_IVB);
```
- `&__executable_start` = 0x40000
- `& 0xFFFF0000UL` 清除低 16 位控制位，保留 64KB 对齐的基址
- bit 14=0 → 4字节向量间距（匹配 crt0.S 的单条 `j` 指令布局）

### 6.3 Partition B 的透明重映射

APP 链接脚本固定以 0x40000 为基址，B 分区（0x240000）通过硬件 remap 映射到 0x40000：
```c
Remap_AddrRemapSet(ADDR_REMAP0, PART_A_BASE, PART_B_BASE, PART_A_SIZE/1024);
```
这样 APP 代码、BootInfo 读取、跳转地址都无需感知分区差异。

### 6.4 诊断字符机制

`Boot_JumpTo()` 中使用 `diag_putc()` 直接写 UART1 MMIO（[upgrade.c:35-40](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.c#L35-L40)）：
```c
#define DIAG_UART1_STATUS  (*(volatile uint32_t *)0x40006014)
#define DIAG_UART1_TX      (*(volatile uint32_t *)0x40006018)
static inline void diag_putc(char c)
{
    while (!(DIAG_UART1_STATUS & (1u << 9))) ;  /* 等 TX FIFO 就绪 */
    DIAG_UART1_TX = (uint32_t)(unsigned char)c;
}
```

**用途**：在 .data/.bss 未初始化、中断已关闭的早期阶段输出诊断字符，绕过 DBG/printf 驱动状态。

实际串口输出示例（成功启动）：
```
[BOOT] Jumping to 0x00040000 ...
JPHJABCDEcr...
```
| 字符 | 阶段 |
|------|------|
| J | Boot_JumpTo 入口 |
| P | Phase 3 开始 |
| H | Handoff magic 已写 |
| (跳转到 APP) | |
| A | APP __cpu_init 完成 |
| B | APP __c_init 完成（跳过复制）|
| C | APP EnableIDCache 完成 |
| D | APP __init 返回 |
| E | APP 进入 main() |
| c | vTaskStartScheduler 前 |
| r | 调度器运行（任务开始）|

### 6.5 BootInfo 偏移设计

```
分区基址 (0x40000)
    │
    ├── 0x000: .vector (crt0.S 向量表)
    ├── 0x024: .stub_section (stub 函数)
    │       └── 0xA4: FW_VALID_MAGIC "BGPF"  ← bootloader 校验
    │       └── 0x104: BootInfo_t "BGOF"     ← bootloader 读取
    │           ├── 0x108: data_lma
    │           ├── 0x10C: data_vma
    │           ├── 0x110: data_end
    │           ├── 0x114: bss_vma
    │           └── 0x118: bss_end
    ├── 0x11C: .text (代码段)
    └── ...
```

`stub()` 函数用 `__attribute__((naked))` + 内联汇编精确控制每个字段的偏移，确保 bootloader 能通过固定偏移读取。

---

## 7. 完整时序图

```
Bootloader                                  APP (0x40000)
─────────                                   ────────────
main()
  Chip_Init(1)
  Clock_Config(PLL 288MHz)
  DbgUartInit(115200)
  SpiFlashInit(80MHz, 4-bit)
  Timer2 启动 (1ms)
  GIE_ENABLE()
  │
  Boot_CheckAndJumpIfNeeded()
    PartFlag_Read @ 0x440000
    校验 FW_VALID_MAGIC @ 0x400A4
    (B 分区: Remap 0x40000→0x240000)
    │
    Boot_JumpTo(0x40000)
      Phase 1: WDG_Disable, Timer_Pause
               INT_MASK2=0, GIE=0
               diag_putc('J')
      Phase 2: DataCacheInvalidAll
      Phase 3: 读 BootInfo @ 0x40104
               校验 "BGOF"
               复制 .data (Flash→SRAM)
                 diag_putc('P','d')
               清 .bss
                 diag_putc('z')
               写 0xDEADBEEF @ 0x20000000
                 diag_putc('H')
      Phase 4: entry = 0x40000
               diag_putc('J')
               entry() ──────────────────►  _start (0x40000)
                                            j ___start
                                              nds32_init
                                              __init():
                                                __cpu_init()
                                                  IVB = 0x40000 ◄── 关键
                                                  INT_MASK=0
                                                  PSW.INTL=0
                                                EnableIDCache()
                                                HardwareStackProtectEnable()
                                                __c_init():
                                                  读 0x20000000
                                                  == 0xDEADBEEF? YES
                                                  清 0x20000000 = 0
                                                  return (跳过复制)
                                              main()
                                                ...初始化各模块...
                                                vTaskStartScheduler()
                                                  GIE=1, 中断开启
                                                  第一个 SysTick
                                                  → IVB=0x40000
                                                  → APP 向量表
                                                  → SystickInterrupt
                                                  ✓ 正常运行
```

---

## 8. 调试指南

### 8.1 串口诊断字符解读

| 输出 | 含义 | 故障排查 |
|------|------|---------|
| 无输出 | Bootloader 未运行 | 检查 Boot ROM 跳转、Flash 0x0 内容 |
| `BG_CARD SDK...` 但无 `Jumping` | PartFlag/FW_VALID 校验失败 | 检查 0x400A4 是否为 "BGPF" |
| `Jumping...J` 后无 `P` | Phase 1/2 卡死 | 检查 Timer/Cache 配置 |
| `P?` | BootInfo magic 不匹配 | 检查 stub() 函数偏移、链接脚本 |
| `PHJABCDEc` 后死机 | 调度器启动后崩溃 | **检查 IVB 是否=0x40000**（最常见）|
| `PHJABCDEcr` | 正常运行 | — |

### 8.2 关键地址速查

| 地址 | 含义 |
|------|------|
| 0x000000 | Bootloader 起始 |
| 0x040000 | Partition A / APP 链接基址 |
| 0x0400A4 | FW_VALID_MAGIC "BGPF" |
| 0x040104 | BootInfo_t "BGOF" |
| 0x240000 | Partition B 基址 |
| 0x440000 | PartFlag_t |
| 0x20000000 | SRAM 起始 / HANDOFF_MAGIC |
| 0x40006014 | UART1 STATUS 寄存器 |
| 0x40006018 | UART1 TX 寄存器 |

### 8.3 常见故障与修复

#### 故障 1：调度器启动后立即崩溃（`c[Task] Har` 后死机）

**原因**：IVB 寄存器未指向 APP 向量表。

**修复**：检查 [`init-default.c`](file:///e:/project_and_dataset/project/BG_Audio_Looper/BanBox/startup/init-default.c) 中的 IVB 配置，确保：
```c
__nds32__mtsr((uint32_t)&__executable_start & 0xFFFF0000UL, NDS32_SR_IVB);
```
而不是 `__nds32__mtsr(0x0000, NDS32_SR_IVB)`。

#### 故障 2：APP 启动时 SBus/IBus 死锁

**原因**：APP 自行复制 .data 时与取指冲突。

**修复**：确认 bootloader Phase 3 已预复制 .data 并写 HANDOFF_MAGIC，APP 的 `__c_init()` 检测 magic 跳过复制。

#### 故障 3：B 分区无法启动

**原因**：未配置硬件 remap。

**修复**：检查 `Boot_CheckAndJumpIfNeeded()` 中 B 分区的 `Remap_AddrRemapSet()` 调用。

---

## 附录 A：相关文件索引

| 文件 | 作用 |
|------|------|
| [bootloader/src/main.c](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/main.c) | Bootloader 主入口、硬件初始化 |
| [bootloader/src/upgrade.c](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.c) | 分区决策、跳转、升级协议 |
| [bootloader/src/upgrade.h](file:///e:/project_and_dataset/project/BG_Audio_Looper/bootloader/src/upgrade.h) | 协议定义、分区布局、结构体 |
| [BanBox/startup/crt0.S](file:///e:/project_and_dataset/project/BG_Audio_Looper/BanBox/startup/crt0.S) | APP 向量表、启动入口 |
| [BanBox/startup/init-default.c](file:///e:/project_and_dataset/project/BG_Audio_Looper/BanBox/startup/init-default.c) | APP C 初始化（__init/__c_init/__cpu_init/stub）|
| [BanBox/nds32-ae210p.ld](file:///e:/project_and_dataset/project/BG_Audio_Looper/BanBox/nds32-ae210p.ld) | APP 链接脚本 |
| [MVsB1_Base_SDK/startup/init-default.c](file:///e:/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/startup/init-default.c) | SDK 通用初始化版本 |

## 附录 B：NDS32 关键寄存器

| 寄存器 | 作用 | 关键位 |
|--------|------|--------|
| IVB | 中断向量基址 | Bit31:16=基址, Bit14=向量间距, Bit13=EVIC |
| PSW | 程序状态字 | Bit2:1=INTL (中断嵌套层级) |
| INT_MASK | 中断屏蔽 | Bit N=1 屏蔽 HW中断 N |
| INT_MASK2 | NVIC 中断屏蔽 | 全 0=屏蔽所有 NVIC 中断 |
| MMU_CTL | MMU 控制 | Bit23=某使能位 |
| MISC_CTL | 杂项控制 | — |

---

**文档版本**：V1.0  
**最后更新**：2026-07-04  
**对应代码版本**：Bootloader V0.2.1, BanBox APP V0.2.1

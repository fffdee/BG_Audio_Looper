# Bootloader → APP 跳转协议与移植对齐指南

> **文档目标**：以 BanBox（已验证可跳）为基准，整理 Bootloader 与 APP 之间的跳转契约（镜像格式、BootInfo、诊断字符、双方职责），作为任何新 APP 工程（如 BanDataHub）接入 bootloader 的对齐依据与验收清单。
>
> 启动全流程的逐步讲解见 [`11_Bootloader到APP启动全流程详解.md`](./11_Bootloader到APP启动全流程详解.md)，本文只聚焦**双方接口契约**与**移植验收**。
>
> **对应代码**：
> - `bootloader/src/upgrade.c` — `Boot_JumpTo()` / `Boot_CheckAndJumpIfNeeded()`
> - `bootloader/src/upgrade.h` — 分区布局、`BootInfo_t`、魔数定义
> - `BanBox/startup/init-default.c` — APP 侧 `stub()` / `__init` / `__c_init`（基准实现）
> - `BanBox/nds32-ae210p.ld` — APP 链接脚本（基准）
> - `BanBox/src/main.c` — `HAS_BOOTLOADER` 启动分支（基准）

---

## 1. Flash 分区布局（跳转的物理基础）

### 1.1 8 MB Flash — 双分区（BanBox）

```
0x000000 ─ 0x03FFFF   Bootloader        256 KB
0x040000 ─ 0x23FFFF   Partition A       2 MB   ← APP 链接基址
0x240000 ─ 0x43FFFF   Partition B       2 MB   （硬件 remap 0x040000→0x240000）
0x440000 ─ 0x440FFF   Partition flags   4 KB   PartFlag_t
0x441000 ─ 0x7FFFFF   System data       ~3.75 MB
```

### 1.2 2 MB 内部 ROM — 单分区（BanDataHub，bootloader 运行时自动降级）

```
0x000000 ─ 0x03FFFF   Bootloader        256 KB
0x040000 ─ 0x1FEFFF   Partition A       ~1.75 MB ← APP 链接基址（不变！）
0x1FF000 ─ 0x1FFFFF   Partition flags   4 KB（末扇区）
（无 Partition B，升级直接覆写 A；bootloader 日志 caps=0x04 即 SINGLE-PARTITION）
```

**要点**：无论 Flash 多大，APP 一律链接在 `0x040000`。分区大小变化只影响
bootloader 运行时的 `PartFlag` 地址与升级写入范围，不影响跳转协议。

---

## 2. APP 镜像契约（stub_section 头部格式）

APP 镜像前 0x120 字节是与 bootloader 的**二进制接口**，由
`startup/init-default.c` 中的 `stub()`（`.stub_section`）生成，链接脚本保证其
位于 `.vector`（0x0～0xA3）之后：

| 偏移 | 内容 | 值 | 谁使用 |
|---|---|---|---|
| 0x000 | `.vector` 向量表 | — | CPU（IVB 指向 0x040000） |
| 0x0A4 | `FW_VALID_MAGIC` | `0x42475046` "BGPF" | bootloader 判定固件有效 |
| 0x0B0 | constant data 指针 | `0x100000` | SDK Flash Boot 烧录工具 |
| 0x0B4 | user data 指针 | `0x1D0000` | SDK Flash Boot 烧录工具 |
| 0x0C0 | magic number | `0xB0BEBDC9` | SDK Flash Boot 烧录工具 |
| 0x104 | `BootInfo_t.magic` | `0x42474F46` "BGOF" | bootloader 跳转前读取 |
| 0x108 | `data_lma` | `__data_lmastart` | .data 在 Flash 中的加载地址 |
| 0x10C | `data_vma` | `__data_start` | .data 在 SRAM 中的运行地址 |
| 0x110 | `data_end` | `_edata` | .data 结束 VMA |
| 0x114 | `bss_vma` | `__bss_start` | .bss 起始 VMA |
| 0x118 | `bss_end` | `_end` | .bss 结束 VMA |

`BootInfo_t` 定义见 [`upgrade.h:62-73`](../bootloader/src/upgrade.h)。

### 2.1 链接脚本硬性要求（★ 最易踩坑）

`nds32-ae210p.ld` 必须同时满足：

```ld
PROVIDE (__executable_start = 0x040000);
NDS_SAG_LMA_EILM = 0x040000 ;
. = 0x040000;                       /* VMA 链从 0x040000 开始 */
```

并且 **LMA 链也必须锚定在 0x040000**：代码段之后所有 `AT(ALIGN(LOADADDR(前一段)+SIZEOF(前一段),…))`
的加载地址会自然跟随代码段末尾（≈0x040000+代码大小）。

> **实测对照（两者均正确）**：
> - BanBox：代码段 LMA `0x00040000` + `0x19E9CC` → `data_lma=0x001DE9E0` ✔
> - BanDataHub：代码段 LMA `0x00040000` + `0xAE55C` → `data_lma=0x000EE560` ✔
>
> 两者 `.ld` 的 LMA 链均已正确锚定在 0x040000，`.data` 加载地址紧跟代码段末尾。
> **BanDataHub 的跳转故障与链接脚本无关**，根因在 APP 启动代码（见 §5）。

**验收标准**：`data_lma` 必须落在 `[0x040000, 分区末地址]` 区间内，且约等于
`0x040000 + 代码段大小`。

---

## 3. Bootloader 侧：`Boot_JumpTo()` 四阶段与诊断字符

bootloader 通过 UART1 裸寄存器输出单字符诊断码（不依赖 printf），
串口日志字符序列即跳转过程的"黑匣子"：

| 字符 | 阶段 | 含义 |
|---|---|---|
| `P` | Phase 3 进入 | 开始处理 BootInfo |
| `d` | Phase 3 | .data 已从 Flash(LMA) 拷贝到 SRAM(VMA) |
| `z` | Phase 3 | .bss 已清零 |
| `E` | Phase 3 | **BootInfo 的 SRAM 范围非法**（越界 0x20000000~0x20048000） |
| `H` | Phase 3 完成 | BootInfo 处理结束 |
| `?` | Phase 3 | **未找到 BGOF 魔数**，APP 需自行初始化 .data/.bss |
| `J` | Phase 4 | 即将 `entry()` 跳转到 APP |

跳转前 bootloader 留下的硬件状态（Phase 1/2，APP 可依赖）：

1. **HSP 关闭**（硬件栈保护，避免 APP 早期访问 0x20000000 触发异常）；
2. WDG 关闭、Timer2 暂停、NVIC 全部中断源屏蔽（`INT_MASK2=0`）、GIE 关闭；
3. **D-Cache 已无效化，I-Cache 保留**；
4. 时钟/PLL/UART1(115200)/SPI Flash XIP(80MHz,4bit)/TCM(`Remap_InitTcm(0,12)`)/DMA 通道表 **保持配置好**；
5. `Remap_AddrRemap*` 已按分区决策设置（双分区 B 激活时 0x040000→0x240000）。

**注意**：bootloader **不会**向 `0x20000000` 写 handoff 魔数 `0xDEADBEEF`
（历史上写入会停在 `Pdz`，见 `upgrade.c:337` 注释）。APP 侧不能依赖该魔数
必然存在，见 §4.2。

---

## 4. APP 侧启动职责（bootloader 跳转路径，`HAS_BOOTLOADER=1`）

> ⚠️ **关键区分（本次误诊的根源）**：本章描述的是**被 bootloader 从 0x040000 跳入**
> 的 APP（BanDataHub）。它与 **BanBox 那种 `HAS_BOOTLOADER=0` 的独立启动恰好相反**：
> - **独立启动**：芯片复位后 Cache 全关、`.data`/`.bss` 未初始化，APP **必须**自己
>   `EnableIDCache()` 并自己拷 `.data`。
> - **跳转启动**：bootloader **已把 Cache 开好、把 `.data`/`.bss` 备好**，APP
>   **不能再碰 Cache、也不能再拷 `.data`**，否则挂死。
>
> 早期把 BanBox（独立启动）当成通用基准，导致给跳转 APP 错误地加回 `EnableIDCache()`，
> 详见 §5 复盘。

### 4.1 `__init()`（startup/init-default.c）

```c
void __init()
{
    app_diag_putc('A');
    __cpu_init();                  /* IVB = __executable_start & 0xFFFF0000 → 0x040000，
                                    * 中断向量必须指向 APP 自己的向量表，否则第一个中断
                                    * 跳回 bootloader 向量表崩溃；同时配 PSW/FPU */
    app_diag_putc('p');
    /* ❌ 不调用 EnableIDCache()！
     * bootloader 的 Boot_JumpTo() Phase 2 已 DataCacheInvalidAll() 且【保留 I-Cache
     * 使能】，跳转进来时 I/D-Cache 均已开启、SRAM 为 write-through。在 APP 的冷代码里
     * 再次 invalidate+enable Cache/TLB，会在单口 XIP Flash 上触发 TLB/Cache 重编程挂死
     * （日志停在 'A' 之后）——SDK init-default.c 亦刻意跳过 EnableIDCache。 */
    HardwareStackProtectEnable();  /* 重新开 HSP，SP_BOUND=0x20003000
                                    * （bootloader 跳转前 Phase 1 已关，这里恢复） */
    app_diag_putc('B');
    /* ❌ 不调用 Chip_MemInit()：MPU 已由 bootloader 的 Chip_MemInit() 配好并跨跳转保留；
     * main() 再按需 Chip_Init。提前调用会留下过期 MPU 状态 → Imprecise Bus Error。 */
    __c_init();                    /* HAS_BOOTLOADER 下直接 return，见 §4.2 */
    app_diag_putc('C');
}
```

### 4.2 `__c_init()` — `HAS_BOOTLOADER` 下【整体跳过】拷贝

```c
void __c_init()
{
#if HAS_BOOTLOADER
    /* bootloader 的 Boot_JumpTo() Phase 3 已按 BootInfo 拷好 .data(日志 'd')、
     * 清好 .bss(日志 'z')，且刻意【不写】0x20000000 handoff 魔数(upgrade.c:337)。所以：
     *   1) 不能用运行期魔数检查——魔数根本不存在，检查永远落空 → 会继续往下拷；
     *   2) 拷贝循环是冷代码(不在 I-Cache)，从 Flash 取指(IBus) 同时读 .data LMA(SBus)
     *      会在单口 XIP Flash 上互斥死锁(日志停在 'B' 之后)。
     * .data/.bss 已备好，直接返回。 */
    return;
#else
    /* 独立启动(无 bootloader)：自行拷 .data、清 .bss。此路径 __init 会先
     * EnableIDCache() 开 I-Cache，拷贝循环取指走 Cache、读数据走 SBus，不死锁。 */
    MEMCPY(&__data_start, &__data_lmastart, &_edata - &__data_start);
    MEMSET(&__bss_start, 0, &_end - &__bss_start);
    return;
#endif
}
```

> **为什么不用 handoff 魔数？** 魔数方案要求 bootloader 跳转前往 0x20000000 写
> `0xDEADBEEF`。但本 bootloader **刻意不写**（该地址在 HSP SP_BOUND 之下，历史上写过
> 会异常、日志停在 `Pdz`，见 `upgrade.c:337`）。既然 bootloader 已无条件拷好
> `.data`/`.bss`，APP 侧用**编译期** `#if HAS_BOOTLOADER` 跳过最简单可靠。

### 4.3 `main()` — `HAS_BOOTLOADER=1` 分支

| 动作 | bootloader 路径 | 独立启动路径 |
|---|---|---|
| `Chip_Init(1)` / 时钟 / PLL | **跳过**（bootloader 已配置，重配可能 PLL 失锁挂死） | 执行 |
| `WDG_Disable()` | 执行 | 执行 |
| `DbgUartInit(1,115200,8,0,1)` | 执行（只重建被 `__c_init` 清掉的驱动软件状态，硬件不动） | 执行 |
| `Remap_InitTcm(0, 12)` + `SpiFlashInit(80M,4BIT)` + `DMA_ChannelAllocTableSet` | 执行（同上，重建软件状态） | `Remap_DisableTcm()` + 按板型配置 |
| `spi_init()`（SPIM 外设） | **跳过**（会重配共享 SPI 控制器，破坏 XIP） | 执行 |
| `FwUpgrade_BootInit()` | 执行（仅记录当前分区，跳转决策已由 bootloader 完成） | 执行 |

### 4.4 启动成功后（MainTask 内）

```c
FwUpgrade_ConfirmBootSuccess();  /* 清 PartFlag.boot_fail_cnt，
                                  * 告诉 bootloader 本分区启动成功 */
FwUpgrade_Init();                /* 初始化 CDC + BLE OTA 升级引擎 */
```

### 4.5 请求进入升级模式（APP → bootloader）

`FwUpgrade_RebootToBootloader()`：向 `BURN_FLAG_ADDR(0x3F000)` 写
`"BOOT"` 魔数 → 复位。bootloader 读到后**擦除魔数**（一次性）并驻留
USB CDC 升级；若用户直接再复位，魔数已清，正常跳 APP。

升级中的断电保护由 `UPG_PENDING`（0x3F004，"PEND"）兜底：置位后
bootloader 拒绝跳转，直到 `CMD_FINISH` 成功才清除。

### 4.6 APP 侧诊断字符速查（贯穿 `__init` 与 `main`）

BanDataHub 在启动关键节点埋了裸 UART1 单字符诊断码（`app_diag_putc` /
`diag_putc`，与 bootloader 同一 UART、115200，不依赖 printf），串口日志即启动
“黑匣子”。bootloader 跳转冷启动的**完整正常字符流**：

| 字符 | 出处 | 含义 |
|---|---|---|
| `P d z H J` | bootloader `Boot_JumpTo` | 见 §3（校验/拷 .data/清 .bss/跳转） |
| `A` | `__init` 入口 | 向量表 + crt0 已成功进入 C 代码 |
| `p` | `__cpu_init` 之后 | IVB 已指向 APP 向量表(0x040000)、PSW/FPU 就绪 |
| `B` | `HardwareStackProtectEnable` 之后 | 栈保护恢复（**中间已不再有 `EnableIDCache`**） |
| `C` | `__c_init` 之后 | 跳转路径下 `__c_init` 直接 return（.data/.bss 已由 bootloader 备好） |
| `M` | `main()` 入口 | startup 全部完成，进入应用 |
| `1` | `WDG_Disable` 后 | 看门狗已关 |
| `U` | `DbgUartInit` 后 | UART 驱动软件状态已重建 |
| `R` | `Remap_InitTcm`/`SpiFlashInit`/`DMA_ChannelAllocTableSet` 后 | XIP/TCM/DMA 软件状态已重建 |
| `3 4 5` | `FwUpgrade_BootInit` 前后 | 分区跟踪初始化 |
| `6 7` | `GIE_ENABLE` / Timer2 配置后 | 全局中断与系统节拍就绪 |
| `8` | `prvInitialiseHeap` 后 | FreeRTOS 堆就绪 |
| banner | `DBG(...)` | `BanDataHub SDK` 横幅，随后创建 MainTask |

**排障口诀（字符流停在哪，故障就在它对应的调用里）**：

- 停在 `H`，无 `J`/`A` → bootloader Phase 4 跳转本身失败（入口地址 / 镜像损坏）；
- 有 `A` 无 `p` → `__cpu_init` 出问题（IVB / PSW / FPU 配置）；
- 有 `p` 无 `B` → `HardwareStackProtectEnable` 出问题（SP 已低于 SP_BOUND 0x20003000？）；
- 有 `B` 无 `C` → `__c_init` 拷贝死锁：没走 `#if HAS_BOOTLOADER` 跳过（跳转 APP 误开了拷贝）；
- 有 `C` 无 `M` → crt0 到 main 之间（栈 / 堆 / 构造器）异常；
- `M` 之后中断 → 对应 main 初始化步骤（UART/SPI/DMA/PLL）出问题。

> ⚠️ 若在 `A`→`p`→`B` 之间出现乱码，几乎总是**跳转 APP 误调了 `EnableIDCache()`**
> （Cache 已由 bootloader 开好，APP 再开即在单口 XIP 上挂死），或 HSP 过早开启（详见 §5）。

---

## 5. BanDataHub 失败案例复盘（2026-09 实测日志，两阶段定位）

本案例的价值在于：**第一直觉（照搬 BanBox）是错的**，靠诊断字符流迭代才定位真因。

### 阶段一：`PdzH` + 乱码（无 `J`/`A`）

```
[BOOT] Jumping to 0x00040000 ...
PdzH（乱码）
```

当时误判为“APP 缺 `EnableIDCache()`、Cache 没初始化”，于是**照 BanBox 加回**
`EnableIDCache()` + handoff 魔数版 `__c_init()`。→ 见阶段二，这个方向是错的。
（同期修正的 `.sag` 基址、`crt0.S` 对齐等确属真问题，让 APP 至少能进到 `__init`。）

### 阶段二：`PdzHJA` + 乱码（有 `A`，卡在 `A`→`B`）

```
[BOOT] Single-partition: checking firmware @ 0x00040000 = 0xF60D0048 (magic@0xA4=0x42475046)
[BOOT] Jumping to 0x00040000 ...
PdzHJA（乱码）
```

| 现象 | 结论 |
|---|---|
| 多出 `J` `A` | bootloader 已跳转、APP `__init` **已进入并执行到第一行** |
| 卡在 `A` 之后、无 `B` | 崩溃落在 `__cpu_init` / `EnableIDCache` / `HSP` 三者之间 |
| SDK `init-default.c` 刻意跳过 `EnableIDCache`（注：“TLB hang”） | 强佐证：跳转 APP 再开 Cache 会挂 |

**真根因**：**跳转启动 ≠ 独立启动**。bootloader 的 `Boot_JumpTo()`：
- Phase 2 只 `DataCacheInvalidAll()`、**保留 I-Cache 使能**；且 bootloader 自身 `__init`
  早已 `EnableIDCache()`。→ 跳转进 APP 时 **Cache 本来就是开的**。
- Phase 3 已按 BootInfo **拷好 `.data`、清好 `.bss`**，且**刻意不写** handoff 魔数。

所以 APP 再调 `EnableIDCache()` = 在冷 XIP 代码里重新 invalidate+enable Cache/TLB →
单口 Flash 上 TLB/Cache 重编程挂死（阶段二卡在 `A` 后）。而魔数版 `__c_init()` 因
魔数不存在会落空、继续拷贝 → IBus/SBus 死锁（这是修好 `A`→`B` 后紧接着会撞的第二堵墙）。

**修复**（本文档 §4.1/§4.2 已同步）：
1. `__init` **删掉 `EnableIDCache()`**（Cache 由 bootloader 负责，APP 不碰）；
2. `__c_init` 改用**编译期** `#if HAS_BOOTLOADER return;`（不再依赖运行期魔数）；
3. 保留 `__cpu_init`（重定向 IVB 到 APP 向量表）与 `HardwareStackProtectEnable`。

**验证**：重编 + 烧录（烧录前按 §6 校验镜像），上电串口应输出完整字符流
`PdzHJ` → `ApBC` → `M1UR345678` → `BanDataHub SDK` 横幅，中途无乱码即跳转成功。
若仍卡在 `A`/`p`/`B` 某处，按 §4.6 口诀继续缩小到具体调用。

---

## 6. 移植验收清单（新 APP 接入 bootloader 必查）

编译后、烧录前，用如下 PowerShell 校验 `output/<APP>.bin`：

```powershell
$b=[IO.File]::ReadAllBytes("<APP>.bin")
"magic@0xA4  = 0x{0:X8}  (须=0x42475046 BGPF)" -f [BitConverter]::ToUInt32($b,0xA4)
"BGOF @0x104 = 0x{0:X8}  (须=0x42474F46)"      -f [BitConverter]::ToUInt32($b,0x104)
"data_lma    = 0x{0:X8}  (须∈[0x040000,分区末])" -f [BitConverter]::ToUInt32($b,0x108)
"data_vma    = 0x{0:X8}  (须=0x20004000)"      -f [BitConverter]::ToUInt32($b,0x10C)
"bss_end     = 0x{0:X8}  (须≤0x20048000)"      -f [BitConverter]::ToUInt32($b,0x118)
```

逐项检查：

- [ ] `.ld`：`__executable_start` / `NDS_SAG_LMA_EILM` / `. =` 均为 `0x040000`；EILM 溢出断言与分区容量匹配（2MB 单分区 ≤ `0x1BF000`）
- [ ] `.sag`（若保留）：`EILM/EXEC_CODE` 基址与 `.ld` 一致（历史遗留文件，不参与 makefile 链接，但不得误导）
- [ ] `app_config.h`：`HAS_BOOTLOADER = 1`
- [ ] `stub()`：0xA4 BGPF + 0x104 BGOF 六个字段齐全（直接抄 BanBox）
- [ ] `__cpu_init()`：IVB = `__executable_start & 0xFFFF0000`
- [ ] `__init()`：`__cpu_init` → `HardwareStackProtectEnable` → `__c_init`，**既不调用 `EnableIDCache`也不调用 `Chip_MemInit`**（Cache/MPU 均由 bootloader 备好并跨跳转保留）
- [ ] `__c_init()`：`#if HAS_BOOTLOADER` **直接 return**（bootloader 已拷好 .data/.bss 且不写魔数）；仅独立启动（`#else`）才自拷贝
- [ ] `main()`：bootloader 路径跳过 `Chip_Init`/时钟/`spi_init`，重建 UART/SPI Flash/DMA/TCM 软件状态
- [ ] MainTask：`FwUpgrade_ConfirmBootSuccess()` + `FwUpgrade_Init()`
- [ ] `dual_partition.h`：`PART_A_BASE/PART_FLAG_MAGIC/BURN_FLAG_*` 与 bootloader `upgrade.h` 逐字段一致（结构体偏移不得变化）
- [ ] USB 身份：APP VID/PID 与 bootloader（0x0001/0x4247）区分
- [ ] bin 校验：§6 脚本五个值全部达标
- [ ] 上电日志：`PdzHJ` → `ApBC` → `M1UR345678` → APP banner，无乱码

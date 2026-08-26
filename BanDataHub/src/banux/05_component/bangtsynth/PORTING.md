# BanGTsynth 移植清单

产品代码只包含 `bg_synth.h`，按下面顺序调用：

```c
/* 1. 平台：PSRAM / SD / Flash 就绪 */
/* 2. */ bg_synth_init();
/* 3. 音频回调 */ bg_synth_render(pcm, frames);
/* 4. 任意任务 */ bg_synth_note_on(ch, note, vel, prog);
```

## 必实现（一个 port 目录）

从 `01_hal/port/template/` 复制到 `01_hal/port/<mcu>/`，去掉 `BG_PORT_TEMPLATE` 保护，实现：

| 文件 | 接口 | 最低要求 |
|---|---|---|
| `bg_storage_*.c` | `bg_storage_driver_port` | `read(offset)` 能读出完整 SF2/BGS |
| `bg_osal_*.c` | `bg_osal.h` | 队列 + `bg_get_tick_ms`；裸机用 template 忙等 |
| `bg_extmem_*.c` | `bg_extmem.h` | 无 PSRAM 可 `ready()==0`（不要开 `SYNTH_SD_NAND_PSRAM_EN`） |
| `bg_mem_*.c` | `bg_mem.h` | malloc 或 `bg_mem_arena.c` |
| `bg_download_port_*.c` | 可选 | 不下载体用空函数 |

日志：`01_hal/bg_log.c` 已通用，用 `BG_Log.SetOutputFunc()` 接到 UART。

## 配置

1. 复制 `01_hal/port/template/bg_config_port.template.h` 的开关到 `bg_config_port.h`（或 `-D`）。
2. 只编 `sources.core.cmake` 里的 `BANGTSYNTH_CORE_SRCS` + 你的 port，**不要编** `BG_*` 旧目录、`port/bp10`、`port/template`、`02_core/fat32`。
3. Include：`bangtsynth/`、`01_hal/`、`02_core/soundbank|midi|envelope|sampler`、`BG_err_handle/`。

## 自检

- [ ] 链接只有一份 `bg_storage_driver_port` / `bg_mem_alloc` / `bg_extmem_read`
- [ ] `bg_synth_init()` 后 `bg_synth_render` 输出静音（无音源）或 SF2 声
- [ ] NoteOn 从非音频任务调用不死锁
- [ ] 1ms tick 调用了 `bg_tick_increment()`（裸机）

## 不要改

`02_core/` 解析与混音。板级差异只放 port 和 `bg_config_port.h`。

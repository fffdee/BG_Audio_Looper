# 采样率同步问题修复说明

## 问题描述

在 BanBox 音频系统中，不同音频源使用不同的采样率：

1. **ADC 输入 (吉他/麦克风)**: 44100 Hz
2. **USB 音频**: 通常 48000 Hz
3. **蓝牙 A2DP**: 44100 Hz 或 48000 Hz (取决于编码)

当多个音频源混音时，如果采样率不匹配会导致：
- 音调变化（移调）
- 播放速度异常
- 蓝牙音频无声或杂音

## 原方案处理方式

老方案中采用了两种工作模式：

### 1. 蓝牙驱动模式
```c
// 由蓝牙解码驱动整个音频循环
while (AudioADC_DataLenGet(ADC0_MODULE) < n || AudioADC_DataLenGet(ADC1_MODULE) < n);
// 同步等待 ADC 数据准备好
```

### 2. ADC 驱动模式
```c
// 由 ADC 数据可用量驱动
if (AudioADC_DataLenGet(ADC0_MODULE) >= MIN_SAMPLE)
```

## 新方案 (Effect Graph) 的修复

### 修改 1: AudioLoopWithGraph 支持双模式

```c
static void AudioLoopWithGraph(void)
{
    bool bt_active = (GetA2dpState() == BT_A2DP_STATE_STREAMING) && 
                     (mv_msize(&SBC_MemHandle) > SBC_DECODER_FIFO_MIN);
    
    if (bt_active) {
        /* 蓝牙驱动模式 */
        // 预估帧大小
        frame_size = 128;  
        
        // 等待 DAC 空间
        if (AudioDAC_DataSpaceLenGet(DAC0) < frame_size) return;
        
        // 同步等待 ADC 数据
        if (AudioADC_DataLenGet(ADC0_MODULE) < frame_size || 
            AudioADC_DataLenGet(ADC1_MODULE) < frame_size) return;
            
        EffectGraph_Process(frame_size);
    } else {
        /* ADC 驱动模式 */
        // 根据 ADC 可用数据量决定帧大小
        // ...
    }
}
```

### 修改 2: BT_ReadAudioData 添加采样率检测

```c
static uint16_t BT_ReadAudioData(...)
{
    // 解码蓝牙音频
    audio_decoder_decode();
    
    // 检测采样率变化
    if (last_sample_rate != audio_decoder->song_info->sampling_rate) {
        DBG("[BT Audio] Sample rate changed to: %ld Hz\n", ...);
        
        // 如果不匹配，警告
        if (last_sample_rate != BG_AudioManager.Audio_data.SampleRate) {
            DBG("[BT Audio] WARNING: Sample rate mismatch!\n");
            // TODO: 初始化重采样器
        }
    }
    
    // TODO: 应用重采样（如果需要）
    // pcm_len = resampler_apply(&bt_resmaper, ...);
    
    return pcm_len;
}
```

## 采样率重采样方案 (可选)

如果需要支持不同采样率的音频源混音，需要启用重采样器：

### 1. 在 BG_audio_Init 中初始化重采样器

```c
// 初始化蓝牙音频重采样器 (48kHz -> 44.1kHz)
resampler_init(&bt_resmaper, 
               2,      // 2通道立体声
               48000,  // 输入采样率
               44100,  // 输出采样率
               0, 0);
```

### 2. 在 BT_ReadAudioData 中应用重采样

```c
if (last_sample_rate != BG_AudioManager.Audio_data.SampleRate) {
    // 重采样到系统采样率
    pcm_len = resampler_apply(&bt_resmaper, 
                              pcm_data,    // 输入
                              pcm_data,    // 输出(原地)
                              pcm_len);    // 输入长度
}
```

### 3. USB 音频同样需要处理

USB 音频通常是 48kHz，如果系统是 44.1kHz，也需要重采样：

```c
static uint16_t USB_ReadAudioData(...)
{
    // 读取 USB 数据
    UsbAudioSpeakerDataGet(temp_buf, samples_to_read);
    
    // TODO: 如果 USB 是 48kHz，系统是 44.1kHz，需要重采样
    // samples_to_read = resampler_apply(&usb_resampler, ...);
    
    return samples_to_read;
}
```

## 当前状态

**已实现:**
- ✅ 双模式音频循环（蓝牙/ADC 驱动）
- ✅ 蓝牙采样率检测和警告
- ✅ 同步等待机制

**待实现 (如果需要):**
- ⚠️ 蓝牙音频重采样（48kHz -> 44.1kHz）
- ⚠️ USB 音频重采样
- ⚠️ 动态采样率切换

## 建议

### 方案 A: 固定系统采样率 (推荐)
保持系统采样率为 44100 Hz，所有输入源重采样到 44100 Hz：
- 优点：稳定，延迟可控
- 缺点：需要重采样开销

### 方案 B: 动态采样率切换
根据主要音频源动态切换 DAC/ADC 采样率：
- 优点：无需重采样
- 缺点：切换时可能有爆音，逻辑复杂

### 方案 C: 只在必要时重采样
大部分蓝牙音频是 44.1kHz，只有少数是 48kHz：
- 优点：减少重采样开销
- 缺点：需要运行时检测和处理

## 测试建议

1. **测试蓝牙 44.1kHz 音频**: 应该正常播放
2. **测试蓝牙 48kHz 音频**: 检查是否有移调/变速
3. **测试 USB 音频**: 检查是否有移调/变速
4. **测试混音**: ADC + 蓝牙同时播放

## 调试信息

查看串口输出，应该看到：
```
[BT Audio] Sample rate changed to: 44100 Hz
或
[BT Audio] Sample rate changed to: 48000 Hz
[BT Audio] WARNING: Sample rate mismatch! BT:48000 System:44100
```

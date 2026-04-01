#!/usr/bin/env python3
"""
MP3/WAV/SBC → C Array Converter
将 MP3/WAV/SBC 文件转换为嵌入式 C const 数组，用于 BanBox 开机提示音。
生成的 .h 和 .c 文件可直接添加到 BanBox 固件工程中。
"""

import sys
import os
import wave
import audioop
import io
import lameenc

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QGroupBox, QLabel, QLineEdit, QPushButton, QTextEdit, QFileDialog,
    QProgressBar, QStatusBar, QSplitter, QMessageBox, QComboBox, QSlider, QSpinBox,
    QDoubleSpinBox, QCheckBox
)
from PyQt5.QtCore import Qt, QThread, pyqtSignal, QTimer, QUrl, QEventLoop
from PyQt5.QtMultimedia import QMediaPlayer, QMediaContent, QAudioDecoder, QAudioFormat
from PyQt5.QtGui import QFont, QColor, QPalette, QTextCursor


# ---------------------------------------------------------------------------
# Dark theme palette
# ---------------------------------------------------------------------------
def apply_dark_theme(app: QApplication):
    app.setStyle("Fusion")
    pal = QPalette()
    pal.setColor(QPalette.Window, QColor(45, 45, 45))
    pal.setColor(QPalette.WindowText, QColor(220, 220, 220))
    pal.setColor(QPalette.Base, QColor(30, 30, 30))
    pal.setColor(QPalette.AlternateBase, QColor(50, 50, 50))
    pal.setColor(QPalette.ToolTipBase, QColor(60, 60, 60))
    pal.setColor(QPalette.ToolTipText, QColor(220, 220, 220))
    pal.setColor(QPalette.Text, QColor(220, 220, 220))
    pal.setColor(QPalette.Button, QColor(60, 60, 60))
    pal.setColor(QPalette.ButtonText, QColor(220, 220, 220))
    pal.setColor(QPalette.BrightText, Qt.red)
    pal.setColor(QPalette.Highlight, QColor(42, 130, 218))
    pal.setColor(QPalette.HighlightedText, Qt.black)
    pal.setColor(QPalette.Disabled, QPalette.Text, QColor(128, 128, 128))
    pal.setColor(QPalette.Disabled, QPalette.ButtonText, QColor(128, 128, 128))
    app.setPalette(pal)


# ---------------------------------------------------------------------------
# Conversion worker (runs in background thread)
# ---------------------------------------------------------------------------
class ConvertWorker(QThread):
    log_signal   = pyqtSignal(str)   # log text line
    progress     = pyqtSignal(int)   # 0-100
    finished_ok  = pyqtSignal(str, str)  # (h_path, c_path)
    finished_err = pyqtSignal(str)   # error message

    def __init__(self, src_path: str, out_dir: str, array_name: str,
                 volume_pct: int = 100, pre_data: bytes = None,
                 trim_start_ms: int = 0, trim_end_ms: int = 0):
        super().__init__()
        self.src_path      = src_path
        self.out_dir       = out_dir
        self.array_name    = array_name
        self.volume_pct    = max(1, min(200, volume_pct))  # clamp 1-200%
        self.pre_data      = pre_data  # MP3 音量/裁剪后的字节（主线程预处理）
        self.trim_start_ms = trim_start_ms
        self.trim_end_ms   = trim_end_ms

    def log(self, msg: str):
        self.log_signal.emit(msg)

    def run(self):
        try:
            self._convert()
        except Exception as e:
            self.finished_err.emit(str(e))

    # ------------------------------------------------------------------
    def _apply_wav_volume(self, data: bytes, vol_pct: int) -> bytes:
        """步骤3: 对 WAV 文件的 PCM 采样值按 vol_pct/100 缩放，返回新的 WAV bytes。
        使用内置 wave + audioop，无需外部依赖。
        """
        buf_in = io.BytesIO(data)
        try:
            with wave.open(buf_in) as wf:
                n_channels = wf.getnchannels()
                sampwidth  = wf.getsampwidth()   # bytes per sample (1/2/4)
                framerate  = wf.getframerate()
                n_frames   = wf.getnframes()
                pcm        = wf.readframes(n_frames)
        except Exception as e:
            raise ValueError(f"WAV 解析失败，无法调整音量: {e}")

        factor  = vol_pct / 100.0
        if sampwidth not in (1, 2, 4):
            raise ValueError(f"不支持的 WAV 位宽: {sampwidth * 8} bit")

        # audioop.mul 会自动处理整数溢出截断（防止破音）
        scaled = audioop.mul(pcm, sampwidth, factor)

        buf_out = io.BytesIO()
        with wave.open(buf_out, 'wb') as wf:
            wf.setnchannels(n_channels)
            wf.setsampwidth(sampwidth)
            wf.setframerate(framerate)
            wf.writeframes(scaled)
        return buf_out.getvalue()

    def _trim_wav(self, data: bytes, start_ms: int, end_ms: int) -> bytes:
        """对 WAV 文件按起止时间（毫秒）裁剪，返回新的 WAV bytes。"""
        buf_in = io.BytesIO(data)
        try:
            with wave.open(buf_in) as wf:
                n_channels = wf.getnchannels()
                sampwidth  = wf.getsampwidth()
                framerate  = wf.getframerate()
                n_frames   = wf.getnframes()
                start_frame = int(framerate * start_ms / 1000)
                end_frame   = int(framerate * end_ms / 1000) if end_ms > 0 else n_frames
                start_frame = max(0, min(start_frame, n_frames))
                end_frame   = max(start_frame, min(end_frame, n_frames))
                wf.setpos(start_frame)
                pcm = wf.readframes(end_frame - start_frame)
        except Exception as e:
            raise ValueError(f"WAV 解析失败，无法裁剪: {e}")
        buf_out = io.BytesIO()
        with wave.open(buf_out, 'wb') as wf:
            wf.setnchannels(n_channels)
            wf.setsampwidth(sampwidth)
            wf.setframerate(framerate)
            wf.writeframes(pcm)
        return buf_out.getvalue()

    def _convert(self):
        src = self.src_path
        if not os.path.isfile(src):
            raise FileNotFoundError(f"文件不存在: {src}")

        file_size = os.path.getsize(src)
        self.log(f"读取文件: {src}")
        self.log(f"文件大小: {file_size:,} 字节 ({file_size/1024:.1f} KB)")

        with open(src, "rb") as f:
            data = f.read()

        ext = os.path.splitext(src)[1].lower()

        # ------------------------------------------------------------------
        # WAV 长度裁剪（在音量调整之前执行）
        # ------------------------------------------------------------------
        if (self.trim_start_ms > 0 or self.trim_end_ms > 0) and ext == '.wav':
            start_s = self.trim_start_ms / 1000.0
            end_s   = self.trim_end_ms / 1000.0
            self.log(f"裁剪 WAV: {start_s:.3f}s ~ {end_s:.3f}s")
            self.progress.emit(5)
            data = self._trim_wav(data, self.trim_start_ms, self.trim_end_ms)
            self.log(f"裁剪完成，新大小: {len(data):,} 字节")

        # ------------------------------------------------------------------
        # 检测文件类型并执行音量调整
        # WAV:  wave + audioop 直接 PCM 缩放
        # MP3:  QAudioDecoder 解码 → audioop 缩放 → 输出 WAV（固件 RIFF 自动检测）
        # 其他: 跳过音量调整，按原始数据嵌入
        # ------------------------------------------------------------------
        vol        = self.volume_pct
        output_ext = ext   # 记录实际输出扩展名（MP3 调音量时改为 .wav）
        if vol != 100:
            if ext == '.wav':
                self.log(f"调整音量: {vol}%  (WAV PCM 线性缩放)")
                self.progress.emit(10)
                data = self._apply_wav_volume(data, vol)
                self.log(f"音量调整完成，缩放后大小: {len(data):,} 字节")
            elif ext == '.mp3':
                if self.pre_data is not None:
                    self.log(f"使用主线程预处理数据 (MP3 {vol}% 音量): {len(self.pre_data):,} 字节")
                    data = self.pre_data
                else:
                    self.log("⚠ MP3 预处理数据缺失，按原始数据嵌入")
            else:
                self.log(f"⚠ 警告: {ext.upper()} 格式暂不支持音量调整，已按原始电平嵌入。")
        else:
            output_ext = ext

        self.progress.emit(20)
        self.log("生成 C 数组...")

        name = self.array_name
        h_name = f"{name}.h"
        c_name = f"{name}.c"
        h_path = os.path.join(self.out_dir, h_name)
        c_path = os.path.join(self.out_dir, c_name)

        # ---------- Build .h ----------
        fmt_note = f" [→{output_ext[1:].upper()}]" if output_ext != ext else ""
        guard = name.upper() + "_H"
        h_lines = [
            f"/* Auto-generated by mp3_to_c_array.py — DO NOT EDIT */",
            f"/* Source: {os.path.basename(src)}{fmt_note}  Size: {file_size} bytes  Volume: {self.volume_pct}% */",
            f"#ifndef {guard}",
            f"#define {guard}",
            "",
            "#include <stdint.h>",
            "",
            f"extern const uint8_t  {name}[];",
            f"extern const uint32_t {name}_size;",
            "",
            f"#endif /* {guard} */",
            "",
        ]
        h_content = "\n".join(h_lines)

        # ---------- Build .c ----------
        COLS = 16  # bytes per row
        total = len(data)
        rows = []
        for i in range(0, total, COLS):
            chunk = data[i:i+COLS]
            hex_vals = ", ".join(f"0x{b:02X}" for b in chunk)
            rows.append(f"    {hex_vals}")
            pct = 20 + int(70 * min(i + COLS, total) / total)
            self.progress.emit(pct)

        array_body = ",\n".join(rows)
        c_lines = [
            f"/* Auto-generated by mp3_to_c_array.py — DO NOT EDIT */",
            f"/* Source: {os.path.basename(src)}{fmt_note}  Size: {file_size} bytes  Volume: {self.volume_pct}% */",
            "",
            "#include <stdint.h>",
            f'#include "{h_name}"',
            "",
            f"const uint8_t {name}[] = {{",
            array_body,
            "};",
            "",
            f"const uint32_t {name}_size = {total}U;",
            "",
        ]
        c_content = "\n".join(c_lines)

        # ---------- Write files ----------
        os.makedirs(self.out_dir, exist_ok=True)
        with open(h_path, "w", encoding="utf-8") as f:
            f.write(h_content)
        with open(c_path, "w", encoding="utf-8") as f:
            f.write(c_content)

        self.progress.emit(100)
        self.log(f"写入 .h: {h_path}")
        self.log(f"写入 .c: {c_path}")
        self.log(f"数组名称 : {name}[]")
        self.log(f"大小变量 : {name}_size = {total}")
        if self.volume_pct != 100:
            if ext == '.wav':
                self.log(f"音量设置 : {self.volume_pct}%  (WAV PCM 已缩放)")
            elif ext == '.mp3':
                self.log(f"音量设置 : {self.volume_pct}%  (MP3→WAV PCM 已缩放)")
            else:
                self.log(f"音量设置 : {self.volume_pct}%  (格式不支持，按原始电平嵌入)")
        else:
            self.log(f"音量设置 : 100%  (原始电平，未修改)")
        if self.trim_start_ms > 0 or self.trim_end_ms > 0:
            self.log(f"长度裁剪 : {self.trim_start_ms/1000:.3f}s ~ {self.trim_end_ms/1000:.3f}s")
        else:
            self.log(f"长度裁剪 : 未启用（完整文件）")
        self.log(f"固件 ROM 占用: ~{total:,} 字节 ({total/1024:.1f} KB)")
        self.log("转换完成！")
        self.finished_ok.emit(h_path, c_path)


# ---------------------------------------------------------------------------
# Main window
# ---------------------------------------------------------------------------
class MainWindow(QMainWindow):

    def __init__(self):
        super().__init__()
        self.setWindowTitle("MP3 → C Array Converter  |  BanBox 开机提示音工具")
        self.setMinimumSize(640, 560)
        self._worker = None
        self._player = QMediaPlayer()
        self._player.stateChanged.connect(self._on_player_state_changed)
        self._player.durationChanged.connect(self._on_duration_detected)
        self._audio_duration_ms = 0
        self._build_ui()

    # ------------------------------------------------------------------
    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setSpacing(8)
        root.setContentsMargins(10, 10, 10, 10)

        # ---- Source file ----
        grp_src = QGroupBox("源文件  (MP3 / WAV / SBC)")
        lay_src = QHBoxLayout(grp_src)
        self.src_edit = QLineEdit()
        self.src_edit.setPlaceholderText("选择要转换的音频文件...")
        self.src_edit.textChanged.connect(self._auto_fill_defaults)
        btn_src = QPushButton("浏览…")
        btn_src.setFixedWidth(70)
        btn_src.clicked.connect(self._browse_src)
        lay_src.addWidget(self.src_edit)
        lay_src.addWidget(btn_src)
        root.addWidget(grp_src)

        # ---- Output directory ----
        grp_out = QGroupBox("输出目录")
        lay_out = QHBoxLayout(grp_out)
        self.out_edit = QLineEdit()
        self.out_edit.setPlaceholderText("生成的 .h / .c 文件保存目录...")
        btn_out = QPushButton("浏览…")
        btn_out.setFixedWidth(70)
        btn_out.clicked.connect(self._browse_out)
        lay_out.addWidget(self.out_edit)
        lay_out.addWidget(btn_out)
        root.addWidget(grp_out)

        # ---- Array name ----
        grp_name = QGroupBox("C 数组名称")
        lay_name = QHBoxLayout(grp_name)
        lbl_prefix = QLabel("const uint8_t ")
        lbl_prefix.setFont(QFont("Courier New", 10))
        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("例: g_remind_power_on")
        self.name_edit.setFont(QFont("Courier New", 10))
        lbl_suffix = QLabel("[];")
        lbl_suffix.setFont(QFont("Courier New", 10))
        lay_name.addWidget(lbl_prefix)
        lay_name.addWidget(self.name_edit)
        lay_name.addWidget(lbl_suffix)
        root.addWidget(grp_name)

        # ---- Volume ----
        # 步骤5: 音量控制（WAV 有效，MP3/SBC 灰掉）
        grp_vol = QGroupBox("音量调整  (WAV / MP3 均支持)")
        lay_vol_outer = QVBoxLayout(grp_vol)
        lay_vol = QHBoxLayout()
        lbl_vol = QLabel("音量:")
        self.vol_slider = QSlider(Qt.Horizontal)
        self.vol_slider.setRange(1, 200)
        self.vol_slider.setValue(100)
        self.vol_slider.setTickInterval(10)
        self.vol_slider.setTickPosition(QSlider.TicksBelow)
        self.vol_spinbox = QSpinBox()
        self.vol_spinbox.setRange(1, 200)
        self.vol_spinbox.setValue(100)
        self.vol_spinbox.setSuffix(" %")
        self.vol_spinbox.setFixedWidth(72)
        # sync slider <-> spinbox
        self.vol_slider.valueChanged.connect(self.vol_spinbox.setValue)
        self.vol_spinbox.valueChanged.connect(self.vol_slider.setValue)
        # real-time preview volume (clamped to 0-100 for QMediaPlayer)
        self.vol_slider.valueChanged.connect(lambda v: self._player.setVolume(min(100, v)))
        lbl_hint = QLabel("100% = 原始电平  |  50% = 减半  |  200% = 加倍（可能失真）")
        lbl_hint.setStyleSheet("color: #888; font-size: 9px;")
        lay_vol.addWidget(lbl_vol)
        lay_vol.addWidget(self.vol_slider, stretch=1)
        lay_vol.addWidget(self.vol_spinbox)
        lay_vol_outer.addLayout(lay_vol)
        lay_vol_outer.addWidget(lbl_hint)
        # 监听源文件变化来决定是否灰掉音量控件
        self.src_edit.textChanged.connect(self._update_vol_state)
        root.addWidget(grp_vol)

        # ---- Trim ----
        grp_trim = QGroupBox("长度裁剪  (WAV / MP3 支持)")
        lay_trim_outer = QVBoxLayout(grp_trim)
        lay_trim_top = QHBoxLayout()
        self.trim_check = QCheckBox("启用裁剪")
        self.trim_check.setChecked(False)
        self.trim_check.toggled.connect(self._on_trim_toggled)
        self.trim_duration_lbl = QLabel("总时长: --")
        self.trim_duration_lbl.setStyleSheet("color: #aaa;")
        lay_trim_top.addWidget(self.trim_check)
        lay_trim_top.addStretch()
        lay_trim_top.addWidget(self.trim_duration_lbl)
        lay_trim_outer.addLayout(lay_trim_top)
        lay_trim_spin = QHBoxLayout()
        lbl_start = QLabel("开始:")
        self.trim_start_spin = QDoubleSpinBox()
        self.trim_start_spin.setRange(0, 9999.0)
        self.trim_start_spin.setValue(0)
        self.trim_start_spin.setDecimals(3)
        self.trim_start_spin.setSuffix(" 秒")
        self.trim_start_spin.setSingleStep(0.1)
        self.trim_start_spin.setFixedWidth(120)
        self.trim_start_spin.setEnabled(False)
        lbl_end = QLabel("结束:")
        self.trim_end_spin = QDoubleSpinBox()
        self.trim_end_spin.setRange(0, 9999.0)
        self.trim_end_spin.setValue(0)
        self.trim_end_spin.setDecimals(3)
        self.trim_end_spin.setSuffix(" 秒")
        self.trim_end_spin.setSingleStep(0.1)
        self.trim_end_spin.setFixedWidth(120)
        self.trim_end_spin.setEnabled(False)
        lay_trim_spin.addWidget(lbl_start)
        lay_trim_spin.addWidget(self.trim_start_spin)
        lay_trim_spin.addSpacing(16)
        lay_trim_spin.addWidget(lbl_end)
        lay_trim_spin.addWidget(self.trim_end_spin)
        lay_trim_spin.addStretch()
        lay_trim_outer.addLayout(lay_trim_spin)
        lbl_trim_hint = QLabel("选择文件后自动检测时长  |  结束=0 表示到文件末尾")
        lbl_trim_hint.setStyleSheet("color: #888; font-size: 9px;")
        lay_trim_outer.addWidget(lbl_trim_hint)
        self.src_edit.textChanged.connect(self._on_src_changed)
        self.src_edit.textChanged.connect(self._update_trim_state)
        root.addWidget(grp_trim)

        # ---- Preview ----
        grp_preview = QGroupBox("试听  (播放原始文件，音量旋鈕实时控制预听电平)")
        lay_preview = QHBoxLayout()
        self.preview_btn = QPushButton("▶  试听")
        self.preview_btn.setFixedWidth(100)
        self.preview_btn.setEnabled(False)
        self.preview_btn.clicked.connect(self._toggle_preview)
        lbl_preview_hint = QLabel("试听不影响转换结果；仅用于预览音量效果")
        lbl_preview_hint.setStyleSheet("color: #888; font-size: 9px;")
        lay_preview.addWidget(self.preview_btn)
        lay_preview.addWidget(lbl_preview_hint)
        lay_preview.addStretch()
        grp_preview.setLayout(lay_preview)
        root.addWidget(grp_preview)
        self.src_edit.textChanged.connect(self._update_preview_btn_state)

        # ---- Convert button ----
        btn_row = QHBoxLayout()
        self.convert_btn = QPushButton("开始转换")
        self.convert_btn.setFixedHeight(36)
        self.convert_btn.setStyleSheet(
            "QPushButton { background-color: #2A82DA; color: white; font-weight: bold; border-radius: 4px; }"
            "QPushButton:hover { background-color: #3A92EA; }"
            "QPushButton:disabled { background-color: #555; color: #888; }"
        )
        self.convert_btn.clicked.connect(self._start_convert)
        btn_row.addStretch()
        btn_row.addWidget(self.convert_btn)
        btn_row.addStretch()
        root.addLayout(btn_row)

        # ---- Progress ----
        self.progress_bar = QProgressBar()
        self.progress_bar.setTextVisible(True)
        self.progress_bar.setValue(0)
        root.addWidget(self.progress_bar)

        # ---- Log ----
        grp_log = QGroupBox("日志")
        lay_log = QVBoxLayout(grp_log)
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setFont(QFont("Courier New", 9))
        self.log_text.setStyleSheet("background-color: #1a1a1a; color: #b0e0b0;")
        lay_log.addWidget(self.log_text)

        # Copy/Clear buttons under log
        log_btn_row = QHBoxLayout()
        btn_copy = QPushButton("复制日志")
        btn_copy.setFixedWidth(90)
        btn_copy.clicked.connect(self._copy_log)
        btn_clear = QPushButton("清空日志")
        btn_clear.setFixedWidth(90)
        btn_clear.clicked.connect(self.log_text.clear)
        log_btn_row.addWidget(btn_copy)
        log_btn_row.addWidget(btn_clear)
        log_btn_row.addStretch()
        lay_log.addLayout(log_btn_row)
        root.addWidget(grp_log, stretch=1)

        self.statusBar().showMessage("就绪")

    # ------------------------------------------------------------------
    def _browse_src(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "选择音频文件", "",
            "音频文件 (*.mp3 *.wav *.sbc);;所有文件 (*)"
        )
        if path:
            self.src_edit.setText(path)

    def _update_vol_state(self, src_path: str):
        """WAV 和 MP3 均支持音量调整；其他格式置灰恢复默认。"""
        ext    = src_path.lower()
        vol_ok = ext.endswith('.wav') or ext.endswith('.mp3')
        self.vol_slider.setEnabled(vol_ok)
        self.vol_spinbox.setEnabled(vol_ok)
        if not vol_ok and src_path:
            self.vol_slider.setValue(100)
            self.vol_spinbox.setValue(100)

    def _update_trim_state(self, src_path: str):
        """WAV / MP3 支持裁剪；其他格式置灰。"""
        ext = src_path.lower()
        trim_ok = ext.endswith('.wav') or ext.endswith('.mp3')
        self.trim_check.setEnabled(trim_ok)
        enabled = trim_ok and self.trim_check.isChecked()
        self.trim_start_spin.setEnabled(enabled)
        self.trim_end_spin.setEnabled(enabled)
        if not trim_ok and src_path:
            self.trim_check.setChecked(False)

    def _on_trim_toggled(self, checked: bool):
        src = self.src_edit.text().strip().lower()
        trim_ok = src.endswith('.wav') or src.endswith('.mp3')
        enabled = checked and trim_ok
        self.trim_start_spin.setEnabled(enabled)
        self.trim_end_spin.setEnabled(enabled)

    def _on_src_changed(self, src_path: str):
        """检测音频时长，更新裁剪控件范围。"""
        self._audio_duration_ms = 0
        if not src_path or not os.path.isfile(src_path):
            self._update_trim_range(0)
            return
        ext = os.path.splitext(src_path)[1].lower()
        if ext == '.wav':
            try:
                with wave.open(src_path) as wf:
                    dur_ms = int(wf.getnframes() / wf.getframerate() * 1000)
                self._audio_duration_ms = dur_ms
                self._update_trim_range(dur_ms)
            except Exception:
                self._update_trim_range(0)
        elif ext == '.mp3':
            # QMediaPlayer 异步检测时长，durationChanged 回调更新
            self._player.setMedia(QMediaContent(QUrl.fromLocalFile(src_path)))
        else:
            self._update_trim_range(0)

    def _on_duration_detected(self, duration_ms: int):
        if duration_ms > 0:
            self._audio_duration_ms = duration_ms
            self._update_trim_range(duration_ms)

    def _update_trim_range(self, duration_ms: int):
        dur_sec = duration_ms / 1000.0
        self.trim_start_spin.setMaximum(max(0, dur_sec - 0.001) if dur_sec > 0 else 9999.0)
        self.trim_end_spin.setMaximum(dur_sec if dur_sec > 0 else 9999.0)
        self.trim_end_spin.setValue(dur_sec)
        self.trim_start_spin.setValue(0)
        if duration_ms > 0:
            self.trim_duration_lbl.setText(f"总时长: {dur_sec:.3f} 秒")
        else:
            self.trim_duration_lbl.setText("总时长: --")

    def _preprocess_mp3(self, src_path: str, vol_pct: int,
                        trim_start_ms: int = 0, trim_end_ms: int = 0) -> bytes:
        """在主线程用 QAudioDecoder 解码 MP3 → PCM 缩放 → lameenc 编码回 MP3。
        QAudioDecoder 是 Qt 多媒体对象，只能在主线程（有完整事件循环的线程）使用。
        """
        loop    = QEventLoop()
        decoder = QAudioDecoder()

        fmt = QAudioFormat()
        fmt.setSampleRate(44100)
        fmt.setChannelCount(2)
        fmt.setSampleSize(16)
        fmt.setCodec("audio/pcm")
        fmt.setByteOrder(QAudioFormat.LittleEndian)
        fmt.setSampleType(QAudioFormat.SignedInt)
        decoder.setAudioFormat(fmt)
        decoder.setSourceFilename(src_path)

        pcm_data     = bytearray()
        error_holder = [None]

        def on_buffer_ready():
            buf = decoder.read()
            if not buf.isValid():
                return
            af          = buf.format()
            n_frames    = buf.frameCount() if hasattr(buf, 'frameCount') else buf.sampleCount()
            frame_bytes = af.bytesPerFrame() if hasattr(af, 'bytesPerFrame') else (af.sampleSize() // 8 * af.channelCount())
            n_bytes     = n_frames * frame_bytes
            try:
                pcm_data.extend(buf.data().asstring(n_bytes))
            except Exception:
                pass  # 跳过无效帧

        def on_finished():
            loop.quit()

        def on_error(err):
            error_holder[0] = f"QAudioDecoder 错误: {err}"
            loop.quit()

        decoder.bufferReady.connect(on_buffer_ready)
        decoder.finished.connect(on_finished)
        decoder.error.connect(on_error)
        decoder.start()
        loop.exec_()
        decoder.stop()

        if error_holder[0]:
            raise ValueError(error_holder[0])
        if not pcm_data:
            raise ValueError("未获取到 PCM 数据，请确认 Windows 已安装 MP3 解码器")

        pcm = bytes(pcm_data)

        # 裁剪 PCM（16-bit stereo, 44100 Hz）
        frame_size = 2 * 2  # 16-bit * 2ch = 4 bytes/frame
        if trim_start_ms > 0 or trim_end_ms > 0:
            start_byte = int(44100 * trim_start_ms / 1000) * frame_size
            end_byte   = int(44100 * trim_end_ms / 1000) * frame_size if trim_end_ms > 0 else len(pcm)
            start_byte = max(0, min(start_byte, len(pcm)))
            end_byte   = max(start_byte, min(end_byte, len(pcm)))
            pcm = pcm[start_byte:end_byte]

        scaled = audioop.mul(pcm, 2, vol_pct / 100.0)

        enc = lameenc.Encoder()
        enc.set_bit_rate(128)
        enc.set_in_sample_rate(44100)
        enc.set_channels(2)
        enc.set_quality(2)
        mp3_out  = enc.encode(scaled)
        mp3_out += enc.flush()
        return mp3_out

    def _update_preview_btn_state(self, src_path: str):
        has_file = bool(src_path) and os.path.isfile(src_path)
        self.preview_btn.setEnabled(has_file)

    def _toggle_preview(self):
        if self._player.state() == QMediaPlayer.PlayingState:
            self._stop_preview()
        else:
            self._preview_audio()

    def _preview_audio(self):
        src = self.src_edit.text().strip()
        if not src or not os.path.isfile(src):
            return
        self._player.setMedia(QMediaContent(QUrl.fromLocalFile(src)))
        self._player.setVolume(min(100, self.vol_spinbox.value()))
        self._player.play()

    def _stop_preview(self):
        self._player.stop()

    def _on_player_state_changed(self, state):
        if state == QMediaPlayer.PlayingState:
            self.preview_btn.setText("■  停止")
        else:
            self.preview_btn.setText("▶  试听")

    # ------------------------------------------------------------------
    def _browse_out(self):
        path = QFileDialog.getExistingDirectory(self, "选择输出目录")
        if path:
            self.out_edit.setText(path)

    def _auto_fill_defaults(self, src_path: str):
        """当选择源文件后自动填充输出目录和数组名。"""
        if not src_path:
            return
        # Auto-fill output dir = same dir as source
        if not self.out_edit.text():
            self.out_edit.setText(os.path.dirname(src_path))
        # Auto-fill array name from filename
        if not self.name_edit.text():
            base = os.path.splitext(os.path.basename(src_path))[0]
            # Sanitize to C identifier
            safe = "".join(c if c.isalnum() or c == "_" else "_" for c in base)
            if safe and safe[0].isdigit():
                safe = "_" + safe
            self.name_edit.setText(f"g_remind_{safe.lower()}")

    # ------------------------------------------------------------------
    def _start_convert(self):
        src  = self.src_edit.text().strip()
        out  = self.out_edit.text().strip()
        name = self.name_edit.text().strip()

        if not src:
            QMessageBox.warning(self, "输入错误", "请选择源音频文件。")
            return
        if not os.path.isfile(src):
            QMessageBox.warning(self, "输入错误", f"文件不存在:\n{src}")
            return
        if not out:
            QMessageBox.warning(self, "输入错误", "请选择输出目录。")
            return
        if not name:
            QMessageBox.warning(self, "输入错误", "请填写 C 数组名称。")
            return
        # Validate C identifier
        if not name.replace("_", "").isalnum() or (name[0].isdigit()):
            QMessageBox.warning(self, "输入错误", f"数组名称 '{name}' 不是合法的 C 标识符。")
            return

        self.convert_btn.setEnabled(False)
        self.progress_bar.setValue(0)
        self.log_text.clear()
        self._append_log(f"开始转换: {os.path.basename(src)}")

        vol = self.vol_spinbox.value()
        ext = os.path.splitext(src)[1].lower()

        trim_enabled  = self.trim_check.isChecked()
        trim_start_ms = int(self.trim_start_spin.value() * 1000) if trim_enabled else 0
        trim_end_ms   = int(self.trim_end_spin.value() * 1000) if trim_enabled else 0

        # MP3 音量调整/裁剪必须在主线程用 QAudioDecoder 完成（Qt 多媒体对象不能在子线程使用）
        pre_data = None
        need_mp3_preprocess = ext == '.mp3' and (vol != 100 or trim_start_ms > 0 or trim_end_ms > 0)
        if need_mp3_preprocess:
            desc = []
            if vol != 100:
                desc.append(f"音量 {vol}%")
            if trim_start_ms > 0 or trim_end_ms > 0:
                desc.append(f"裁剪 {trim_start_ms/1000:.3f}s~{trim_end_ms/1000:.3f}s")
            self._append_log(f"预处理 MP3（{', '.join(desc)}）（主线程）...")
            self.statusBar().showMessage("正在预处理 MP3...")
            QApplication.processEvents()  # 刷新 UI，防止假死
            try:
                pre_data = self._preprocess_mp3(src, vol, trim_start_ms, trim_end_ms)
                self._append_log(f"MP3 预处理完成: {len(pre_data):,} 字节 ({len(pre_data)/1024:.1f} KB)")
            except Exception as e:
                self.convert_btn.setEnabled(True)
                self.statusBar().showMessage("MP3 预处理失败")
                self._append_log(f"[ERROR] MP3 预处理失败: {e}")
                QMessageBox.critical(self, "MP3 预处理失败", str(e))
                return

        self._worker = ConvertWorker(src, out, name, volume_pct=vol, pre_data=pre_data,
                                     trim_start_ms=trim_start_ms, trim_end_ms=trim_end_ms)
        self._worker.log_signal.connect(self._append_log)
        self._worker.progress.connect(self.progress_bar.setValue)
        self._worker.finished_ok.connect(self._on_success)
        self._worker.finished_err.connect(self._on_error)
        self._worker.start()

    def _append_log(self, msg: str):
        self.log_text.append(msg)
        self.log_text.moveCursor(QTextCursor.End)

    def _on_success(self, h_path: str, c_path: str):
        self.convert_btn.setEnabled(True)
        self.statusBar().showMessage("转换成功！")
        self._append_log("")
        self._append_log("== 使用方法 ==")
        self._append_log(f"1. 将 {os.path.basename(c_path)} 和 {os.path.basename(h_path)}")
        self._append_log( "   添加到 BanBox/src/banux/02_device_drivers/remind_sound/ 目录")
        self._append_log(f"2. 在 main.c 或 power_on() 中包含头文件:")
        name = self.name_edit.text().strip()
        h_name = os.path.basename(h_path)
        self._append_log(f'   #include "{h_name}"')
        self._append_log( "3. 调用播放函数:")
        self._append_log(f"   RemindSound_Play({name}, {name}_size);")
        QMessageBox.information(self, "转换完成", f"成功生成:\n{h_path}\n{c_path}")

    def _on_error(self, err: str):
        self.convert_btn.setEnabled(True)
        self.progress_bar.setValue(0)
        self._append_log(f"[ERROR] {err}")
        self.statusBar().showMessage("转换失败")
        QMessageBox.critical(self, "转换失败", err)

    def _copy_log(self):
        QApplication.clipboard().setText(self.log_text.toPlainText())
        self.statusBar().showMessage("日志已复制到剪贴板", 2000)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main():
    app = QApplication(sys.argv)
    apply_dark_theme(app)
    win = MainWindow()
    win.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()

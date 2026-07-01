#!/usr/bin/env python3
"""
WAV → C 数组转换器
将 WAV 音频文件转换为 C 语言 uint8_t/uint16_t 数组
"""

import sys
import struct
import os
from pathlib import Path

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QLabel, QLineEdit, QTextEdit, QFileDialog, QGroupBox,
    QRadioButton, QSpinBox, QFormLayout, QSplitter, QMessageBox,
    QFrame, QComboBox, QCheckBox, QScrollArea, QSizePolicy
)
from PyQt5.QtCore import Qt, QMimeData, QTimer
from PyQt5.QtGui import QFont, QDragEnterEvent, QDropEvent, QPalette, QColor


# ===================== WAV 解析 =====================

def parse_wav_header(filepath):
    """解析 WAV 头部, 返回 (sample_rate, bit_depth, channels, data_offset, data_size, fmt)"""
    with open(filepath, 'rb') as f:
        riff = f.read(4)
        if riff != b'RIFF':
            raise ValueError("不是有效的 RIFF 文件")
        _ = f.read(4)  # file size - 8
        wave = f.read(4)
        if wave != b'WAVE':
            raise ValueError("不是有效的 WAV 文件")

        fmt_tag = 0
        channels = 1
        sample_rate = 44100
        bit_depth = 16

        while True:
            chunk_id = f.read(4)
            if len(chunk_id) < 4:
                break
            chunk_size = struct.unpack('<I', f.read(4))[0]

            if chunk_id == b'fmt ':
                fmt_data = f.read(chunk_size)
                fmt_tag = struct.unpack('<H', fmt_data[0:2])[0]
                channels = struct.unpack('<H', fmt_data[2:4])[0]
                sample_rate = struct.unpack('<I', fmt_data[4:8])[0]
                bit_depth = struct.unpack('<H', fmt_data[14:16])[0]
            elif chunk_id == b'data':
                data_offset = f.tell()
                data_size = chunk_size
                return sample_rate, bit_depth, channels, data_offset, data_size, fmt_tag
            else:
                f.seek(chunk_size, 1)

    raise ValueError("未找到 data chunk")


def wav_to_c_array(filepath, array_name="audio_data", bytes_per_line=16,
                   fmt="hex", as_const=True, data_type="uint8_t"):
    """将 WAV 文件转换为 C 数组字符串"""
    sample_rate, bit_depth, channels, data_offset, data_size, fmt_tag = parse_wav_header(filepath)

    with open(filepath, 'rb') as f:
        f.seek(data_offset)
        raw_data = f.read(data_size)

    # 生成 C 数组
    lines = []
    qualifier = "const " if as_const else ""
    lines.append(f"// WAV → C Array")
    lines.append(f"// Sample Rate: {sample_rate} Hz, Bit Depth: {bit_depth}-bit, Channels: {channels}")
    lines.append(f"// Data Size: {data_size} bytes ({data_size / 1024:.1f} KB)")
    lines.append(f"{qualifier}{data_type} {array_name}[{data_size}] = {{")

    # 格式化数据
    if fmt == "hex":
        fmt_str = "0x{:02X}"
    else:
        fmt_str = "{:d}"

    values = []
    for byte in raw_data:
        values.append(fmt_str.format(byte))

    # 按 bytes_per_line 换行
    for i in range(0, len(values), bytes_per_line):
        line_vals = values[i:i + bytes_per_line]
        lines.append("    " + ", ".join(line_vals) + ",")

    lines.append("};")
    return "\n".join(lines)


# ===================== 现代化样式表 =====================

STYLE_DARK = """
QMainWindow {
    background-color: #1a1a2e;
}
QWidget {
    font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
    font-size: 13px;
    color: #e0e0e0;
}
QGroupBox {
    font-weight: bold;
    font-size: 14px;
    border: 1px solid #3a3a5c;
    border-radius: 8px;
    margin-top: 12px;
    padding-top: 16px;
    background-color: #16213e;
    color: #c0c0ff;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 16px;
    padding: 0 8px;
}
QLabel {
    color: #b0b0d0;
}
QPushButton {
    background-color: #0f3460;
    color: #e0e0ff;
    border: 1px solid #3a3a6c;
    border-radius: 6px;
    padding: 8px 18px;
    font-weight: bold;
    font-size: 13px;
}
QPushButton:hover {
    background-color: #1a4a8a;
    border-color: #5a5aff;
}
QPushButton:pressed {
    background-color: #0a2550;
}
QPushButton#btn_convert {
    background-color: #533483;
    color: #ffffff;
    font-size: 15px;
    padding: 10px 32px;
}
QPushButton#btn_convert:hover {
    background-color: #6a44b0;
}
QPushButton#btn_copy {
    background-color: #1e6f5c;
}
QPushButton#btn_copy:hover {
    background-color: #289672;
}
QPushButton#btn_save {
    background-color: #16213e;
}
QPushButton#btn_save:hover {
    background-color: #1f3460;
}
QLineEdit {
    background-color: #0d1b3e;
    border: 1px solid #3a3a5c;
    border-radius: 5px;
    padding: 6px 10px;
    color: #d0d0ff;
    font-size: 13px;
}
QLineEdit:focus {
    border-color: #6a6aff;
}
QTextEdit {
    background-color: #0d1117;
    border: 1px solid #3a3a5c;
    border-radius: 6px;
    padding: 10px;
    color: #c9d1d9;
    font-family: "Cascadia Code", "Consolas", "Courier New", monospace;
    font-size: 12px;
}
QSpinBox {
    background-color: #0d1b3e;
    border: 1px solid #3a3a5c;
    border-radius: 5px;
    padding: 4px 8px;
    color: #d0d0ff;
}
QComboBox {
    background-color: #0d1b3e;
    border: 1px solid #3a3a5c;
    border-radius: 5px;
    padding: 4px 8px;
    color: #d0d0ff;
}
QComboBox::drop-down {
    border: none;
}
QComboBox QAbstractItemView {
    background-color: #16213e;
    border: 1px solid #3a3a5c;
    selection-background-color: #533483;
}
QRadioButton, QCheckBox {
    color: #b0b0d0;
    spacing: 8px;
}
QSplitter::handle {
    background-color: #2a2a4a;
    width: 2px;
}
QScrollBar:vertical {
    background: #1a1a2e;
    width: 10px;
    border-radius: 5px;
}
QScrollBar::handle:vertical {
    background: #3a3a5c;
    border-radius: 5px;
    min-height: 30px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}
"""


# ===================== 主窗口 =====================

class Wav2ArrayWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("WAV → C Array 转换器")
        self.setMinimumSize(1050, 720)
        self.resize(1150, 780)
        self.setAcceptDrops(True)

        # 状态
        self.wav_path = ""
        self.wav_info = None
        self.output_text = ""

        self._build_ui()
        self._apply_style()

    # ---------- 拖放 ----------
    def dragEnterEvent(self, event: QDragEnterEvent):
        if event.mimeData().hasUrls():
            event.acceptProposedAction()
            self._highlight_drop(True)

    def dragLeaveEvent(self, event):
        self._highlight_drop(False)

    def dropEvent(self, event: QDropEvent):
        self._highlight_drop(False)
        for url in event.mimeData().urls():
            path = url.toLocalFile()
            if path.lower().endswith('.wav'):
                self._load_wav(path)
                return
        QMessageBox.warning(self, "提示", "请拖入 .wav 文件")

    def _highlight_drop(self, active):
        if active:
            self.drop_hint.setStyleSheet(
                "border: 2px dashed #6a6aff; border-radius: 10px;"
                "background-color: rgba(106,106,255,0.08); color: #6a6aff;"
            )
        else:
            self.drop_hint.setStyleSheet(
                "border: 2px dashed #3a3a5c; border-radius: 10px;"
                "background-color: transparent; color: #6a6a80;"
            )

    # ---------- 界面构建 ----------
    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(24, 20, 24, 20)
        root.setSpacing(16)

        # --- 标题栏 ---
        title = QLabel("🔊  WAV → C Array 转换器")
        title.setAlignment(Qt.AlignCenter)
        title.setStyleSheet(
            "font-size: 24px; font-weight: bold; color: #c0c0ff;"
            "padding: 8px 0;"
        )
        root.addWidget(title)

        # --- 拖放区 ---
        self.drop_hint = QLabel("拖放 .wav 文件到此处\n或点击下方按钮选择文件")
        self.drop_hint.setAlignment(Qt.AlignCenter)
        self.drop_hint.setFixedHeight(86)
        self.drop_hint.setStyleSheet(
            "border: 2px dashed #3a3a5c; border-radius: 10px;"
            "background-color: transparent; color: #6a6a80; font-size: 14px;"
        )
        root.addWidget(self.drop_hint)

        # --- 文件选择行 ---
        file_row = QHBoxLayout()
        self.lbl_path = QLineEdit()
        self.lbl_path.setReadOnly(True)
        self.lbl_path.setPlaceholderText("未选择文件...")
        file_row.addWidget(self.lbl_path, 1)

        btn_browse = QPushButton("📂 选择文件")
        btn_browse.setObjectName("btn_save")
        btn_browse.clicked.connect(self._browse_file)
        file_row.addWidget(btn_browse)
        root.addLayout(file_row)

        # --- 中间分割区 ---
        splitter = QSplitter(Qt.Vertical)
        root.addWidget(splitter, 1)

        # ---- 上半部：文件信息 + 选项 ----
        top_panel = QWidget()
        top_layout = QHBoxLayout(top_panel)
        top_layout.setContentsMargins(0, 0, 0, 0)

        # 文件信息
        info_group = QGroupBox("📋 文件信息")
        info_layout = QVBoxLayout(info_group)
        self.info_text = QLabel("等待加载文件...")
        self.info_text.setWordWrap(True)
        self.info_text.setStyleSheet("font-size: 13px; color: #a0a0c0; padding: 6px;")
        info_layout.addWidget(self.info_text)
        info_layout.addStretch()
        top_layout.addWidget(info_group, 1)

        # 选项
        opt_group = QGroupBox("⚙️ 输出选项")
        opt_layout = QFormLayout(opt_group)
        opt_layout.setSpacing(10)

        self.edit_name = QLineEdit("audio_data")
        opt_layout.addRow("数组名:", self.edit_name)

        self.combo_type = QComboBox()
        self.combo_type.addItems(["uint8_t", "uint16_t", "int8_t", "int16_t", "int32_t", "float"])
        opt_layout.addRow("数据类型:", self.combo_type)

        self.spin_cols = QSpinBox()
        self.spin_cols.setRange(4, 64)
        self.spin_cols.setValue(16)
        opt_layout.addRow("每行列数:", self.spin_cols)

        fmt_layout = QHBoxLayout()
        self.radio_hex = QRadioButton("HEX (0x00)")
        self.radio_hex.setChecked(True)
        self.radio_dec = QRadioButton("DEC (数字)")
        self.radio_hex.toggled.connect(self._on_fmt_changed)
        fmt_layout.addWidget(self.radio_hex)
        fmt_layout.addWidget(self.radio_dec)
        fmt_layout.addStretch()
        opt_layout.addRow("数值格式:", fmt_layout)

        self.chk_const = QCheckBox("使用 const 限定符")
        self.chk_const.setChecked(True)
        opt_layout.addRow("", self.chk_const)

        top_layout.addWidget(opt_group, 1)
        splitter.addWidget(top_panel)

        # ---- 下半部：输出预览 ----
        output_group = QGroupBox("💻 生成的 C 数组")
        output_layout = QVBoxLayout(output_group)

        self.text_output = QTextEdit()
        self.text_output.setReadOnly(True)
        self.text_output.setPlaceholderText("点击「转换」生成 C 数组...")
        output_layout.addWidget(self.text_output, 1)

        btn_row = QHBoxLayout()
        btn_row.addStretch()

        self.btn_copy = QPushButton("📋 复制到剪贴板")
        self.btn_copy.setObjectName("btn_copy")
        self.btn_copy.clicked.connect(self._copy_to_clipboard)
        btn_row.addWidget(self.btn_copy)

        self.btn_save = QPushButton("💾 保存为 .h 文件")
        self.btn_save.setObjectName("btn_save")
        self.btn_save.clicked.connect(self._save_to_file)
        btn_row.addWidget(self.btn_save)
        output_layout.addLayout(btn_row)
        splitter.addWidget(output_group)

        splitter.setStretchFactor(0, 2)
        splitter.setStretchFactor(1, 5)

        # --- 底部转换按钮 ---
        bottom = QHBoxLayout()
        self.lbl_status = QLabel("就绪")
        self.lbl_status.setStyleSheet("color: #707090;")
        bottom.addWidget(self.lbl_status)
        bottom.addStretch()

        self.btn_convert = QPushButton("🚀 转换")
        self.btn_convert.setObjectName("btn_convert")
        self.btn_convert.setFixedSize(160, 44)
        self.btn_convert.clicked.connect(self._convert)
        bottom.addWidget(self.btn_convert)
        root.addLayout(bottom)

    # ---------- 操作逻辑 ----------
    def _browse_file(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "选择 WAV 文件", "", "WAV Files (*.wav);;All Files (*)"
        )
        if path:
            self._load_wav(path)

    def _load_wav(self, path):
        try:
            info = parse_wav_header(path)
            self.wav_path = path
            self.wav_info = info
            self.lbl_path.setText(path)

            sr, bd, ch, _, dsize, ftag = info
            duration = dsize / (sr * ch * bd / 8) if sr and ch and bd else 0
            ftag_names = {1: "PCM", 3: "IEEE Float", 6: "A-law", 7: "μ-law"}
            ftag_str = ftag_names.get(ftag, f"未知 ({ftag})")

            info_lines = [
                f"<b>文件:</b> {Path(path).name}",
                f"<b>采样率:</b> {sr} Hz",
                f"<b>位深度:</b> {bd}-bit",
                f"<b>声道数:</b> {ch} ({'单声道' if ch == 1 else '立体声' if ch == 2 else str(ch)})",
                f"<b>格式:</b> {ftag_str}",
                f"<b>数据大小:</b> {dsize:,} bytes ({dsize/1024:.1f} KB)",
                f"<b>时长:</b> {duration:.2f} 秒",
            ]
            self.info_text.setText("<br>".join(info_lines))
            self.lbl_status.setText(f"已加载: {Path(path).name}")
            self.lbl_status.setStyleSheet("color: #80c080;")

            # 清空旧输出
            self.text_output.clear()
            self.output_text = ""

            # 自动推测数据类型
            if bd == 16:
                self.combo_type.setCurrentText("int16_t")
            elif bd == 8:
                self.combo_type.setCurrentText("uint8_t")
            elif bd == 24 or bd == 32:
                if ftag == 3:
                    self.combo_type.setCurrentText("float")
                else:
                    self.combo_type.setCurrentText("int32_t")

        except Exception as e:
            QMessageBox.critical(self, "错误", f"无法打开 WAV 文件:\n{str(e)}")
            self.lbl_status.setText("加载失败")
            self.lbl_status.setStyleSheet("color: #e06060;")

    def _on_fmt_changed(self):
        if self.output_text:
            self._convert()

    def _convert(self):
        if not self.wav_path or not self.wav_info:
            QMessageBox.warning(self, "提示", "请先选择 WAV 文件")
            return

        try:
            name = self.edit_name.text().strip() or "audio_data"
            cols = self.spin_cols.value()
            fmt = "hex" if self.radio_hex.isChecked() else "dec"
            const = self.chk_const.isChecked()
            dtype = self.combo_type.currentText()

            self.output_text = wav_to_c_array(
                self.wav_path, array_name=name,
                bytes_per_line=cols, fmt=fmt,
                as_const=const, data_type=dtype
            )
            self.text_output.setPlainText(self.output_text)

            _, _, _, _, dsize, _ = self.wav_info
            self.lbl_status.setText(f"转换完成 — {dsize:,} bytes")
            self.lbl_status.setStyleSheet("color: #80c080;")

        except Exception as e:
            QMessageBox.critical(self, "错误", f"转换失败:\n{str(e)}")
            self.lbl_status.setText("转换失败")
            self.lbl_status.setStyleSheet("color: #e06060;")

    def _copy_to_clipboard(self):
        if not self.output_text:
            QMessageBox.warning(self, "提示", "请先转换文件")
            return
        QApplication.clipboard().setText(self.output_text)
        self.lbl_status.setText("已复制到剪贴板!")
        self.lbl_status.setStyleSheet("color: #80c0ff;")
        QTimer.singleShot(2000, lambda: self.lbl_status.setText("就绪"))
        QTimer.singleShot(2000, lambda: self.lbl_status.setStyleSheet("color: #707090;"))

    def _save_to_file(self):
        if not self.output_text:
            QMessageBox.warning(self, "提示", "请先转换文件")
            return
        default_name = self.edit_name.text().strip() or "audio_data"
        path, _ = QFileDialog.getSaveFileName(
            self, "保存 C 头文件", f"{default_name}.h",
            "Header Files (*.h);;C Source Files (*.c);;All Files (*)"
        )
        if path:
            with open(path, 'w', encoding='utf-8') as f:
                f.write(self.output_text)
                f.write('\n')
            self.lbl_status.setText(f"已保存: {Path(path).name}")
            self.lbl_status.setStyleSheet("color: #80c080;")

    def _apply_style(self):
        self.setStyleSheet(STYLE_DARK)


# ===================== 入口 =====================

def main():
    app = QApplication(sys.argv)
    app.setApplicationName("WAV2Array")

    # 全局字体
    font = QFont()
    font.setFamilies(["Segoe UI", "Microsoft YaHei", "sans-serif"])
    font.setPixelSize(13)
    app.setFont(font)

    window = Wav2ArrayWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()

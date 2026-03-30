#!/usr/bin/env python3
"""
bg_bootloader.py — BG Bootloader Host Tool v2.0  (PyQt6 GUI)
USB CDC 固件升级 & 跳转上位机

启动后自动扫描所有串口，发送握手包探测 Bootloader 设备并自动连接。

依赖:
  pip install pyserial PyQt6
"""

import sys
import os
from pathlib import Path

from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QFont, QColor, QIcon
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget,
    QVBoxLayout, QHBoxLayout, QGridLayout, QGroupBox,
    QLabel, QComboBox, QPushButton, QProgressBar,
    QTextEdit, QFileDialog, QCheckBox, QMessageBox,
)

from bl_core import list_ports
from worker import AutoScanWorker, UpgradeWorker


# ═══════════════════════════════════════════════════════════════════════════════
#  MainWindow
# ═══════════════════════════════════════════════════════════════════════════════
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("BG Bootloader — USB CDC 升级工具 v2.0")
        self.setMinimumSize(680, 520)

        self._fw_path: str = ""
        self._worker: UpgradeWorker | None = None
        self._scan_worker: AutoScanWorker | None = None

        self._build_ui()
        self._connect_signals()

        # 启动后自动扫描
        QTimer.singleShot(200, self._on_auto_scan)

    # ────────────────────────────────────────────────────────────────────────
    #  UI 构建
    # ────────────────────────────────────────────────────────────────────────
    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(10, 10, 10, 10)
        root.setSpacing(8)

        # ── 连接区 ──────────────────────────────────────────────────────────
        grp_conn = QGroupBox("连接")
        grid = QGridLayout(grp_conn)

        grid.addWidget(QLabel("串口:"), 0, 0)
        self.combo_port = QComboBox()
        self.combo_port.setMinimumWidth(180)
        grid.addWidget(self.combo_port, 0, 1)

        grid.addWidget(QLabel("波特率:"), 0, 2)
        self.combo_baud = QComboBox()
        self.combo_baud.addItems(["9600", "19200", "38400", "57600",
                                  "115200", "230400", "460800", "921600"])
        self.combo_baud.setCurrentText("115200")
        grid.addWidget(self.combo_baud, 0, 3)

        self.btn_refresh = QPushButton("刷新")
        grid.addWidget(self.btn_refresh, 0, 4)

        self.btn_scan = QPushButton("自动扫描")
        grid.addWidget(self.btn_scan, 0, 5)

        self.lbl_status = QLabel("● 未连接")
        self.lbl_status.setStyleSheet("color: gray; font-weight: bold;")
        grid.addWidget(self.lbl_status, 0, 6)

        grid.setColumnStretch(1, 1)
        root.addWidget(grp_conn)

        # ── 固件区 ──────────────────────────────────────────────────────────
        grp_fw = QGroupBox("固件")
        h_fw = QHBoxLayout(grp_fw)
        self.lbl_fw = QLabel("未选择固件文件")
        self.lbl_fw.setStyleSheet("color: gray;")
        h_fw.addWidget(self.lbl_fw, 1)
        self.btn_browse = QPushButton("浏览 …")
        h_fw.addWidget(self.btn_browse)
        root.addWidget(grp_fw)

        # ── 操作区 ──────────────────────────────────────────────────────────
        grp_ops = QGroupBox("操作")
        h_ops = QHBoxLayout(grp_ops)

        self.btn_ping = QPushButton("握手 (Ping)")
        self.btn_erase = QPushButton("擦除 (Erase)")
        self.btn_upgrade = QPushButton("升级 (Upgrade)")
        self.btn_jump = QPushButton("跳转 (Jump)")
        self.chk_auto_jump = QCheckBox("升级后自动跳转")
        self.chk_auto_jump.setChecked(True)

        for btn in (self.btn_ping, self.btn_erase,
                    self.btn_upgrade, self.btn_jump):
            btn.setMinimumHeight(36)
            h_ops.addWidget(btn)
        h_ops.addWidget(self.chk_auto_jump)

        root.addWidget(grp_ops)

        # ── 进度条 ──────────────────────────────────────────────────────────
        self.progress = QProgressBar()
        self.progress.setTextVisible(True)
        self.progress.setValue(0)
        root.addWidget(self.progress)

        # ── 日志区 ──────────────────────────────────────────────────────────
        grp_log = QGroupBox("日志")
        v_log = QVBoxLayout(grp_log)
        self.txt_log = QTextEdit()
        self.txt_log.setReadOnly(True)
        self.txt_log.setFont(QFont("Consolas", 9))
        v_log.addWidget(self.txt_log)

        h_log_btns = QHBoxLayout()
        self.btn_clear_log = QPushButton("清空日志")
        h_log_btns.addStretch()
        h_log_btns.addWidget(self.btn_clear_log)
        v_log.addLayout(h_log_btns)

        root.addWidget(grp_log, 1)   # stretch=1 让日志区占满剩余空间

    # ────────────────────────────────────────────────────────────────────────
    #  信号绑定
    # ────────────────────────────────────────────────────────────────────────
    def _connect_signals(self):
        self.btn_refresh.clicked.connect(self._refresh_ports)
        self.btn_scan.clicked.connect(self._on_auto_scan)
        self.btn_browse.clicked.connect(self._browse_firmware)
        self.btn_ping.clicked.connect(lambda: self._start_op(UpgradeWorker.OP_PING))
        self.btn_erase.clicked.connect(lambda: self._start_op(UpgradeWorker.OP_ERASE))
        self.btn_upgrade.clicked.connect(lambda: self._start_op(UpgradeWorker.OP_UPGRADE))
        self.btn_jump.clicked.connect(lambda: self._start_op(UpgradeWorker.OP_JUMP))
        self.btn_clear_log.clicked.connect(self.txt_log.clear)

    # ────────────────────────────────────────────────────────────────────────
    #  串口列表
    # ────────────────────────────────────────────────────────────────────────
    def _refresh_ports(self):
        current = self.combo_port.currentText()
        self.combo_port.clear()
        ports = list_ports()
        for device, desc in ports:
            self.combo_port.addItem(f"{device}  —  {desc}", userData=device)
        # 尝试恢复之前的选择
        for i in range(self.combo_port.count()):
            if self.combo_port.itemData(i) == current:
                self.combo_port.setCurrentIndex(i)
                break

    # ────────────────────────────────────────────────────────────────────────
    #  自动扫描
    # ────────────────────────────────────────────────────────────────────────
    def _on_auto_scan(self):
        if self._scan_worker and self._scan_worker.isRunning():
            return
        self._refresh_ports()
        baud = int(self.combo_baud.currentText())
        self._scan_worker = AutoScanWorker(baud, parent=self)
        self._scan_worker.log.connect(self._append_log)
        self._scan_worker.found.connect(self._on_device_found)
        self._scan_worker.scan_finished.connect(self._on_scan_finished)
        self._set_busy(True, scanning=True)
        self._scan_worker.start()

    def _on_device_found(self, port: str, desc: str, ver: int):
        # 选中找到的端口
        for i in range(self.combo_port.count()):
            if self.combo_port.itemData(i) == port:
                self.combo_port.setCurrentIndex(i)
                break
        self.lbl_status.setText(f"● 已连接  {port}  (v{ver})")
        self.lbl_status.setStyleSheet("color: green; font-weight: bold;")

    def _on_scan_finished(self, found: bool, msg: str):
        self._set_busy(False)
        if not found:
            self.lbl_status.setText("● 未连接")
            self.lbl_status.setStyleSheet("color: gray; font-weight: bold;")
            self._append_log(msg)

    # ────────────────────────────────────────────────────────────────────────
    #  固件选择
    # ────────────────────────────────────────────────────────────────────────
    def _browse_firmware(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "选择固件文件", "",
            "二进制文件 (*.bin);;所有文件 (*)")
        if path:
            self._fw_path = path
            size = os.path.getsize(path)
            name = Path(path).name
            self.lbl_fw.setText(f"{name}  ({size:,} 字节)")
            self.lbl_fw.setStyleSheet("color: black;")

    # ────────────────────────────────────────────────────────────────────────
    #  操作执行
    # ────────────────────────────────────────────────────────────────────────
    def _start_op(self, operation: str):
        if self._worker and self._worker.isRunning():
            QMessageBox.warning(self, "操作进行中", "请等待当前操作完成")
            return

        port_data = self.combo_port.currentData()
        if not port_data:
            QMessageBox.warning(self, "未选择串口", "请先选择串口或等待自动扫描完成")
            return

        firmware = b""
        if operation == UpgradeWorker.OP_UPGRADE:
            if not self._fw_path:
                QMessageBox.warning(self, "未选择固件", "请先选择固件文件")
                return
            with open(self._fw_path, "rb") as f:
                firmware = f.read()
            if not firmware:
                QMessageBox.warning(self, "固件为空", "固件文件内容为空")
                return

        baud = int(self.combo_baud.currentText())
        auto_jump = self.chk_auto_jump.isChecked()

        self.progress.setValue(0)
        self._worker = UpgradeWorker(
            port_data, baud, operation,
            firmware=firmware, auto_jump=auto_jump, parent=self)
        self._worker.log.connect(self._append_log)
        self._worker.progress.connect(self._on_progress)
        self._worker.finished.connect(self._on_op_finished)
        self._set_busy(True)
        self._worker.start()

    def _on_progress(self, sent: int, total: int):
        if total > 0:
            self.progress.setMaximum(total)
            self.progress.setValue(sent)

    def _on_op_finished(self, success: bool, msg: str):
        self._set_busy(False)
        self._append_log(msg)
        if success:
            self.lbl_status.setText("● 已连接")
            self.lbl_status.setStyleSheet("color: green; font-weight: bold;")
        else:
            self.lbl_status.setText("● 操作失败")
            self.lbl_status.setStyleSheet("color: red; font-weight: bold;")

    # ────────────────────────────────────────────────────────────────────────
    #  辅助
    # ────────────────────────────────────────────────────────────────────────
    def _append_log(self, msg: str):
        self.txt_log.append(msg)
        sb = self.txt_log.verticalScrollBar()
        sb.setValue(sb.maximum())

    def _set_busy(self, busy: bool, scanning: bool = False):
        """禁用 / 启用操作按钮。"""
        enabled = not busy
        self.btn_ping.setEnabled(enabled)
        self.btn_erase.setEnabled(enabled)
        self.btn_upgrade.setEnabled(enabled)
        self.btn_jump.setEnabled(enabled)
        self.btn_browse.setEnabled(enabled)
        self.btn_scan.setEnabled(enabled)

    def closeEvent(self, event):
        # 确保后台线程退出
        if self._scan_worker and self._scan_worker.isRunning():
            self._scan_worker.abort()
            self._scan_worker.wait(2000)
        if self._worker and self._worker.isRunning():
            self._worker.abort()
            self._worker.wait(2000)
        event.accept()


# ═══════════════════════════════════════════════════════════════════════════════
#  入口
# ═══════════════════════════════════════════════════════════════════════════════
def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    win = MainWindow()
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

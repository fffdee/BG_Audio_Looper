"""
worker.py — 后台线程
1. AutoScanWorker  — 启动时自动扫描所有串口，发送握手探测
2. UpgradeWorker   — 耗时操作（握手、擦除、写 Flash、跳转）
"""

from PyQt5.QtCore import QThread, pyqtSignal
from bl_core import BLComm, Bootloader, list_ports, probe_port


# ═══════════════════════════════════════════════════════════════════════════════
#  AutoScanWorker — 扫描所有串口，逐个发握手包，找到 Bootloader 即返回
# ═══════════════════════════════════════════════════════════════════════════════
class AutoScanWorker(QThread):
    """
    信号:
        log(str)                           — 日志
        found(str, str, int)               — (port, description, protocol_ver)
        scan_finished(bool, str)           — (found_any?, message)
    """
    log             = pyqtSignal(str)
    found           = pyqtSignal(str, str, int)
    scan_finished   = pyqtSignal(bool, str)

    def __init__(self, baudrate: int = 115200, parent=None):
        super().__init__(parent)
        self._baud = baudrate
        self._abort = False

    def abort(self):
        self._abort = True

    def run(self):
        ports = list_ports()
        if not ports:
            self.log.emit("未发现任何串口")
            self.scan_finished.emit(False, "无可用串口")
            return

        self.log.emit(f"正在扫描 {len(ports)} 个串口 …")
        found_any = False
        for device, desc in ports:
            if self._abort:
                break
            self.log.emit(f"  探测 {device} ({desc}) …")
            ver = probe_port(device, self._baud, timeout=0.5)
            if ver is not None:
                self.log.emit(f"  ✓ {device} — Bootloader v{ver}")
                self.found.emit(device, desc, ver)
                found_any = True
                break                     # 找到第一个即停止
            else:
                self.log.emit(f"  ✗ {device} 无应答")

        if found_any:
            self.scan_finished.emit(True, "已找到设备")
        else:
            self.scan_finished.emit(False, "未发现 Bootloader 设备")


class UpgradeWorker(QThread):
    """
    信号:
        log(str)              — 一行日志文本
        progress(int, int)    — (已完成字节, 总字节)
        finished(bool, str)   — (成功?, 结果消息)
    """

    log      = pyqtSignal(str)
    progress = pyqtSignal(int, int)
    finished = pyqtSignal(bool, str)

    # 操作类型常量
    OP_PING    = "ping"
    OP_ERASE   = "erase"
    OP_UPGRADE = "upgrade"
    OP_JUMP    = "jump"

    def __init__(self, port: str, baud: int, operation: str,
                 firmware: bytes = b"", auto_jump: bool = False,
                 parent=None):
        super().__init__(parent)
        self._port      = port
        self._baud      = baud
        self._operation = operation
        self._firmware  = firmware
        self._auto_jump = auto_jump
        self._abort     = False

    def abort(self):
        """请求中止（当前正执行的 transact 会在超时后停止）。"""
        self._abort = True

    # ─── 内部辅助 ─────────────────────────────────────────────────────────────

    def _emit_log(self, msg: str):
        self.log.emit(msg)

    def _emit_progress(self, sent: int, total: int):
        self.progress.emit(sent, total)

    # ─── 线程入口 ─────────────────────────────────────────────────────────────

    def run(self):
        comm = None
        try:
            self._emit_log(f"正在连接 {self._port} @ {self._baud} bps …")
            comm = BLComm(self._port, self._baud)
            bl   = Bootloader(comm,
                               progress_cb=self._emit_progress,
                               log_cb=self._emit_log)

            if self._operation == self.OP_PING:
                bl.ping()

            elif self._operation == self.OP_ERASE:
                bl.ping()
                bl.erase()

            elif self._operation == self.OP_UPGRADE:
                if not self._firmware:
                    raise RuntimeError("未选择固件文件")
                bl.ping()
                bl.upgrade(self._firmware)
                if self._auto_jump:
                    bl.jump()

            elif self._operation == self.OP_JUMP:
                bl.jump()

            else:
                raise RuntimeError(f"未知操作: {self._operation}")

            self.finished.emit(True, "操作成功完成")

        except Exception as exc:
            self.finished.emit(False, str(exc))
        finally:
            if comm:
                comm.close()

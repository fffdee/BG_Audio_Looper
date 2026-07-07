#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
PyInstaller GUI 上位机 - 带UI界面的Python打包为exe工具
支持选择py文件、配置打包参数、实时查看打包进度
"""

import sys
import os
import subprocess
import threading
import json
from pathlib import Path
from datetime import datetime

try:
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox, scrolledtext
except ImportError:
    print("错误：需要 tkinter 模块，请安装 Python 时勾选 tcl/tk 选项。")
    sys.exit(1)


class PyInstallerGUI:
    """PyInstaller 图形化打包上位机"""

    CONFIG_FILE = Path.home() / ".pyinstaller_gui_config.json"

    def __init__(self):
        self.root = tk.Tk()
        self.root.title("PyInstaller 打包上位机 v1.0")
        self.root.geometry("900x700")
        self.root.minsize(800, 600)

        # 颜色方案
        self.colors = {
            "bg": "#1e1e2e",
            "fg": "#cdd6f4",
            "accent": "#89b4fa",
            "btn_bg": "#313244",
            "btn_fg": "#cdd6f4",
            "entry_bg": "#313244",
            "entry_fg": "#cdd6f4",
            "output_bg": "#11111b",
            "output_fg": "#a6e3a1",
            "error_fg": "#f38ba8",
            "warn_fg": "#f9e2af",
            "success": "#a6e3a1",
            "hover_btn": "#45475a",
            "select_bg": "#45475a",
            "progress_bg": "#313244",
            "progress_fg": "#89b4fa",
        }

        self.root.configure(bg=self.colors["bg"])

        # 设置 ttk 样式
        self._setup_ttk_style()

        # 变量
        self.py_file_path = tk.StringVar()
        self.output_dir = tk.StringVar(value=str(Path.cwd() / "dist"))
        self.icon_path = tk.StringVar()
        self.app_name = tk.StringVar()
        self.onefile_var = tk.BooleanVar(value=True)
        self.console_var = tk.BooleanVar(value=False)
        self.clean_var = tk.BooleanVar(value=True)
        self.noconfirm_var = tk.BooleanVar(value=True)
        self.uac_var = tk.BooleanVar(value=False)
        self.add_data_var = tk.StringVar()
        self.hidden_imports_var = tk.StringVar()
        self.exclude_modules_var = tk.StringVar()
        self.extra_args_var = tk.StringVar()
        self.process = None
        self.build_running = False

        self._load_config()
        self._create_widgets()
        self._center_window()

    def _setup_ttk_style(self):
        """配置 ttk 样式"""
        style = ttk.Style()
        style.theme_use("clam")

        style.configure(
            "TFrame",
            background=self.colors["bg"],
        )
        style.configure(
            "TLabel",
            background=self.colors["bg"],
            foreground=self.colors["fg"],
            font=("Microsoft YaHei UI", 10),
        )
        style.configure(
            "TButton",
            background=self.colors["btn_bg"],
            foreground=self.colors["btn_fg"],
            borderwidth=0,
            font=("Microsoft YaHei UI", 10),
            padding=(12, 6),
        )
        style.map(
            "TButton",
            background=[("active", self.colors["hover_btn"]),
                       ("!disabled", self.colors["btn_bg"])],
        )
        style.configure(
            "TCheckbutton",
            background=self.colors["bg"],
            foreground=self.colors["fg"],
            font=("Microsoft YaHei UI", 10),
        )
        style.map(
            "TCheckbutton",
            background=[("active", self.colors["bg"])],
        )
        style.configure(
            "TEntry",
            fieldbackground=self.colors["entry_bg"],
            foreground=self.colors["entry_fg"],
            insertcolor=self.colors["fg"],
            font=("Microsoft YaHei UI", 10),
        )
        style.configure(
            "TProgressbar",
            troughcolor=self.colors["progress_bg"],
            background=self.colors["progress_fg"],
            thickness=8,
        )
        style.configure(
            "TLabelframe",
            background=self.colors["bg"],
            foreground=self.colors["accent"],
            font=("Microsoft YaHei UI", 11, "bold"),
        )
        style.configure(
            "TLabelframe.Label",
            background=self.colors["bg"],
            foreground=self.colors["accent"],
            font=("Microsoft YaHei UI", 11, "bold"),
        )

    def _create_widgets(self):
        """创建界面组件"""
        # 顶部标题栏
        title_frame = tk.Frame(self.root, bg=self.colors["bg"], pady=15)
        title_frame.pack(fill=tk.X)

        title_label = tk.Label(
            title_frame,
            text="🐍 PyInstaller 打包上位机",
            font=("Microsoft YaHei UI", 20, "bold"),
            bg=self.colors["bg"],
            fg=self.colors["accent"],
        )
        title_label.pack()

        subtitle = tk.Label(
            title_frame,
            text="将 Python 脚本一键打包为 Windows 可执行文件 (.exe)",
            font=("Microsoft YaHei UI", 10),
            bg=self.colors["bg"],
            fg=self.colors["fg"],
        )
        subtitle.pack(pady=(5, 0))

        # 主内容区域 - 使用 PanedWindow
        main_paned = tk.PanedWindow(
            self.root,
            orient=tk.VERTICAL,
            bg=self.colors["bg"],
            sashwidth=3,
            sashrelief=tk.RAISED,
        )
        main_paned.pack(fill=tk.BOTH, expand=True, padx=15, pady=(0, 10))

        # 上半部分：设置区域（可滚动）
        canvas = tk.Canvas(main_paned, bg=self.colors["bg"], highlightthickness=0)
        scrollbar = ttk.Scrollbar(main_paned, orient=tk.VERTICAL, command=canvas.yview)
        settings_frame = tk.Frame(canvas, bg=self.colors["bg"])

        settings_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all")),
        )

        canvas.create_window((0, 0), window=settings_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)

        # 鼠标滚轮滚动
        def _on_mousewheel(event):
            canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

        canvas.bind_all("<MouseWheel>", _on_mousewheel)

        # ========== 源文件选择 ==========
        file_frame = ttk.Labelframe(settings_frame, text="📁 源文件", padding=12)
        file_frame.pack(fill=tk.X, padx=5, pady=(10, 5))

        row1 = tk.Frame(file_frame, bg=self.colors["bg"])
        row1.pack(fill=tk.X, pady=(0, 5))
        ttk.Label(row1, text="Python 脚本:").pack(side=tk.LEFT, padx=(0, 10))
        ttk.Entry(row1, textvariable=self.py_file_path, width=55).pack(
            side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 5)
        )
        ttk.Button(row1, text="浏览...", command=self._browse_py).pack(side=tk.LEFT)

        row2 = tk.Frame(file_frame, bg=self.colors["bg"])
        row2.pack(fill=tk.X, pady=(5, 0))
        ttk.Label(row2, text="输出目录:").pack(side=tk.LEFT, padx=(0, 10))
        ttk.Entry(row2, textvariable=self.output_dir, width=55).pack(
            side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 5)
        )
        ttk.Button(row2, text="浏览...", command=self._browse_output).pack(side=tk.LEFT)

        # ========== 基本设置 ==========
        basic_frame = ttk.Labelframe(settings_frame, text="⚙️ 基本设置", padding=12)
        basic_frame.pack(fill=tk.X, padx=5, pady=5)

        basic_grid = tk.Frame(basic_frame, bg=self.colors["bg"])
        basic_grid.pack(fill=tk.X)

        # 行1
        ttk.Label(basic_grid, text="应用名称:").grid(
            row=0, column=0, sticky=tk.W, padx=(0, 10), pady=5
        )
        ttk.Entry(basic_grid, textvariable=self.app_name, width=30).grid(
            row=0, column=1, sticky=tk.EW, pady=5
        )
        ttk.Label(basic_grid, text="图标文件:").grid(
            row=0, column=2, sticky=tk.W, padx=(20, 10), pady=5
        )
        icon_row = tk.Frame(basic_grid, bg=self.colors["bg"])
        icon_row.grid(row=0, column=3, sticky=tk.EW, pady=5)
        ttk.Entry(icon_row, textvariable=self.icon_path, width=25).pack(
            side=tk.LEFT, fill=tk.X, expand=True
        )
        ttk.Button(icon_row, text="...", width=4, command=self._browse_icon).pack(
            side=tk.LEFT, padx=(5, 0)
        )

        basic_grid.columnconfigure(1, weight=1)
        basic_grid.columnconfigure(3, weight=1)

        # ========== 打包选项 ==========
        options_frame = ttk.Labelframe(settings_frame, text="📦 打包选项", padding=12)
        options_frame.pack(fill=tk.X, padx=5, pady=5)

        opt_grid = tk.Frame(options_frame, bg=self.colors["bg"])
        opt_grid.pack(fill=tk.X)

        ttk.Checkbutton(opt_grid, text="单文件模式 (--onefile)", variable=self.onefile_var).grid(
            row=0, column=0, sticky=tk.W, padx=(0, 30), pady=3
        )
        ttk.Checkbutton(opt_grid, text="显示控制台窗口 (--console)", variable=self.console_var).grid(
            row=0, column=1, sticky=tk.W, padx=(0, 30), pady=3
        )
        ttk.Checkbutton(opt_grid, text="清理临时文件 (--clean)", variable=self.clean_var).grid(
            row=0, column=2, sticky=tk.W, pady=3
        )

        ttk.Checkbutton(opt_grid, text="覆盖确认 (--noconfirm)", variable=self.noconfirm_var).grid(
            row=1, column=0, sticky=tk.W, padx=(0, 30), pady=3
        )
        ttk.Checkbutton(opt_grid, text="管理员权限 (--uac-admin)", variable=self.uac_var).grid(
            row=1, column=1, sticky=tk.W, padx=(0, 30), pady=3
        )

        # ========== 高级设置 ==========
        adv_frame = ttk.Labelframe(settings_frame, text="🔧 高级设置", padding=12)
        adv_frame.pack(fill=tk.X, padx=5, pady=5)

        adv_grid = tk.Frame(adv_frame, bg=self.colors["bg"])
        adv_grid.pack(fill=tk.X)

        ttk.Label(adv_grid, text="附加数据 (--add-data):").grid(
            row=0, column=0, sticky=tk.W, padx=(0, 10), pady=5
        )
        ttk.Entry(adv_grid, textvariable=self.add_data_var, width=60).grid(
            row=0, column=1, columnspan=2, sticky=tk.EW, pady=5
        )
        self._add_help(adv_grid, "格式: 源路径;目标路径 (多个用逗号分隔)", 0, 3)

        ttk.Label(adv_grid, text="隐藏导入 (--hidden-import):").grid(
            row=1, column=0, sticky=tk.W, padx=(0, 10), pady=5
        )
        ttk.Entry(adv_grid, textvariable=self.hidden_imports_var, width=60).grid(
            row=1, column=1, columnspan=2, sticky=tk.EW, pady=5
        )
        self._add_help(adv_grid, "格式: module1,module2", 1, 3)

        ttk.Label(adv_grid, text="排除模块 (--exclude-module):").grid(
            row=2, column=0, sticky=tk.W, padx=(0, 10), pady=5
        )
        ttk.Entry(adv_grid, textvariable=self.exclude_modules_var, width=60).grid(
            row=2, column=1, columnspan=2, sticky=tk.EW, pady=5
        )
        self._add_help(adv_grid, "格式: module1,module2", 2, 3)

        ttk.Label(adv_grid, text="额外参数:").grid(
            row=3, column=0, sticky=tk.W, padx=(0, 10), pady=5
        )
        ttk.Entry(adv_grid, textvariable=self.extra_args_var, width=60).grid(
            row=3, column=1, columnspan=2, sticky=tk.EW, pady=5
        )
        self._add_help(adv_grid, "自定义 PyInstaller 命令行参数", 3, 3)

        adv_grid.columnconfigure(1, weight=1)

        # ========== 操作按钮 ==========
        btn_frame = tk.Frame(settings_frame, bg=self.colors["bg"], pady=10)
        btn_frame.pack(fill=tk.X, padx=5)

        self.build_btn = tk.Button(
            btn_frame,
            text="🔨 开始打包",
            font=("Microsoft YaHei UI", 13, "bold"),
            bg="#89b4fa",
            fg="#1e1e2e",
            activebackground="#b4befe",
            activeforeground="#1e1e2e",
            relief=tk.FLAT,
            cursor="hand2",
            padx=30,
            pady=8,
            command=self._start_build,
        )
        self.build_btn.pack(side=tk.LEFT, padx=(0, 10))

        self.stop_btn = tk.Button(
            btn_frame,
            text="⏹ 停止",
            font=("Microsoft YaHei UI", 11),
            bg="#f38ba8",
            fg="#1e1e2e",
            activebackground="#eba0ac",
            activeforeground="#1e1e2e",
            relief=tk.FLAT,
            cursor="hand2",
            padx=20,
            pady=8,
            command=self._stop_build,
            state=tk.DISABLED,
        )
        self.stop_btn.pack(side=tk.LEFT, padx=(0, 10))

        ttk.Button(btn_frame, text="保存配置", command=self._save_config).pack(
            side=tk.LEFT, padx=(0, 10)
        )
        ttk.Button(btn_frame, text="清空输出", command=self._clear_output).pack(
            side=tk.LEFT, padx=(0, 10)
        )
        ttk.Button(btn_frame, text="打开输出目录", command=self._open_output_dir).pack(
            side=tk.LEFT
        )

        # 进度条
        self.progress = ttk.Progressbar(
            settings_frame, mode="indeterminate", style="TProgressbar"
        )
        self.progress.pack(fill=tk.X, padx=5, pady=(10, 0))

        # 主内容加入 PanedWindow
        main_paned.add(canvas, stretch="first")

        # ========== 输出日志区域 ==========
        output_frame = ttk.Labelframe(self.root, text="📋 输出日志", padding=8)
        output_frame.pack(fill=tk.BOTH, expand=True, padx=15, pady=(0, 10))

        self.output_text = scrolledtext.ScrolledText(
            output_frame,
            wrap=tk.WORD,
            font=("Cascadia Code", 9),
            bg=self.colors["output_bg"],
            fg=self.colors["output_fg"],
            insertbackground=self.colors["fg"],
            relief=tk.FLAT,
            borderwidth=0,
            padx=8,
            pady=8,
            state=tk.DISABLED,
        )
        self.output_text.pack(fill=tk.BOTH, expand=True)

        # 配置文本标签样式
        self.output_text.tag_configure("info", foreground=self.colors["output_fg"])
        self.output_text.tag_configure("error", foreground=self.colors["error_fg"])
        self.output_text.tag_configure("warn", foreground=self.colors["warn_fg"])
        self.output_text.tag_configure("success", foreground=self.colors["success"])
        self.output_text.tag_configure("command", foreground=self.colors["accent"])
        self.output_text.tag_configure("timestamp", foreground="#6c7086")

        # 状态栏
        status_frame = tk.Frame(self.root, bg="#181825", height=28)
        status_frame.pack(fill=tk.X, side=tk.BOTTOM)

        self.status_label = tk.Label(
            status_frame,
            text="就绪 - 选择 Python 脚本后点击「开始打包」",
            font=("Microsoft YaHei UI", 9),
            bg="#181825",
            fg="#a6adc8",
            anchor=tk.W,
            padx=10,
        )
        self.status_label.pack(fill=tk.X)

        # 绑定窗口关闭事件
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _add_help(self, parent, text, row, col):
        """添加帮助标签"""
        help_lbl = tk.Label(
            parent,
            text=text,
            font=("Microsoft YaHei UI", 8),
            bg=self.colors["bg"],
            fg="#6c7086",
        )
        help_lbl.grid(row=row, column=col, sticky=tk.W, padx=(5, 0), pady=5)

    def _center_window(self):
        """窗口居中"""
        self.root.update_idletasks()
        w = self.root.winfo_width()
        h = self.root.winfo_height()
        sw = self.root.winfo_screenwidth()
        sh = self.root.winfo_screenheight()
        x = (sw - w) // 2
        y = (sh - h) // 2
        self.root.geometry(f"{w}x{h}+{x}+{y}")

    def _browse_py(self):
        """浏览选择 Python 文件"""
        path = filedialog.askopenfilename(
            title="选择 Python 脚本",
            filetypes=[("Python 文件", "*.py"), ("所有文件", "*.*")],
        )
        if path:
            self.py_file_path.set(path)
            # 自动填充应用名称
            if not self.app_name.get():
                self.app_name.set(Path(path).stem)
            # 自动设置输出目录
            if self.output_dir.get() == str(Path.cwd() / "dist"):
                self.output_dir.set(str(Path(path).parent / "dist"))

    def _browse_output(self):
        """浏览选择输出目录"""
        path = filedialog.askdirectory(title="选择输出目录")
        if path:
            self.output_dir.set(path)

    def _browse_icon(self):
        """浏览选择图标文件"""
        path = filedialog.askopenfilename(
            title="选择图标文件",
            filetypes=[
                ("图标文件", "*.ico"),
                ("PNG 图片", "*.png"),
                ("所有文件", "*.*"),
            ],
        )
        if path:
            self.icon_path.set(path)

    def _build_command(self):
        """构建 PyInstaller 命令"""
        py_file = self.py_file_path.get().strip()
        if not py_file:
            messagebox.showwarning("警告", "请先选择 Python 脚本文件！")
            return None

        if not os.path.exists(py_file):
            messagebox.showerror("错误", f"文件不存在:\n{py_file}")
            return None

        cmd = ["pyinstaller"]

        # 基本参数
        if self.onefile_var.get():
            cmd.append("--onefile")
        else:
            cmd.append("--onedir")

        if self.console_var.get():
            cmd.append("--console")
        else:
            cmd.append("--noconsole")

        if self.clean_var.get():
            cmd.append("--clean")

        if self.noconfirm_var.get():
            cmd.append("--noconfirm")

        if self.uac_var.get():
            cmd.append("--uac-admin")

        # 应用名称
        app_name = self.app_name.get().strip()
        if app_name:
            cmd.extend(["--name", app_name])

        # 输出目录
        output_dir = self.output_dir.get().strip()
        if output_dir:
            cmd.extend(["--distpath", output_dir])

        # 图标
        icon = self.icon_path.get().strip()
        if icon and os.path.exists(icon):
            cmd.extend(["--icon", icon])

        # 附加数据
        add_data = self.add_data_var.get().strip()
        if add_data:
            for item in add_data.split(","):
                item = item.strip()
                if item:
                    cmd.extend(["--add-data", item])

        # 隐藏导入
        hidden = self.hidden_imports_var.get().strip()
        if hidden:
            for item in hidden.split(","):
                item = item.strip()
                if item:
                    cmd.extend(["--hidden-import", item])

        # 排除模块
        excluded = self.exclude_modules_var.get().strip()
        if excluded:
            for item in excluded.split(","):
                item = item.strip()
                if item:
                    cmd.extend(["--exclude-module", item])

        # 额外参数
        extra = self.extra_args_var.get().strip()
        if extra:
            cmd.extend(extra.split())

        # 源文件
        cmd.append(py_file)

        return cmd

    def _log(self, message, tag="info"):
        """输出日志到文本框"""
        self.output_text.configure(state=tk.NORMAL)
        timestamp = datetime.now().strftime("%H:%M:%S")

        if message.startswith("[") and "]" in message[:6]:
            # 已带标签
            self.output_text.insert(tk.END, message + "\n", tag)
        else:
            self.output_text.insert(tk.END, f"[{timestamp}] ", "timestamp")
            self.output_text.insert(tk.END, message + "\n", tag)

        self.output_text.see(tk.END)
        self.output_text.configure(state=tk.DISABLED)
        self.root.update_idletasks()

    def _read_output(self, pipe, tag):
        """读取子进程输出流"""
        try:
            for line in iter(pipe.readline, ""):
                if not self.build_running:
                    break
                line = line.rstrip()
                if line:
                    # 根据内容智能标记
                    if any(kw in line.upper() for kw in ["ERROR", "FAILED", "CRITICAL"]):
                        self._log(line, "error")
                    elif any(kw in line.upper() for kw in ["WARNING", "WARN"]):
                        self._log(line, "warn")
                    elif any(kw in line.upper() for kw in ["SUCCESS", "COMPLETED", "BUILD COMPLETE"]):
                        self._log(line, "success")
                    elif line.startswith(("pyinstaller", "python")):
                        self._log(line, "command")
                    else:
                        self._log(line, tag)
        except Exception:
            pass
        finally:
            pipe.close()

    def _start_build(self):
        """开始打包"""
        cmd = self._build_command()
        if not cmd:
            return

        self._save_config()

        # 更新 UI 状态
        self.build_running = True
        self.build_btn.configure(state=tk.DISABLED, bg="#45475a")
        self.stop_btn.configure(state=tk.NORMAL)
        self.progress.start(10)
        self.status_label.configure(text="正在打包中...")

        # 清空之前的输出（可选）
        self._clear_output()

        # 输出命令
        cmd_str = " ".join(f'"{c}"' if " " in c else c for c in cmd)
        self._log(f">>> {cmd_str}", "command")
        self._log("=" * 70, "info")

        # 在新线程中运行
        def run_build():
            try:
                # 使用 shell=True 以兼容 Windows
                self.process = subprocess.Popen(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    stdin=subprocess.PIPE,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                    creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
                )

                # 并行读取 stdout 和 stderr
                t1 = threading.Thread(
                    target=self._read_output, args=(self.process.stdout, "info"), daemon=True
                )
                t2 = threading.Thread(
                    target=self._read_output, args=(self.process.stderr, "error"), daemon=True
                )
                t1.start()
                t2.start()

                returncode = self.process.wait()
                t1.join(timeout=2)
                t2.join(timeout=2)

                self.process = None

                # 在主线程更新 UI
                self.root.after(0, self._build_finished, returncode)

            except FileNotFoundError:
                self.root.after(
                    0,
                    self._build_error,
                    "未找到 pyinstaller，请先安装:\npip install pyinstaller",
                )
            except Exception as e:
                self.root.after(0, self._build_error, str(e))

        threading.Thread(target=run_build, daemon=True).start()

    def _build_finished(self, returncode):
        """打包完成回调"""
        self.build_running = False
        self.progress.stop()
        self.build_btn.configure(state=tk.NORMAL, bg="#89b4fa")
        self.stop_btn.configure(state=tk.DISABLED)

        self._log("=" * 70, "info")

        if returncode == 0:
            self._log("✅ 打包成功！", "success")
            self.status_label.configure(
                text=f"打包完成 - 输出目录: {self.output_dir.get()}",
            )
            self._log(f"📂 输出目录: {self.output_dir.get()}", "success")

            # 显示生成的 exe 路径
            app_name = self.app_name.get().strip() or Path(self.py_file_path.get()).stem
            exe_path = Path(self.output_dir.get()) / f"{app_name}.exe"
            if exe_path.exists():
                self._log(f"🎯 可执行文件: {exe_path}", "success")
        else:
            self._log(f"❌ 打包失败 (返回码: {returncode})", "error")
            self.status_label.configure(text=f"打包失败 - 返回码: {returncode}")

    def _build_error(self, error_msg):
        """打包错误回调"""
        self.build_running = False
        self.progress.stop()
        self.build_btn.configure(state=tk.NORMAL, bg="#89b4fa")
        self.stop_btn.configure(state=tk.DISABLED)
        self._log(f"❌ 错误: {error_msg}", "error")
        self.status_label.configure(text="打包出错")
        messagebox.showerror("打包错误", error_msg)

    def _stop_build(self):
        """停止打包"""
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
            self._log("⏹ 用户停止了打包进程", "warn")
            self.build_running = False
            self.progress.stop()
            self.build_btn.configure(state=tk.NORMAL, bg="#89b4fa")
            self.stop_btn.configure(state=tk.DISABLED)
            self.status_label.configure(text="打包已停止")

    def _clear_output(self):
        """清空输出"""
        self.output_text.configure(state=tk.NORMAL)
        self.output_text.delete(1.0, tk.END)
        self.output_text.configure(state=tk.DISABLED)

    def _open_output_dir(self):
        """打开输出目录"""
        output_dir = self.output_dir.get().strip()
        if output_dir and os.path.exists(output_dir):
            os.startfile(output_dir)
        elif output_dir:
            messagebox.showinfo("提示", f"输出目录不存在:\n{output_dir}")

    def _save_config(self):
        """保存当前配置"""
        config = {
            "py_file_path": self.py_file_path.get(),
            "output_dir": self.output_dir.get(),
            "icon_path": self.icon_path.get(),
            "app_name": self.app_name.get(),
            "onefile": self.onefile_var.get(),
            "console": self.console_var.get(),
            "clean": self.clean_var.get(),
            "noconfirm": self.noconfirm_var.get(),
            "uac_admin": self.uac_var.get(),
            "add_data": self.add_data_var.get(),
            "hidden_imports": self.hidden_imports_var.get(),
            "exclude_modules": self.exclude_modules_var.get(),
            "extra_args": self.extra_args_var.get(),
        }
        try:
            with open(self.CONFIG_FILE, "w", encoding="utf-8") as f:
                json.dump(config, f, ensure_ascii=False, indent=2)
        except Exception:
            pass  # 静默失败，不影响使用

    def _load_config(self):
        """加载保存的配置"""
        if not self.CONFIG_FILE.exists():
            return
        try:
            with open(self.CONFIG_FILE, "r", encoding="utf-8") as f:
                config = json.load(f)
            self.py_file_path.set(config.get("py_file_path", ""))
            self.output_dir.set(config.get("output_dir", str(Path.cwd() / "dist")))
            self.icon_path.set(config.get("icon_path", ""))
            self.app_name.set(config.get("app_name", ""))
            self.onefile_var.set(config.get("onefile", True))
            self.console_var.set(config.get("console", False))
            self.clean_var.set(config.get("clean", True))
            self.noconfirm_var.set(config.get("noconfirm", True))
            self.uac_var.set(config.get("uac_admin", False))
            self.add_data_var.set(config.get("add_data", ""))
            self.hidden_imports_var.set(config.get("hidden_imports", ""))
            self.exclude_modules_var.set(config.get("exclude_modules", ""))
            self.extra_args_var.set(config.get("extra_args", ""))
        except Exception:
            pass

    def _on_close(self):
        """窗口关闭处理"""
        if self.build_running:
            if messagebox.askyesno("确认", "打包正在进行中，确定要退出吗？"):
                if self.process:
                    self.process.terminate()
                self.root.destroy()
        else:
            self._save_config()
            self.root.destroy()

    def run(self):
        """启动 GUI"""
        self.root.mainloop()


def main():
    """入口函数"""
    app = PyInstallerGUI()
    app.run()


if __name__ == "__main__":
    main()

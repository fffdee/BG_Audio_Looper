#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BanGTsynth 配置工具 (PyQt6 GUI)
功能: 可视化编辑 bg_config.h 配置文件
"""

import sys
import re
from pathlib import Path
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QTabWidget, QGroupBox, QLabel, QSpinBox, QCheckBox, QComboBox,
    QPushButton, QScrollArea, QGridLayout, QMessageBox, QTextEdit,
    QFileDialog
)
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QFont
import json

from bg_config_parser import BGConfigParser


class BGConfigEditor(QMainWindow):
    """BanGTsynth 配置编辑器主窗口"""
    
    def __init__(self, config_file="bg_config.h"):
        super().__init__()
        self.config_file = Path(config_file)
        self.parser = BGConfigParser(str(self.config_file))
        self.widgets = {}  # 存储控件引用
        
        self.init_ui()
        self.load_config()
    
    def init_ui(self):
        """初始化界面"""
        self.setWindowTitle("BanGTsynth 配置工具")
        self.setGeometry(100, 100, 900, 700)
        
        # 主布局
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        layout = QVBoxLayout(main_widget)
        
        # 标题
        title = QLabel("BanGTsynth 框架配置编辑器")
        title_font = QFont()
        title_font.setPointSize(16)
        title_font.setBold(True)
        title.setFont(title_font)
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title)
        
        # 文件路径
        file_label = QLabel(f"配置文件: {self.config_file.absolute()}")
        file_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(file_label)
        
        # 标签页
        self.tabs = QTabWidget()
        layout.addWidget(self.tabs)
        
        # 按钮栏
        button_layout = QHBoxLayout()
        
        self.btn_import = QPushButton("导入配置")
        self.btn_import.clicked.connect(self.import_config)
        button_layout.addWidget(self.btn_import)
        
        self.btn_export = QPushButton("导出配置")
        self.btn_export.clicked.connect(self.export_config)
        button_layout.addWidget(self.btn_export)
        
        button_layout.addStretch()
        
        self.btn_save = QPushButton("保存配置")
        self.btn_save.clicked.connect(self.save_config)
        button_layout.addWidget(self.btn_save)
        
        self.btn_reload = QPushButton("重新加载")
        self.btn_reload.clicked.connect(self.load_config)
        button_layout.addWidget(self.btn_reload)
        
        self.btn_preview = QPushButton("预览更改")
        self.btn_preview.clicked.connect(self.preview_changes)
        button_layout.addWidget(self.btn_preview)
        
        layout.addLayout(button_layout)
        
        # 状态栏
        self.statusBar().showMessage("就绪")
    
    def load_config(self):
        """加载配置文件"""
        try:
            self.parser.parse()
            self.create_config_tabs()
            self.statusBar().showMessage(f"已加载 {len(self.parser.config_data)} 个配置项")
        except Exception as e:
            QMessageBox.critical(self, "错误", f"加载配置文件失败:\n{str(e)}")
    
    def create_config_tabs(self):
        """创建配置标签页"""
        # 清空现有标签页
        self.tabs.clear()
        self.widgets.clear()
        
        # 定义标签页分组
        tab_groups = {
            "功能模块": ["ENABLE_"],
            "音频参数": ["BG_SAMPLE_", "BG_AUDIO_", "BG_ALSA_", "BG_MAX_POLYPHONY", "BG_CHANNELS"],
            "存储配置": ["BG_STORAGE_"],
            "MIDI配置": ["BG_MIDI_"],
            "调试配置": ["BG_DEBUG_", "BG_LOG_"],
            "性能优化": ["BG_ENABLE_FAST", "BG_ENABLE_SIMD", "BG_MAIN_LOOP"],
            "内存配置": ["BG_USE_DYNAMIC", "BG_MEMORY_"],
        }
        
        for tab_name, prefixes in tab_groups.items():
            tab_widget = self.create_tab_for_group(tab_name, prefixes)
            if tab_widget:
                self.tabs.addTab(tab_widget, tab_name)
        
        # 添加"全部配置"标签页
        all_tab = self.create_all_configs_tab()
        self.tabs.addTab(all_tab, "全部配置")
    
    def create_tab_for_group(self, group_name, prefixes):
        """为指定组创建标签页"""
        # 过滤匹配的配置项
        configs = {}
        for key, value in self.parser.config_data.items():
            if any(key.startswith(prefix) for prefix in prefixes):
                configs[key] = value
        
        if not configs:
            return None
        
        # 创建滚动区域
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        
        # 主容器
        container = QWidget()
        layout = QVBoxLayout(container)
        
        # 按章节分组
        sections = {}
        for key in configs.keys():
            meta = self.parser.get_metadata(key)
            section = meta.get('section', '未分类')
            if section not in sections:
                sections[section] = []
            sections[section].append(key)
        
        # 创建配置项
        for section, keys in sections.items():
            if group_name in section or any(key.startswith(p) for key in keys for p in prefixes):
                group_box = QGroupBox(section)
                group_layout = QGridLayout()
                
                row = 0
                for key in sorted(keys):
                    value = configs[key]
                    meta = self.parser.get_metadata(key)
                    comment = meta.get('comment', '')
                    description = meta.get('description', '')
                    
                    # 优先使用完整描述，否则使用简短注释
                    display_text = description if description else comment
                    
                    widget = self.create_config_widget(key, value, display_text)
                    if widget:
                        # 标签
                        label = QLabel(key + ":")
                        label.setToolTip(display_text)
                        group_layout.addWidget(label, row, 0)
                        
                        # 控件
                        group_layout.addWidget(widget, row, 1)
                        
                        # 说明
                        if display_text:
                            help_label = QLabel(display_text)
                            help_label.setStyleSheet("color: gray; font-size: 9pt;")
                            help_label.setWordWrap(True)
                            group_layout.addWidget(help_label, row, 2)
                        
                        row += 1
                
                group_box.setLayout(group_layout)
                layout.addWidget(group_box)
        
        layout.addStretch()
        scroll.setWidget(container)
        return scroll
    
    def create_config_widget(self, key, value, comment):
        """根据配置项类型创建对应的控件"""
        # 布尔型 (ENABLE_XXX 或值为 0/1)
        if key.startswith("ENABLE_") or (isinstance(value, int) and value in [0, 1]):
            checkbox = QCheckBox()
            checkbox.setChecked(bool(value))
            self.widgets[key] = checkbox
            return checkbox
        
        # 采样率选择
        if key == "BG_SAMPLE_RATE":
            combo = QComboBox()
            combo.addItems(["44100", "48000", "96000"])
            combo.setCurrentText(str(value))
            self.widgets[key] = combo
            return combo
        
        # 平台选择
        if key == "BG_TARGET_PLATFORM":
            combo = QComboBox()
            combo.addItems(["BG_PLATFORM_LINUX", "BG_PLATFORM_STM32", "BG_PLATFORM_ESP32"])
            # 根据值设置当前项
            if value == 1:
                combo.setCurrentText("BG_PLATFORM_LINUX")
            elif value == 2:
                combo.setCurrentText("BG_PLATFORM_STM32")
            elif value == 3:
                combo.setCurrentText("BG_PLATFORM_ESP32")
            self.widgets[key] = combo
            return combo
        
        # 整数型
        if isinstance(value, int):
            spinbox = QSpinBox()
            
            # 根据配置项设置范围
            if "POLYPHONY" in key:
                spinbox.setRange(1, 128)
            elif "BUFFER" in key or "DELAY" in key:
                spinbox.setRange(1, 10000)
            elif "SIZE" in key:
                spinbox.setRange(0, 1024*1024*1024)  # 最大1GB
            elif "RATE" in key:
                spinbox.setRange(1000, 192000)
            elif "CHANNELS" in key:
                spinbox.setRange(1, 16)
            elif "LEVEL" in key:
                spinbox.setRange(0, 4)
            else:
                spinbox.setRange(0, 1000000)
            
            spinbox.setValue(value)
            self.widgets[key] = spinbox
            return spinbox
        
        return None
    
    def create_all_configs_tab(self):
        """创建显示所有配置的标签页"""
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        
        text_edit = QTextEdit()
        text_edit.setReadOnly(True)
        text_edit.setFont(QFont("Courier New", 10))
        
        # 生成配置摘要
        sections = self.parser.get_sections()
        content = []
        
        for section, keys in sections.items():
            content.append(f"\n{'='*70}")
            content.append(f"  {section}")
            content.append('='*70 + "\n")
            
            for key in sorted(keys):
                value = self.parser.config_data[key]
                meta = self.parser.get_metadata(key)
                comment = meta.get('comment', '')
                description = meta.get('description', '')
                
                # 优先显示完整描述
                display_text = description if description else comment
                
                # 格式化值
                if isinstance(value, int) and value > 1024:
                    value_str = f"{value} (0x{value:X})"
                else:
                    value_str = str(value)
                
                content.append(f"{key:35} = {value_str:20}")
                if display_text:
                    content.append(f"  // {display_text}")
                content.append("\n")
        
        text_edit.setPlainText("".join(content))
        
        scroll.setWidget(text_edit)
        return scroll
    
    def get_widget_value(self, key, widget):
        """获取控件的当前值"""
        if isinstance(widget, QCheckBox):
            return 1 if widget.isChecked() else 0
        elif isinstance(widget, QSpinBox):
            return widget.value()
        elif isinstance(widget, QComboBox):
            text = widget.currentText()
            # 平台选择
            if text == "BG_PLATFORM_LINUX":
                return 1
            elif text == "BG_PLATFORM_STM32":
                return 2
            elif text == "BG_PLATFORM_ESP32":
                return 3
            # 其他数值
            try:
                return int(text)
            except:
                return text
        return None
    
    def save_config(self):
        """保存配置到文件"""
        try:
            # 读取原文件
            with open(self.config_file, 'r', encoding='utf-8') as f:
                lines = f.readlines()
            
            # 更新配置值
            new_lines = []
            for line in lines:
                updated = False
                
                # 查找 #define 行
                for key, widget in self.widgets.items():
                    if f"#define {key}" in line and not line.strip().startswith('//'):
                        # 获取新值
                        new_value = self.get_widget_value(key, widget)
                        
                        # 替换值
                        match = re.match(r'(#define\s+\w+\s+).*?(//.+)?$', line.strip())
                        if match:
                            prefix = match.group(1)
                            comment = match.group(2) or ""
                            indent = len(line) - len(line.lstrip())
                            new_line = ' ' * indent + f"{prefix}{new_value}  {comment}\n"
                            new_lines.append(new_line)
                            updated = True
                            break
                
                if not updated:
                    new_lines.append(line)
            
            # 写回文件
            with open(self.config_file, 'w', encoding='utf-8') as f:
                f.writelines(new_lines)
            
            QMessageBox.information(self, "成功", "配置已保存!\n请重新编译项目以使更改生效。")
            self.statusBar().showMessage("配置已保存")
            
        except Exception as e:
            QMessageBox.critical(self, "错误", f"保存配置失败:\n{str(e)}")
    
    def preview_changes(self):
        """预览将要保存的更改"""
        changes = []
        
        for key, widget in self.widgets.items():
            old_value = self.parser.config_data.get(key)
            new_value = self.get_widget_value(key, widget)
            
            if old_value != new_value:
                changes.append(f"{key}: {old_value} → {new_value}")
        
        if changes:
            msg = "以下配置将被修改:\n\n" + "\n".join(changes)
            QMessageBox.information(self, "配置更改预览", msg)
        else:
            QMessageBox.information(self, "配置更改预览", "没有任何更改")
    
    def import_config(self):
        """导入配置文件"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "选择配置文件",
            str(Path.home()),
            "配置文件 (*.json *.h);;所有文件 (*.*)"
        )
        
        if not file_path:
            return
        
        try:
            file_path = Path(file_path)
            
            # 如果是 JSON 文件
            if file_path.suffix == '.json':
                with open(file_path, 'r', encoding='utf-8') as f:
                    imported_config = json.load(f)
                
                # 更新控件值
                updated_count = 0
                for key, value in imported_config.items():
                    if key in self.widgets:
                        widget = self.widgets[key]
                        if isinstance(widget, QCheckBox):
                            widget.setChecked(bool(value))
                        elif isinstance(widget, QSpinBox):
                            widget.setValue(int(value))
                        elif isinstance(widget, QComboBox):
                            widget.setCurrentText(str(value))
                        updated_count += 1
                
                QMessageBox.information(
                    self, 
                    "导入成功", 
                    f"已导入 {updated_count} 个配置项\n请点击'保存配置'以应用更改"
                )
                self.statusBar().showMessage(f"已导入 {updated_count} 个配置项")
            
            # 如果是 .h 文件
            elif file_path.suffix == '.h':
                # 创建临时解析器
                temp_parser = BGConfigParser(str(file_path))
                temp_parser.parse()
                
                # 更新控件值
                updated_count = 0
                for key, value in temp_parser.config_data.items():
                    if key in self.widgets:
                        widget = self.widgets[key]
                        if isinstance(widget, QCheckBox):
                            widget.setChecked(bool(value))
                        elif isinstance(widget, QSpinBox):
                            widget.setValue(int(value))
                        elif isinstance(widget, QComboBox):
                            widget.setCurrentText(str(value))
                        updated_count += 1
                
                QMessageBox.information(
                    self,
                    "导入成功",
                    f"已从 {file_path.name} 导入 {updated_count} 个配置项\n请点击'保存配置'以应用更改"
                )
                self.statusBar().showMessage(f"已导入 {updated_count} 个配置项")
            
            else:
                QMessageBox.warning(self, "警告", "不支持的文件格式")
        
        except Exception as e:
            QMessageBox.critical(self, "错误", f"导入配置失败:\n{str(e)}")
    
    def export_config(self):
        """导出配置文件"""
        file_path, selected_filter = QFileDialog.getSaveFileName(
            self,
            "导出配置文件",
            str(Path.home() / "bg_config_export.json"),
            "JSON文件 (*.json);;头文件 (*.h);;所有文件 (*.*)"
        )
        
        if not file_path:
            return
        
        try:
            file_path = Path(file_path)
            
            # 收集当前配置
            current_config = {}
            for key, widget in self.widgets.items():
                current_config[key] = self.get_widget_value(key, widget)
            
            # 导出为 JSON
            if file_path.suffix == '.json' or 'JSON' in selected_filter:
                if file_path.suffix != '.json':
                    file_path = file_path.with_suffix('.json')
                
                with open(file_path, 'w', encoding='utf-8') as f:
                    json.dump(current_config, f, indent=4, ensure_ascii=False)
                
                QMessageBox.information(
                    self,
                    "导出成功",
                    f"配置已导出到:\n{file_path}"
                )
            
            # 导出为 .h 文件 (复制当前 bg_config.h)
            elif file_path.suffix == '.h' or '头文件' in selected_filter:
                if file_path.suffix != '.h':
                    file_path = file_path.with_suffix('.h')
                
                # 读取当前文件并更新值
                with open(self.config_file, 'r', encoding='utf-8') as f:
                    lines = f.readlines()
                
                new_lines = []
                for line in lines:
                    updated = False
                    
                    for key, value in current_config.items():
                        if f"#define {key}" in line and not line.strip().startswith('//'):
                            match = re.match(r'(#define\s+\w+\s+).*?(//.+)?$', line.strip())
                            if match:
                                prefix = match.group(1)
                                comment = match.group(2) or ""
                                indent = len(line) - len(line.lstrip())
                                new_line = ' ' * indent + f"{prefix}{value}  {comment}\n"
                                new_lines.append(new_line)
                                updated = True
                                break
                    
                    if not updated:
                        new_lines.append(line)
                
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.writelines(new_lines)
                
                QMessageBox.information(
                    self,
                    "导出成功",
                    f"配置已导出到:\n{file_path}"
                )
            
            self.statusBar().showMessage(f"配置已导出到 {file_path.name}")
        
        except Exception as e:
            QMessageBox.critical(self, "错误", f"导出配置失败:\n{str(e)}")


def main():
    """主函数"""
    app = QApplication(sys.argv)
    
    # 查找配置文件
    config_file = Path("bg_config.h")
    if not config_file.exists():
        config_file = Path(__file__).parent / "bg_config.h"
    
    if not config_file.exists():
        QMessageBox.critical(None, "错误", f"找不到配置文件: bg_config.h")
        sys.exit(1)
    
    # 创建主窗口
    window = BGConfigEditor(str(config_file))
    window.show()
    
    sys.exit(app.exec())


if __name__ == '__main__':
    main()

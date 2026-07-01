#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BanGTsynth 配置文件解析器
功能: 解析 bg_config.h 中的宏定义
"""

import re
from typing import Dict, Any, List, Tuple
from pathlib import Path


class BGConfigParser:
    """bg_config.h 配置文件解析器"""
    
    def __init__(self, config_file: str = "bg_config.h"):
        self.config_file = Path(config_file)
        self.config_data = {}
        self.config_metadata = {}  # 存储注释等元数据
        
    def parse(self) -> Dict[str, Any]:
        """解析配置文件,返回配置字典"""
        if not self.config_file.exists():
            raise FileNotFoundError(f"配置文件不存在: {self.config_file}")
        
        with open(self.config_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
        
        current_section = "未分类"
        block_comment = []  # 存储多行注释块
        in_block_comment = False
        
        i = 0
        while i < len(lines):
            line = lines[i]
            stripped = line.strip()
            
            # 检测多行注释开始
            if '/*' in stripped:
                in_block_comment = True
                block_comment = []
                # 提取注释内容
                comment_line = stripped[stripped.index('/*')+2:]
                if '*/' in comment_line:
                    # 单行的多行注释
                    comment_line = comment_line[:comment_line.index('*/')]
                    in_block_comment = False
                comment_line = comment_line.strip('* ').strip()
                if comment_line and not comment_line.startswith('='):
                    block_comment.append(comment_line)
                i += 1
                continue
            
            # 多行注释中间部分
            if in_block_comment:
                if '*/' in stripped:
                    in_block_comment = False
                    # 提取最后一行注释
                    comment_line = stripped[:stripped.index('*/')].strip('* ').strip()
                    if comment_line and not comment_line.startswith('='):
                        block_comment.append(comment_line)
                else:
                    # 提取注释内容
                    comment_line = stripped.strip('* ').strip()
                    if comment_line and not comment_line.startswith('='):
                        block_comment.append(comment_line)
                i += 1
                continue
            
            # 检测章节分隔符
            if '============' in stripped:
                i += 1
                continue
            
            # 解析 #define 宏定义
            if stripped.startswith('#define'):
                # 合并多行注释作为描述
                full_comment = ' '.join(block_comment) if block_comment else ""
                
                result = self._parse_define(stripped, full_comment)
                if result:
                    key, value, comment = result
                    self.config_data[key] = value
                    self.config_metadata[key] = {
                        'comment': comment,
                        'description': full_comment,  # 完整的多行注释
                        'section': current_section,
                        'raw_line': stripped
                    }
                
                block_comment = []
            
            # 检测章节标题（从注释块中提取）
            elif block_comment:
                combined = ' '.join(block_comment)
                if '配置' in combined or '模块' in combined:
                    current_section = block_comment[0] if block_comment else "未分类"
            
            i += 1
        
        return self.config_data
    
    def _parse_define(self, line: str, comment: str = "") -> Tuple[str, Any, str]:
        """解析单个 #define 行"""
        # 移除单行注释
        if '//' in line:
            line, inline_comment = line.split('//', 1)
            if not comment:
                comment = inline_comment.strip()
        
        # 匹配 #define NAME VALUE
        match = re.match(r'#define\s+(\w+)\s+(.*)', line.strip())
        if not match:
            # 匹配 #define NAME (无值,视为 1)
            match = re.match(r'#define\s+(\w+)\s*$', line.strip())
            if match:
                return (match.group(1), 1, comment)
            return None
        
        name = match.group(1)
        value_str = match.group(2).strip()
        
        # 跳过内部宏定义
        if name.startswith('_'):
            return None
        
        # 跳过函数式宏
        if '(' in name:
            return None
        
        # 解析值
        value = self._parse_value(value_str)
        
        return (name, value, comment)
    
    def _parse_value(self, value_str: str) -> Any:
        """解析宏定义的值"""
        value_str = value_str.strip()
        
        # 空值
        if not value_str:
            return 1
        
        # 十六进制数
        if value_str.startswith('0x') or value_str.startswith('0X'):
            try:
                return int(value_str, 16)
            except ValueError:
                return value_str
        
        # 十进制整数
        if value_str.isdigit():
            return int(value_str)
        
        # 表达式计算 (简单支持)
        if re.match(r'^[\d\s\+\-\*\/\(\)]+$', value_str):
            try:
                return eval(value_str)
            except:
                return value_str
        
        # 表达式包含宏引用
        if re.match(r'^[\w\s\+\-\*\/\(\)]+$', value_str):
            # 尝试替换已知宏
            try:
                for key, val in self.config_data.items():
                    if key in value_str and isinstance(val, (int, float)):
                        value_str = value_str.replace(key, str(val))
                return eval(value_str)
            except:
                return value_str
        
        # 字符串或其他
        return value_str
    
    def get_config(self, key: str, default: Any = None) -> Any:
        """获取配置项的值"""
        return self.config_data.get(key, default)
    
    def get_metadata(self, key: str) -> Dict[str, str]:
        """获取配置项的元数据(注释等)"""
        return self.config_metadata.get(key, {})
    
    def get_sections(self) -> Dict[str, List[str]]:
        """按章节分组返回配置项"""
        sections = {}
        for key, meta in self.config_metadata.items():
            section = meta.get('section', '未分类')
            if section not in sections:
                sections[section] = []
            sections[section].append(key)
        return sections
    
    def print_config(self, show_description=False):
        """打印所有配置项
        
        Args:
            show_description: 是否显示完整的多行描述
        """
        sections = self.get_sections()
        
        for section, keys in sections.items():
            print(f"\n{'='*60}")
            print(f"  {section}")
            print('='*60)
            
            for key in keys:
                value = self.config_data[key]
                meta = self.config_metadata[key]
                comment = meta.get('comment', '')
                description = meta.get('description', '')
                
                # 格式化值
                if isinstance(value, int) and value > 1024:
                    value_str = f"{value} (0x{value:X})"
                else:
                    value_str = str(value)
                
                print(f"{key:30} = {value_str:20} # {comment}")
                
                # 如果有详细描述且与简短注释不同，则显示
                if show_description and description and description != comment:
                    print(f"{'':30}   说明: {description}")


def main():
    """测试解析器"""
    # 查找配置文件
    config_file = Path("bg_config.h")
    if not config_file.exists():
        config_file = Path(__file__).parent / "bg_config.h"
    
    if not config_file.exists():
        print(f"错误: 找不到配置文件 bg_config.h")
        print(f"当前目录: {Path.cwd()}")
        print(f"脚本目录: {Path(__file__).parent}")
        return
    
    parser = BGConfigParser(str(config_file))
    
    print("正在解析 bg_config.h...")
    config = parser.parse()
    
    print(f"\n解析完成! 共找到 {len(config)} 个配置项\n")
    
    # 打印所有配置
    parser.print_config()
    
    # 示例: 获取特定配置和详细信息
    print("\n" + "="*60)
    print("  示例: 获取特定配置及其详细说明")
    print("="*60)
    
    test_keys = ['BG_SAMPLE_RATE', 'BG_MAX_POLYPHONY', 'ENABLE_MIDI_CONTROLLER']
    for key in test_keys:
        if key in parser.config_data:
            value = parser.get_config(key)
            meta = parser.get_metadata(key)
            print(f"\n配置项: {key}")
            print(f"  值: {value}")
            print(f"  简短注释: {meta.get('comment', '无')}")
            print(f"  详细说明: {meta.get('description', '无')}")


if __name__ == '__main__':
    main()

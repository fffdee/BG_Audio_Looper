#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SF2 文件转 C 语言数组工具
将 .sf2 文件转换为 C 语言 const 数组格式
用于嵌入式系统直接包含音源数据
"""

import os
import sys
from pathlib import Path

def sanitize_name(filename):
    """将文件名转换为合法的 C 变量名"""
    # 移除扩展名
    name = Path(filename).stem
    # 替换非法字符为下划线
    name = ''.join(c if c.isalnum() else '_' for c in name)
    # 确保不以数字开头
    if name[0].isdigit():
        name = '_' + name
    return name.lower()

def format_bytes_per_line(data, offset, bytes_per_line=16):
    """格式化一行的字节数据"""
    line_data = data[offset:offset + bytes_per_line]
    hex_str = ', '.join(f'0x{b:02X}' for b in line_data)
    return hex_str

def sf2_to_c_header(sf2_path, output_dir=None, bytes_per_line=16):
    """
    将 SF2 文件转换为 C 头文件格式
    
    Args:
        sf2_path: SF2 文件路径
        output_dir: 输出目录（默认与源文件同目录）
        bytes_per_line: 每行的字节数（默认16）
    """
    sf2_path = Path(sf2_path)
    
    if not sf2_path.exists():
        print(f"错误: 文件不存在 - {sf2_path}")
        return False
    
    # 读取文件数据
    with open(sf2_path, 'rb') as f:
        data = f.read()
    
    file_size = len(data)
    var_name = sanitize_name(sf2_path.name)
    
    # 确定输出路径
    if output_dir is None:
        output_dir = sf2_path.parent
    else:
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
    
    output_h = output_dir / f"{var_name}_data.h"
    
    print(f"处理: {sf2_path.name}")
    print(f"  大小: {file_size:,} 字节 ({file_size / 1024:.2f} KB)")
    print(f"  输出: {output_h}")
    
    # 生成头文件
    with open(output_h, 'w', encoding='utf-8') as f:
        guard = f"{var_name.upper()}_DATA_H"
        
        f.write(f"/**\n")
        f.write(f" * @file {var_name}_data.h\n")
        f.write(f" * @brief SF2 音源数据 - {sf2_path.name}\n")
        f.write(f" * \n")
        f.write(f" * 文件大小: {file_size:,} 字节 ({file_size / 1024:.2f} KB)\n")
        f.write(f" * 生成工具: sf2_to_c_array.py\n")
        f.write(f" */\n\n")
        
        f.write(f"#ifndef {guard}\n")
        f.write(f"#define {guard}\n\n")
        
        f.write(f"#include <stdint.h>\n\n")
        
        # 数组长度定义
        f.write(f"#define {var_name.upper()}_SIZE  {file_size}U\n\n")
        
        # 数组声明
        f.write(f"/* SF2 音源数据数组 */\n")
        f.write(f"extern const uint8_t {var_name}_data[{file_size}];\n\n")
        
        f.write(f"#endif /* {guard} */\n")
    
    # 生成 .c 文件
    output_c = output_dir / f"{var_name}_data.c"
    
    with open(output_c, 'w', encoding='utf-8') as f:
        f.write(f"/**\n")
        f.write(f" * @file {var_name}_data.c\n")
        f.write(f" * @brief SF2 音源数据实现 - {sf2_path.name}\n")
        f.write(f" */\n\n")
        
        f.write(f"#include \"{var_name}_data.h\"\n\n")
        
        f.write(f"/* SF2 数据: {sf2_path.name} ({file_size:,} bytes) */\n")
        f.write(f"const uint8_t {var_name}_data[{file_size}] = {{\n")
        
        # 写入数据
        offset = 0
        while offset < file_size:
            remaining = file_size - offset
            chunk_size = min(bytes_per_line, remaining)
            
            hex_line = format_bytes_per_line(data, offset, chunk_size)
            
            # 添加地址注释（每256字节）
            if offset % 256 == 0:
                f.write(f"    /* 0x{offset:06X} */\n")
            
            f.write(f"    {hex_line}")
            
            # 最后一行不加逗号
            if offset + chunk_size < file_size:
                f.write(",\n")
            else:
                f.write("\n")
            
            offset += chunk_size
        
        f.write(f"}};\n")
    
    print(f"  ✓ 生成成功\n")
    return True

def batch_convert(directory='.', output_dir=None):
    """批量转换目录下所有 SF2 文件"""
    directory = Path(directory)
    sf2_files = list(directory.glob('*.sf2')) + list(directory.glob('*.SF2'))
    
    if not sf2_files:
        print(f"在 {directory} 中未找到 SF2 文件")
        return
    
    print(f"找到 {len(sf2_files)} 个 SF2 文件\n")
    
    success_count = 0
    for sf2_file in sf2_files:
        if sf2_to_c_header(sf2_file, output_dir):
            success_count += 1
    
    print(f"\n完成: {success_count}/{len(sf2_files)} 个文件转换成功")

def print_usage():
    """打印使用说明"""
    print("SF2 to C Array Converter")
    print("=" * 50)
    print("\n用法:")
    print("  python sf2_to_c_array.py <file.sf2>          - 转换单个文件")
    print("  python sf2_to_c_array.py                     - 转换当前目录所有 .sf2")
    print("  python sf2_to_c_array.py <directory>         - 转换指定目录所有 .sf2")
    print("  python sf2_to_c_array.py <file.sf2> <outdir> - 指定输出目录")
    print("\n输出:")
    print("  <name>_data.h  - 头文件（数组声明 + 大小定义）")
    print("  <name>_data.c  - 实现文件（数组数据）")
    print()

if __name__ == '__main__':
    if len(sys.argv) == 1:
        # 无参数 - 转换当前目录
        batch_convert('.')
    elif sys.argv[1] in ['-h', '--help', '/?']:
        print_usage()
    elif len(sys.argv) == 2:
        path = Path(sys.argv[1])
        if path.is_file() and path.suffix.lower() == '.sf2':
            # 单个文件
            sf2_to_c_header(path)
        elif path.is_dir():
            # 目录
            batch_convert(path)
        else:
            print(f"错误: {path} 不是有效的 SF2 文件或目录")
            print_usage()
    elif len(sys.argv) == 3:
        # 文件 + 输出目录
        sf2_to_c_header(sys.argv[1], sys.argv[2])
    else:
        print_usage()

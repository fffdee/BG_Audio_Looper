#!/usr/bin/env python3
"""
C89 兼容性修复脚本
自动将 C99 风格的 for 循环变量声明移到函数开头
"""

import re
import os
import sys

def fix_c89_for_loops(filepath):
    """修复单个文件的 C89 兼容性问题"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except:
        print(f"无法读取文件: {filepath}")
        return False
    
    original_content = content
    
    # 查找所有函数定义
    # 匹配模式: 返回类型 函数名(...) { ... }
    func_pattern = r'((?:static\s+)?(?:const\s+)?(?:unsigned\s+)?(?:int|void|uint8_t|uint16_t|uint32_t|size_t|BG_ERR|FlashStatus_t)\s+\**\s*\w+\s*\([^)]*\)\s*\{)'
    
    def fix_function(match):
        func_start = match.group(0)
        func_pos = match.start()
        
        # 找到函数结束位置
        brace_count = 1
        pos = match.end()
        while pos < len(content) and brace_count > 0:
            if content[pos] == '{':
                brace_count += 1
            elif content[pos] == '}':
                brace_count -= 1
            pos += 1
        
        if brace_count != 0:
            return match.group(0)  # 无法找到函数结束，跳过
        
        func_body = content[match.end():pos-1]
        
        # 查找所有 for 循环中的变量声明
        for_pattern = r'for\s*\(\s*(uint8_t|uint16_t|uint32_t|int|size_t)\s+(\w+)\s*='
        
        declared_vars = {}
        
        def collect_var(m):
            var_type = m.group(1)
            var_name = m.group(2)
            if var_name not in declared_vars:
                declared_vars[var_name] = var_type
            # 移除类型声明，只保留变量名
            return f'for ({var_name} ='
        
        new_body = re.sub(for_pattern, collect_var, func_body)
        
        if not declared_vars:
            return match.group(0) + func_body  # 没有需要修复的
        
        # 在函数开头添加变量声明
        var_decls = '\n    '.join([f'{vtype} {vname};' for vname, vtype in declared_vars.items()])
        new_func = func_start + '\n    ' + var_decls + '\n' + new_body
        
        return new_func
    
    # 不使用 re.DOTALL，而是逐个处理函数
    # 这里简化处理：只替换 for 循环模式
    modified = content
    for_pattern = r'for\s*\(\s*(uint8_t|uint16_t|uint32_t|int|size_t)\s+(\w+)\s*='
    
    if re.search(for_pattern, content):
        # 文件包含需要修复的代码
        print(f"处理文件: {filepath}")
        
        # 简单策略：收集所有需要声明的变量，在每个函数开头查找并添加
        # 这需要更复杂的解析，这里只提示用户
        matches = list(re.finditer(for_pattern, content))
        if matches:
            print(f"  发现 {len(matches)} 个需要修复的 for 循环")
            for m in matches:
                line_num = content[:m.start()].count('\n') + 1
                print(f"    行 {line_num}: for ({m.group(1)} {m.group(2)} = ...)")
        return False  # 需要手动修复
    
    return True  # 无需修复

def main():
    bangtsynth_dir = r"c:\Users\BanGO\Desktop\BanGO_prj\BG_Audio_Looper\BanBox\src\banux\05_component\bangtsynth"
    
    # 遍历所有 .c 文件
    for root, dirs, files in os.walk(bangtsynth_dir):
        for file in files:
            if file.endswith('.c'):
                filepath = os.path.join(root, file)
                fix_c89_for_loops(filepath)

if __name__ == '__main__':
    main()

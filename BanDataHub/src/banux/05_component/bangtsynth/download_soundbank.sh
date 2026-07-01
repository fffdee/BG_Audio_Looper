#!/bin/bash
# 音源下载脚本
# 将SF2/BGS音源文件写入soundbank.bin供主程序使用

# 检查参数
if [ $# -lt 1 ]; then
    echo "用法: $0 <音源文件路径> [偏移地址]"
    echo ""
    echo "示例:"
    echo "  $0 soundbank/piano/piano.sf2           # 下载到偏移0"
    echo "  $0 soundbank/drums/drums.bg 0x100000   # 下载到偏移1MB"
    exit 1
fi

SOURCE_FILE=$1
OFFSET=${2:-0}  # 默认偏移为0

# 检查文件是否存在
if [ ! -f "$SOURCE_FILE" ]; then
    echo "错误: 文件不存在: $SOURCE_FILE"
    exit 1
fi

# 获取文件大小
FILE_SIZE=$(stat -c%s "$SOURCE_FILE" 2>/dev/null || stat -f%z "$SOURCE_FILE" 2>/dev/null)

echo "========================================"
echo "音源下载工具"
echo "========================================"
echo "源文件: $SOURCE_FILE"
echo "文件大小: $FILE_SIZE bytes ($(echo "scale=2; $FILE_SIZE/1024/1024" | bc) MB)"
echo "目标偏移: $OFFSET"
echo "目标文件: soundbank.bin"
echo "========================================"
echo ""

# 方法1: 使用dd命令直接写入 (简单快速)
echo "使用 dd 命令写入..."

# 创建32MB的空文件(如果不存在)
if [ ! -f "soundbank.bin" ]; then
    echo "创建 soundbank.bin (32MB)..."
    dd if=/dev/zero of=soundbank.bin bs=1M count=32 status=progress
fi

# 将音源写入指定偏移
echo "写入音源数据..."
dd if="$SOURCE_FILE" of=soundbank.bin bs=1 seek=$OFFSET conv=notrunc status=progress

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ 下载成功!"
    echo ""
    echo "后续操作:"
    echo "  1. 运行主程序: ./demo"
    echo "  2. 程序会自动从 soundbank.bin 偏移 $OFFSET 处加载音源"
else
    echo ""
    echo "✗ 下载失败!"
    exit 1
fi

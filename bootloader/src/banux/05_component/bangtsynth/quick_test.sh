#!/bin/bash
# BanGTsynth 快速测试脚本
# 功能: 下载音源 → 编译项目 → 运行测试

set -e  # 遇到错误立即退出

echo "========================================"
echo "BanGTsynth 快速测试"
echo "========================================"
echo ""

# 步骤1: 下载音源
echo ">>> 步骤1: 下载音源到 soundbank.bin"
echo ""

# 检查是否有SF2文件
if [ -f "../sf2/Full Grand.sf2" ]; then
    SF2_FILE="../sf2/Full Grand.sf2"
elif [ -f "soundbank/piano/piano.sf2" ]; then
    SF2_FILE="soundbank/piano/piano.sf2"
else
    echo "错误: 找不到SF2文件"
    echo "请将SF2文件放置在以下位置之一:"
    echo "  - ../sf2/Full Grand.sf2"
    echo "  - soundbank/piano/piano.sf2"
    exit 1
fi

echo "使用音源: $SF2_FILE"
echo ""

# 使用dd命令下载
./download_soundbank.sh "$SF2_FILE" 0

echo ""

# 步骤2: 编译项目
echo ">>> 步骤2: 编译项目"
echo ""

cd build
cmake ..
make -j$(nproc)
cd ..

echo ""

# 步骤3: 验证soundbank.bin
echo ">>> 步骤3: 验证 soundbank.bin"
echo ""

if [ -f "soundbank.bin" ]; then
    FILE_SIZE=$(stat -c%s soundbank.bin 2>/dev/null || stat -f%z soundbank.bin 2>/dev/null)
    echo "✓ soundbank.bin 存在"
    echo "  文件大小: $FILE_SIZE bytes ($(echo "scale=2; $FILE_SIZE/1024/1024" | bc) MB)"
    
    # 读取前4字节检查魔数
    MAGIC=$(xxd -l 4 -p soundbank.bin)
    if [ "$MAGIC" = "46464952" ]; then
        echo "  魔数: RIFF (SF2格式) ✓"
    elif [ "$MAGIC" = "50534742" ]; then
        echo "  魔数: BGSP (打包格式) ✓"
    else
        echo "  魔数: $MAGIC (未知格式)"
    fi
else
    echo "✗ soundbank.bin 不存在!"
    exit 1
fi

echo ""

# 步骤4: 运行测试
echo ">>> 步骤4: 准备运行"
echo ""
echo "========================================"
echo "测试准备完成!"
echo "========================================"
echo ""
echo "运行主程序:"
echo "  cd build && ./demo"
echo ""
echo "或者手动测试下载接口:"
echo "  cd example/download_example/build"
echo "  ./download_example ../../soundbank/piano.sf2 0"
echo ""

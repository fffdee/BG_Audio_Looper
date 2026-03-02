#!/bin/bash
# 显示编译信息的详细脚本

echo "========================================="
echo "  检查编译配置"
echo "========================================="
echo ""

cd build 2>/dev/null || {
    echo "build 目录不存在，请先运行 ./rebuild.sh"
    exit 1
}

echo "CMake 配置文件:"
if [ -f "CMakeCache.txt" ]; then
    echo "✓ CMakeCache.txt 存在"
    echo ""
    echo "源文件列表 (ALL_SRC):"
    make VERBOSE=1 2>&1 | grep -E "\.c" | head -20
else
    echo "✗ CMakeCache.txt 不存在，需要运行 cmake .."
fi

echo ""
echo "========================================="
echo "检查关键文件是否存在:"
echo "========================================="

FILES=(
    "../components/hardware_interfance.c"
    "../components/hardware_interfance.h"
    "../src/main.c"
    "../hardware/play/src/play.c"
    "../hardware/bg_read/src/bg_read.c"
)

for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "✓ $file"
    else
        echo "✗ $file (缺失)"
    fi
done

echo ""
echo "========================================="

#!/bin/bash
# 清理并重新编译 BanGTsynth

echo "========================================="
echo "  BanGTsynth 清理重编译脚本"
echo "========================================="
echo ""

# 清理旧的构建
if [ -d "build" ]; then
    echo "正在清理旧的构建目录..."
    rm -rf build
fi

# 创建新的构建目录
echo "创建构建目录..."
mkdir -p build
cd build

# 运行 CMake
echo ""
echo "运行 CMake 配置..."
cmake ..

if [ $? -ne 0 ]; then
    echo ""
    echo "❌ CMake 配置失败！"
    exit 1
fi

# 编译
echo ""
echo "开始编译..."
make

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================="
    echo "  ✅ 编译成功！"
    echo "========================================="
    echo ""
    echo "可执行文件: build/BanGTsynth"
    echo ""
    echo "运行程序: ./BanGTsynth"
else
    echo ""
    echo "========================================="
    echo "  ❌ 编译失败！"
    echo "========================================="
    exit 1
fi

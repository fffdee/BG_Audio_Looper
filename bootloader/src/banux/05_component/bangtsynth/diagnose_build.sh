#!/bin/bash
# BanGTsynth 编译诊断和修复脚本

echo "========================================="
echo "  BanGTsynth 编译诊断"
echo "========================================="
echo ""

# 1. 检查关键源文件
echo "1. 检查关键源文件..."
echo "-------------------"

if [ ! -f "components/hardware_interfance.c" ]; then
    echo "❌ components/hardware_interfance.c 不存在！"
    exit 1
else
    echo "✓ components/hardware_interfance.c"
fi

if [ ! -f "components/hardware_interfance.h" ]; then
    echo "❌ components/hardware_interfance.h 不存在！"
    exit 1
else
    echo "✓ components/hardware_interfance.h"
fi

# 2. 检查符号定义
echo ""
echo "2. 检查 audioPlay 和 BG_reader 定义..."
echo "-------------------"

if grep -q "AudioPlay audioPlay" components/hardware_interfance.c; then
    echo "✓ audioPlay 已定义"
else
    echo "❌ audioPlay 未定义！"
fi

if grep -q "BG_Reader BG_reader" components/hardware_interfance.c; then
    echo "✓ BG_reader 已定义"
else
    echo "❌ BG_reader 未定义！"
fi

# 3. 检查 CMakeLists.txt
echo ""
echo "3. 检查 CMakeLists.txt 配置..."
echo "-------------------"

if grep -q "hardware_interfance.c" CMakeLists.txt; then
    echo "✓ hardware_interfance.c 已加入 CMakeLists.txt"
else
    echo "❌ hardware_interfance.c 未加入 CMakeLists.txt！"
    echo ""
    echo "修复建议: 在 CMakeLists.txt 中添加:"
    echo "  set(COMPONENT_INTERFACE_SRC"
    echo "      \${CMAKE_SOURCE_DIR}/components/hardware_interfance.c"
    echo "  )"
    exit 1
fi

# 4. 清理并重新编译
echo ""
echo "4. 清理旧构建..."
echo "-------------------"
rm -rf build
mkdir -p build

echo ""
echo "5. 运行 CMake 配置..."
echo "-------------------"
cd build
cmake .. 2>&1 | tee cmake_output.txt

if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo ""
    echo "❌ CMake 配置失败！"
    echo "详细信息请查看: build/cmake_output.txt"
    exit 1
fi

# 检查生成的 Makefile
echo ""
echo "6. 检查生成的编译规则..."
echo "-------------------"

if grep -q "hardware_interfance.c" Makefile; then
    echo "✓ Makefile 包含 hardware_interfance.c"
else
    echo "⚠ 警告: Makefile 可能未包含 hardware_interfance.c"
fi

# 7. 编译
echo ""
echo "7. 开始编译..."
echo "-------------------"
make VERBOSE=1 2>&1 | tee make_output.txt

if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo ""
    echo "========================================="
    echo "  ✅ 编译成功！"
    echo "========================================="
    echo ""
    ls -lh BanGTsynth
    echo ""
    echo "运行程序: ./build/BanGTsynth"
else
    echo ""
    echo "========================================="
    echo "  ❌ 编译失败！"
    echo "========================================="
    echo ""
    echo "详细信息请查看:"
    echo "  - build/cmake_output.txt (CMake 输出)"
    echo "  - build/make_output.txt (Make 输出)"
    echo ""
    
    # 检查是否是链接错误
    if grep -q "undefined reference" make_output.txt; then
        echo "检测到未定义引用错误："
        grep "undefined reference" make_output.txt | sort -u
        echo ""
        echo "可能的原因："
        echo "1. hardware_interfance.c 未被编译"
        echo "2. CMake 缓存问题"
        echo "3. 头文件包含路径问题"
        echo ""
        echo "建议检查 make_output.txt 中是否有编译 hardware_interfance.c 的日志"
    fi
    
    exit 1
fi

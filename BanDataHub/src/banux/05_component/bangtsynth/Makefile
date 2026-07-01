# BanGTsynth 快速构建 Makefile

.PHONY: all clean download build run test help

# 默认目标
all: help

# 帮助信息
help:
	@echo "========================================"
	@echo "BanGTsynth 构建工具"
	@echo "========================================"
	@echo ""
	@echo "使用方法:"
	@echo "  make download SF2=<文件路径>  - 下载音源到soundbank.bin"
	@echo "  make build                    - 编译项目"
	@echo "  make run                      - 运行demo"
	@echo "  make test                     - 快速测试(下载+编译)"
	@echo "  make clean                    - 清理构建文件"
	@echo ""
	@echo "示例:"
	@echo "  make download SF2=soundbank/piano/piano.sf2"
	@echo "  make build"
	@echo "  make run"
	@echo ""

# 下载音源
download:
ifndef SF2
	@echo "错误: 请指定SF2文件"
	@echo "用法: make download SF2=<文件路径>"
	@echo "示例: make download SF2=soundbank/piano/piano.sf2"
	@exit 1
endif
	@chmod +x download_soundbank.sh
	@./download_soundbank.sh "$(SF2)" 0

# 编译项目
build:
	@mkdir -p build
	@cd build && cmake .. && make -j$$(nproc)
	@echo ""
	@echo "✓ 编译完成! 可执行文件: build/demo"

# 运行
run:
	@if [ ! -f build/demo ]; then \
		echo "错误: 可执行文件不存在，请先运行 make build"; \
		exit 1; \
	fi
	@if [ ! -f soundbank.bin ]; then \
		echo "警告: soundbank.bin 不存在，请先运行 make download"; \
		echo "示例: make download SF2=soundbank/piano/piano.sf2"; \
		exit 1; \
	fi
	@cd build && ./demo

# 快速测试 (自动下载默认音源并编译)
test:
	@chmod +x quick_test.sh download_soundbank.sh
	@./quick_test.sh

# 清理
clean:
	@rm -rf build
	@echo "✓ 清理完成"

# 深度清理 (包括soundbank.bin)
distclean: clean
	@rm -f soundbank.bin
	@echo "✓ 深度清理完成 (已删除 soundbank.bin)"

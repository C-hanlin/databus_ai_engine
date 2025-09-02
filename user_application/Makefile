#
# DataBus AI Engine - 用户空间客户端构建系统
# =============================================
#
# 🎯 构建目标：高性能AI数据处理客户端应用程序
# 📋 功能：编译、链接、安装DataBus用户空间接口程序
# 🔧 技术栈：C99标准、GCC编译器、标准系统库
# ⚡ 优化：O2级别优化，适合生产环境部署
#
# 🏗️ 构建架构：
# 源码(.c) → 预处理 → 编译(.o) → 链接 → 可执行文件
#     ↓         ↓        ↓       ↓        ↓
#  语法检查   优化处理   目标码   库链接   最终程序
#
# 📦 使用方法：
#   make          # 编译程序
#   make clean    # 清理构建文件
#   make install  # 安装到系统
#   make test     # 运行测试
#

# 🔧 编译器配置 - 现代C开发环境
CC = gcc                    # GNU C编译器（推荐版本 >= 7.0）

# 🚀 编译选项 - 平衡性能、安全性和调试能力
CFLAGS = -Wall              \  # 启用所有常见警告
         -Wextra            \  # 启用额外警告检查
         -O2                \  # 二级优化（生产环境推荐）
         -std=c99           \  # C99标准（现代C特性支持）
         -fPIC              \  # 位置无关代码（共享库兼容）
         -D_GNU_SOURCE         # GNU扩展支持（Linux系统调用）

# 🔗 链接器配置 - 外部库依赖管理
# 注意：io_uring支持是可选的，当前使用标准I/O以确保兼容性
# LDFLAGS = -luring         # liburing异步I/O库（高级功能）
LDFLAGS =                   # 当前：仅使用标准C库和系统调用

# 🎯 构建目标定义
TARGET = databus_client     # 最终可执行文件名

# 📁 源文件管理 - 模块化代码组织
SRCS = databus_client.c     # 主程序源码（包含完整功能实现）

# 🔄 中间文件生成规则
OBJS = $(SRCS:.c=.o)        # 自动生成目标文件列表（.c → .o）

# 📋 头文件依赖 - 接口定义和数据结构
HEADERS = databus_shared.h  # 用户空间-内核空间通信协议定义

# ============================================================================
# 🏗️ 构建规则定义 - 自动化编译流水线
# ============================================================================

# 🎯 默认构建目标 - 一键编译完整程序
all: $(TARGET)
	@echo "🎉 DataBus AI客户端构建完成！"
	@echo "📋 可执行文件：$(TARGET)"
	@echo "🚀 运行方式：sudo ./$(TARGET)"

# 🔗 链接阶段 - 生成最终可执行文件
$(TARGET): $(OBJS)
	@echo "🔗 正在链接 $(TARGET)..."
	@echo "   📦 目标文件：$(OBJS)"
	@echo "   🔧 链接器选项：$(LDFLAGS)"
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "✅ 链接完成：$(TARGET)"
	@echo "📏 文件大小：$$(du -h $(TARGET) | cut -f1)"
	@echo "🔍 依赖库：$$(ldd $(TARGET) 2>/dev/null | wc -l) 个动态库"

# 🔨 编译阶段 - 源码到目标文件转换
%.o: %.c $(HEADERS)
	@echo "🔨 正在编译 $<..."
	@echo "   📋 编译选项：$(CFLAGS)"
	@echo "   📁 头文件依赖：$(HEADERS)"
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "✅ 编译完成：$@"

# 🧹 清理目标 - 删除构建产物
clean:
	@echo "🧹 正在清理构建文件..."
	@echo "   🗑️ 删除目标文件：$(OBJS)"
	@echo "   🗑️ 删除可执行文件：$(TARGET)"
	rm -f $(OBJS) $(TARGET)
	@echo "✅ 清理完成，工作目录已恢复干净状态"

# ============================================================================
# 📦 系统集成目标 - 部署和管理
# ============================================================================

# 📥 系统安装 - 部署到标准系统路径
install: $(TARGET)
	@echo "📥 正在安装 $(TARGET) 到系统..."
	@echo "   📂 目标路径：/usr/local/bin/$(TARGET)"
	@echo "   🔐 权限要求：需要sudo权限"
	@echo "   🎯 安装后可全局访问：$(TARGET)"
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)
	@echo "✅ 安装完成！"
	@echo "🚀 现在可以在任意位置运行：sudo $(TARGET)"
	@echo "📋 查看安装：which $(TARGET)"

# 📤 系统卸载 - 从系统中移除
uninstall:
	@echo "📤 正在卸载 $(TARGET)..."
	@echo "   🗑️ 删除系统文件：/usr/local/bin/$(TARGET)"
	@echo "   🔐 权限要求：需要sudo权限"
	sudo rm -f /usr/local/bin/$(TARGET)
	@echo "✅ 卸载完成，$(TARGET) 已从系统中移除"

# ============================================================================
# 🚀 开发和测试目标 - 便捷的开发工作流
# ============================================================================

# 🏃 快速运行 - 一键构建并执行（需要root权限访问设备）
run: $(TARGET)
	@echo "🏃 正在运行 $(TARGET)..."
	@echo "   🔐 权限：使用sudo获取设备访问权限"
	@echo "   📋 设备文件：/dev/databus_ai"
	@echo "   ⚠️ 确保内核模块已加载：lsmod | grep databus"
	@echo "========================================"
	sudo ./$(TARGET)

# 🐛 调试构建 - 包含调试符号和额外信息
debug: CFLAGS += -DDEBUG -g -O0 -fno-omit-frame-pointer
debug: $(TARGET)
	@echo "🐛 调试版本构建完成！"
	@echo "   🔍 调试符号：已包含 (-g)"
	@echo "   📋 调试宏：DEBUG已定义"
	@echo "   🚫 优化：已禁用 (-O0)"
	@echo "   🔧 调试工具：gdb ./$(TARGET)"

# ✅ 依赖检查 - 验证构建环境
check-deps:
	@echo "✅ 正在检查构建依赖..."
	@echo "   🔧 检查GCC编译器..."
	@which gcc > /dev/null || (echo "❌ 错误：gcc编译器未找到，请安装：sudo apt install gcc" && exit 1)
	@echo "   ✅ GCC编译器：$$(gcc --version | head -1)"
	@echo "   📋 检查内核头文件..."
	@test -d /usr/src/linux-headers-$$(uname -r) || echo "⚠️ 警告：内核头文件可能缺失"
	# @pkg-config --exists liburing || (echo "⚠️ 注意：liburing未安装，异步I/O功能不可用" && exit 1)
	@echo "   ✅ 所有必需依赖已满足"

# ============================================================================
# 📚 帮助和文档目标
# ============================================================================

# 📖 显示帮助信息 - 完整的使用指南
help:
	@echo "🚀 DataBus AI Engine - 用户空间客户端构建系统"
	@echo "=============================================="
	@echo ""
	@echo "📋 可用构建目标："
	@echo "  🎯 all        - 构建应用程序（默认目标）"
	@echo "  🧹 clean      - 清理构建产物"
	@echo "  📥 install    - 安装到系统路径 (/usr/local/bin)"
	@echo "  📤 uninstall  - 从系统中卸载"
	@echo "  🏃 run        - 构建并运行应用程序"
	@echo "  🐛 debug      - 构建调试版本（包含调试符号）"
	@echo "  ✅ check-deps - 检查构建依赖"
	@echo "  📖 help       - 显示此帮助信息"
	@echo ""
	@echo "🔧 使用示例："
	@echo "  make                    # 构建应用程序"
	@echo "  make run                # 构建并运行"
	@echo "  make clean all          # 清理后重新构建"
	@echo "  sudo make install       # 系统级安装"
	@echo "  make debug              # 构建调试版本"
	@echo "  make check-deps         # 检查环境依赖"
	@echo ""
	@echo "⚠️ 重要提示："
	@echo "  • 运行需要root权限：sudo ./$(TARGET)"
	@echo "  • 确保内核模块已加载：sudo insmod ../kernel_module/databus_ai.ko"
	@echo "  • 检查设备文件：ls -l /dev/databus*"
	@echo ""
	@echo "📚 更多信息请参考：../README.md"

# ============================================================================
# 🏷️ 伪目标声明 - 防止文件名冲突
# ============================================================================
.PHONY: all clean install uninstall run debug check-deps help
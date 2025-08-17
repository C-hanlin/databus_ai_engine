# databus_ai - 高性能AI预处理驱动引擎

## 项目概述

**databus_ai** 是一个高性能的Linux内核驱动模块，专为AI应用的实时数据预处理而设计。它提供了基于DMA、io_uring和环形缓冲区的零拷贝数据传输机制，支持30fps的实时图像传输和IMU传感器数据采集。

DataBus AI 引擎是一个复杂的内核级驱动程序，专为AI应用程序提供超低延迟的数据流传输。它利用了Linux内核的高级特性，包括DMA一致性内存分配、io_uring异步I/O和内存映射零拷贝数据传输。

### 核心特性
- 高性能数据流传输: 实现约30fps的实时数据生成
- 零拷贝架构: 内核空间与用户空间之间的直接内存映射
- DMA一致性内存: 为高吞吐量操作提供高效的内存管理
- 环形缓冲区实现: 为生产者-消费者场景提供无锁循环缓冲区
- io_uring集成: 现代异步I/O支持（占位符实现）
- IMU传感器模拟: 生成真实的加速度计和陀螺仪数据
- 内核跟踪支持: 内置跟踪点用于性能分析
- 字符设备接口: 标准的Linux设备文件操作

## 系统要求

### 硬件要求
- x86_64 或 ARM64 架构
- 最少 2GB RAM
- 支持DMA的系统

### 软件要求
- Linux内核 4.19 或更高版本
- 带有内核头文件的GCC编译器
- Make构建系统
- 加载模块需要root权限
- liburing开发库（可选，用于io_uring功能）

### 依赖安装
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
sudo apt install liburing-dev liburing2

# CentOS/RHEL
sudo yum groupinstall "Development Tools"
sudo yum install kernel-devel liburing-devel

# Arch Linux
sudo pacman -S base-devel linux-headers liburing
```

## 项目结构

```
databus_ai_engine/
├── kernel_module/          # 内核模块源代码
│   ├── databus_core.c     # 主模块实现
│   ├── databus.h          # 头文件定义
│   ├── databus_ioctl.c    # IOCTL命令处理器
│   ├── databus_mmap.c     # 内存映射实现
│   ├── databus_io_uring.c # io_uring集成
│   ├── trace.h            # 内核跟踪定义
│   └── Makefile           # 内核模块构建配置
├── user_application/       # 用户空间应用程序
│   ├── databus_client.c   # 客户端应用程序
│   ├── databus_shared.h   # 共享定义
│   └── Makefile           # 用户应用程序构建配置
├── Makefile               # 顶级构建配置
├── README.md              # 本文档
├── requirements.txt       # Python依赖包列表
├── activate_env.sh        # 虚拟环境激活脚本
├── check_dev_env.py       # 开发环境检查脚本
├── setup_dev_env.sh       # 开发环境设置脚本
├── quick_build.sh         # 快速构建脚本
└── quick_test.sh          # 快速测试脚本
```

## 快速开始

### 1. 设置开发环境

```bash
# 运行开发环境设置脚本
./setup_dev_env.sh

# 或者手动激活虚拟环境
source databus_ai_venv/bin/activate
./activate_env.sh
```

### 2. 构建项目

```bash
# 使用快速构建脚本
./quick_build.sh

# 或者手动构建内核模块和用户应用程序
make

# 或者分别构建各个组件
make kernel
make user
```

### 3. 加载内核模块

```bash
# 加载模块（需要root权限）
sudo insmod kernel_module/databus_ai.ko

# 验证模块已加载
lsmod | grep databus_ai

# 检查内核消息
dmesg | tail -20
```

### 4. 运行用户应用程序

```bash
# 运行客户端应用程序
./user_application/databus_client

# 应用程序将会：
# 1. 打开设备文件 (/dev/databus_ai)
# 2. 将DMA缓冲区映射到用户空间
# 3. 启动数据流传输
# 4. 处理并显示传入的数据包
# 5. 显示性能统计信息
```

### 5. 清理

```bash
# 停止应用程序（按Ctrl+C）
# 卸载内核模块
sudo rmmod databus_ai

# 清理构建产物
make clean
```

## 使用指南

### 设备接口

内核模块在 `/dev/databus_ai` 创建一个字符设备，支持以下操作：

- 打开/关闭: 标准的设备访问文件操作
- IOCTL命令:
  - `DATABUS_IOCTL_START_STREAM`: 启动数据生成
  - `DATABUS_IOCTL_STOP_STREAM`: 停止数据生成  
  - `DATABUS_IOCTL_GET_BUFFER_INFO`: 获取缓冲区布局信息
- 内存映射: 将DMA缓冲区映射到用户空间以实现零拷贝访问

### 数据结构

每个数据包包含：

```c
struct ai_data_packet {
    uint64_t timestamp_ns;      // 纳秒级时间戳
    uint64_t sequence_id;       // 数据包序列号
    uint32_t status;            // 状态标志
    
    // IMU传感器数据
    int32_t accel_x, accel_y, accel_z;  // 加速度计 (mm/s²)
    int32_t gyro_x, gyro_y, gyro_z;     // 陀螺仪 (mdps)
    
    // 图像张量元数据（占位符）
    uint32_t tensor_offset;     // 张量数据偏移量
    uint32_t tensor_size;       // 张量数据大小
    uint32_t tensor_height;     // 图像高度
    uint32_t tensor_width;      // 图像宽度
    uint32_t tensor_format;     // 像素格式
};
```

### 环形缓冲区布局

DMA缓冲区的组织结构如下：

```
[环形缓冲区头部][数据包元数据区][实际数据区]
|<-- 16字节 -->|<-- MAX_PACKETS * sizeof(ai_data_packet) -->|<-- 剩余空间 -->|
```

- 环形缓冲区头部: 包含头/尾指针和容量信息
- 数据包元数据区: `ai_data_packet` 结构体数组
- 实际数据区: 为未来的图像/张量数据预留空间

## 性能指标

### 典型性能表现

- 数据生成速率: 约30 FPS（33毫秒间隔）
- 延迟: 从生成到用户空间访问 < 1毫秒
- 内存使用: 4MB DMA缓冲区（可配置）
- CPU开销: 由于零拷贝设计，开销极小
- 数据包丢失: 正常条件下无丢失

### 监控

```bash
# 监控内核消息
sudo dmesg -w | grep databus

# 检查设备统计信息
cat /proc/modules | grep databus_ai

# 监控内存使用情况
cat /proc/meminfo | grep -E "(MemFree|Buffers|Cached)"
```

## 调试和故障排除

### 常见问题

1. 模块加载失败
   ```bash
   # 检查内核版本兼容性
   uname -r
   # 确保已安装内核头文件
   sudo apt-get install linux-headers-$(uname -r)
   ```

2. 设备文件未创建
   ```bash
   # 检查udev规则是否工作
   ls -la /dev/databus_ai
   # 如需要可手动创建设备节点
   sudo mknod /dev/databus_ai c <主设备号> 0
   ```

3. 内存映射失败
   ```bash
   # 检查可用内存
   free -h
   # 在dmesg中验证DMA缓冲区分配
   dmesg | grep "DMA buffer"
   ```

### 调试构建

```bash
# 使用调试符号构建
make DEBUG=1

# 启用内核调试
echo 8 > /proc/sys/kernel/printk

# 使用ftrace进行详细跟踪
echo 1 > /sys/kernel/debug/tracing/events/databus/enable
cat /sys/kernel/debug/tracing/trace
```

## 开发指南

### 添加新功能

1. 扩展数据包结构
   - 在内核和用户头文件中修改 `ai_data_packet`
   - 更新数据包处理逻辑
   - 保持二进制兼容性

2. 添加新的IOCTL命令
   - 在共享头文件中定义命令
   - 在 `databus_ioctl.c` 中实现处理器
   - 添加用户空间包装函数

3. 实现真实硬件集成
   - 替换模拟数据生成
   - 添加硬件特定的初始化
   - 处理硬件中断和DMA

### 代码风格

- 遵循Linux内核编码风格
- 使用适当的错误处理
- 添加全面的注释
- 提交前进行彻底测试

## API参考

### 内核模块API

#### IOCTL命令

- `DATABUS_IOCTL_START_STREAM`: 启动数据生成
- `DATABUS_IOCTL_STOP_STREAM`: 停止数据生成
- `DATABUS_IOCTL_GET_BUFFER_INFO`: 获取缓冲区信息

#### 内存布局

- 环形缓冲区头部位于偏移量0
- 数据包元数据位于偏移量 `sizeof(ring_buffer_header)`
- 数据区位于偏移量 `sizeof(ring_buffer_header) + PACKET_METADATA_SIZE`

### 用户空间API

#### 基本用法

```c
// 打开设备
int fd = open("/dev/databus_ai", O_RDWR);

// 获取缓冲区信息
struct databus_buffer_info info;
ioctl(fd, DATABUS_IOCTL_GET_BUFFER_INFO, &info);

// 映射缓冲区
void *buffer = mmap(NULL, info.dma_buffer_size, PROT_READ, MAP_SHARED, fd, 0);

// 启动流传输
ioctl(fd, DATABUS_IOCTL_START_STREAM, NULL);

// 处理数据...

// 停止流传输
ioctl(fd, DATABUS_IOCTL_STOP_STREAM, NULL);

// 清理资源
munmap(buffer, info.dma_buffer_size);
close(fd);
```

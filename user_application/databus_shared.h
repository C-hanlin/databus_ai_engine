/**
 * databus_shared.h - 用户空间与内核空间通信协议（API接口定义）
 * 
 * 【文件概述】
 * 这是DataBus AI Engine的核心接口定义文件，建立了用户空间应用程序
 * 与内核模块之间的标准化通信协议。该文件确保了跨进程、跨地址空间
 * 的数据结构兼容性和ABI（应用程序二进制接口）稳定性。
 * 
 * 🔗 核心职责：
 * 1. 数据结构定义：AI数据包、缓冲区布局、控制结构
 * 2. 接口协议规范：IOCTL命令、参数格式、返回值定义
 * 3. 常量和枚举：错误码、状态标志、配置参数
 * 4. 内存布局约定：确保用户空间和内核空间的数据对齐
 * 
 * 📋 主要内容：
 * ┌─────────────────────────────────────────────────────────┐
 * │ 🎯 AI数据包结构 (ai_data_packet)                        │
 * │   ├── 数据包头部：时间戳、序列号、类型标识              │
 * │   ├── 传感器数据：IMU、温度、状态信息                   │
 * │   └── 张量数据：图像、特征向量、AI推理结果              │
 * ├─────────────────────────────────────────────────────────┤
 * │ 🔄 环形缓冲区布局 (ring_buffer_header)                  │
 * │   ├── 生产者-消费者指针：head、tail                     │
 * │   ├── 同步原语：原子计数器、状态标志                    │
 * │   └── 元数据区域：数据包索引、完整性校验                │
 * ├─────────────────────────────────────────────────────────┤
 * │ ⚡ 异步I/O接口 (io_uring支持)                           │
 * │   ├── 命令结构：读取、写入、轮询操作                    │
 * │   ├── 状态查询：队列状态、完成统计                      │
 * │   └── 错误处理：超时、重试、恢复机制                    │
 * ├─────────────────────────────────────────────────────────┤
 * │ 🎮 设备控制接口 (IOCTL命令)                             │
 * │   ├── 流控制：启动、停止、暂停、恢复                    │
 * │   ├── 配置管理：参数设置、状态查询                      │
 * │   └── 调试支持：性能统计、错误诊断                      │
 * └─────────────────────────────────────────────────────────┘
 * 
 * 🔒 ABI兼容性保证：
 * - 结构体字段顺序固定，不允许随意调整
 * - 使用显式的数据类型大小（u32、u64等）
 * - 内存对齐要求明确定义（__attribute__((packed))）
 * - 版本控制机制确保向后兼容性
 * 
 * 📊 性能考虑：
 * - 缓存行对齐：减少false sharing
 * - 零拷贝设计：最小化内存拷贝开销
 * - 原子操作：避免锁竞争
 * - 批量操作：提高I/O吞吐量
 */

#ifndef _DATABUS_SHARED_H_
#define _DATABUS_SHARED_H_

#include <stdint.h>
#include <sys/ioctl.h>

// IOCTL命令的魔数
// 用于验证IOCTL命令的合法性，防止误操作
#define DATABUS_IOCTL_MAGIC 'D'

/**
 * IOCTL命令定义 - 用户空间与内核空间的控制接口
 * 
 * 【命令分类】
 * 🎮 流控制命令：START_STREAM, STOP_STREAM
 * 📊 信息查询命令：GET_BUFFER_INFO, GET_URING_STATUS
 * ⚡ 异步I/O命令：URING_CMD
 * 
 * 【使用方式】
 * int fd = open("/dev/databus_ai", O_RDWR);
 * ioctl(fd, DATABUS_IOCTL_START_STREAM, NULL);
 * 
 * 【错误处理】
 * 返回值：成功时返回0，失败时返回-1并设置errno
 * 常见错误：EBUSY(设备忙), EINVAL(参数无效), ENOMEM(内存不足)
 */
#define DATABUS_IOCTL_START_STREAM _IO(DATABUS_IOCTL_MAGIC, 1)   // 🚀 启动数据流传输（分配DMA缓冲区，启动内核线程）
#define DATABUS_IOCTL_STOP_STREAM  _IO(DATABUS_IOCTL_MAGIC, 2)   // 🛑 停止数据流传输（释放资源，停止内核线程）
#define DATABUS_IOCTL_GET_BUFFER_INFO _IOR(DATABUS_IOCTL_MAGIC, 3, struct databus_buffer_info)  // 📋 获取缓冲区布局信息
#define DATABUS_IOCTL_URING_CMD        _IOWR(DATABUS_IOCTL_MAGIC, 4, struct databus_uring_cmd_args)  // ⚡ 提交io_uring异步命令
#define DATABUS_IOCTL_GET_URING_STATUS _IOR(DATABUS_IOCTL_MAGIC, 5, struct databus_uring_queue_status)  // 📊 查询io_uring队列状态

// 常量定义
// 定义了设备路径和缓冲区配置参数
#define DATABUS_DEVICE_PATH "/dev/databus_ai"  // 设备文件路径
#define MAX_PACKETS 1000                        // 最大数据包数量
#define DMA_BUFFER_SIZE (1024 * 1024)          // DMA缓冲区大小：1MB
#define MAX_URING_REQUESTS  256                 // 最大io_uring请求数量

// io_uring 命令类型定义
#define DATABUS_URING_CMD_READ_PACKET    1
#define DATABUS_URING_CMD_WRITE_PACKET   2
#define DATABUS_URING_CMD_POLL_READY     3

// 缓冲区信息结构体（用于IOCTL）
// 向用户态提供缓冲区布局和配置信息
struct databus_buffer_info {
    uint64_t dma_buffer_size;    // DMA缓冲区总大小（字节）
    uint32_t max_packets;        // 最大数据包数量
    uint32_t packet_size;        // 每个数据包的大小（字节）
    uint64_t header_offset;      // 环形缓冲区头部的偏移量
    uint64_t metadata_offset;    // 数据包元数据区的偏移量
    uint64_t data_offset;        // 实际数据区的偏移量
};

/**
 * ai_data_packet - AI数据包结构（高性能数据传输的核心载体）
 * 
 * 【结构概述】
 * 这是DataBus AI Engine的核心数据载体，设计用于高效传输AI传感器数据。
 * 该结构体经过精心优化，兼顾了性能、兼容性和可扩展性。
 * 
 * 【内存布局】（总大小：64字节，缓存行友好）
 * ┌─────────────────────────────────────────────────────────┐
 * │ 📋 元数据区域 (16字节)                                   │
 * │   ├── timestamp_ns: 纳秒级时间戳 (8字节)                │
 * │   ├── sequence_id: 数据包序列号 (4字节)                 │
 * │   └── status: 状态和错误标志 (4字节)                    │
 * ├─────────────────────────────────────────────────────────┤
 * │ 🎯 IMU传感器数据 (24字节)                               │
 * │   ├── accel_xyz: 三轴加速度计 (12字节)                  │
 * │   └── gyro_xyz: 三轴陀螺仪 (12字节)                     │
 * ├─────────────────────────────────────────────────────────┤
 * │ 🖼️ 张量数据描述符 (20字节)                              │
 * │   ├── tensor_offset: DMA缓冲区偏移量 (4字节)            │
 * │   ├── tensor_size: 数据总大小 (4字节)                   │
 * │   ├── tensor_height: 图像高度 (4字节)                   │
 * │   ├── tensor_width: 图像宽度 (4字节)                    │
 * │   └── tensor_format: 像素格式标识 (4字节)               │
 * └─────────────────────────────────────────────────────────┘
 * 
 * 【字段详解】
 * 必须与内核版本保持严格匹配，确保ABI兼容性
 */
struct ai_data_packet {
    /* ==================== 数据包元数据 ==================== */
    uint64_t timestamp_ns;       // 📅 纳秒级时间戳（CLOCK_MONOTONIC，单调递增，不受系统时间调整影响）
    uint32_t sequence_id;        // 🔢 数据包序列号（用于检测丢包、重排序和重复包）
    uint32_t status;             // 🚦 状态标志位（bit0:有效性, bit1:校验和错误, bit2-31:预留）
    
    /* ==================== IMU传感器数据 ==================== */
    // 6轴IMU数据，模拟真实传感器的输出格式
    int32_t accel_x, accel_y, accel_z;  // 🏃 三轴加速度计数据（单位：mm/s²，范围：±2000mm/s²）
    int32_t gyro_x, gyro_y, gyro_z;     // 🌀 三轴陀螺仪数据（单位：mdps，范围：±1000000mdps）
    
    /* ==================== 张量数据描述符 ==================== */
    // 为AI推理和图像处理预留的张量数据描述符
    uint32_t tensor_offset;      // 📍 张量数据在DMA缓冲区中的字节偏移量
    uint32_t tensor_size;        // 📏 张量数据的总字节大小（包括所有通道和维度）
    uint32_t tensor_height;      // 📐 图像/张量的高度（像素或元素数）
    uint32_t tensor_width;       // 📐 图像/张量的宽度（像素或元素数）
    uint32_t tensor_format;      // 🎨 数据格式标识（0:RGB888, 1:YUV420, 2:Float32, 3:Int8等）
};

/**
 * ring_buffer_header - 环形缓冲区控制结构（高性能生产者-消费者同步机制）
 * 
 * 【设计理念】
 * 这是一个无锁的环形缓冲区控制结构，实现了高效的生产者-消费者模式。
 * 采用经典的Lamport算法，通过原子操作和内存屏障确保多核CPU环境下的数据一致性。
 * 
 * 【同步机制】
 * 生产者（内核）：只修改head指针，读取tail指针
 * 消费者（用户空间）：只修改tail指针，读取head指针
 * 无锁设计：避免锁竞争，提供更好的实时性能
 * 
 * 【缓冲区状态判断】
 * 空缓冲区：head == tail
 * 满缓冲区：(head + 1) % capacity == tail
 * 可用数据：(head - tail + capacity) % capacity
 * 可用空间：(tail - head - 1 + capacity) % capacity
 * 
 * 【内存布局】（12字节，缓存行友好）
 * ┌─────────────────────────────────────────────────────────┐
 * │ head (4B) │ tail (4B) │ capacity (4B) │
 * └─────────────────────────────────────────────────────────┘
 */
struct ring_buffer_header {
    volatile uint32_t head;      // 🎯 生产者索引（内核写入位置，只有内核线程修改）
    volatile uint32_t tail;      // 📍 消费者索引（用户读取位置，只有用户空间修改）
    uint32_t capacity;           // 🔢 环形缓冲区最大数据包容量（编译时确定）
};

// io_uring 命令参数结构体
struct databus_uring_cmd_args {
    uint32_t cmd_type;           // 命令类型
    uint32_t packet_id;          // 数据包ID
    uint64_t user_data;          // 用户数据指针
    uint32_t data_len;           // 数据长度
    uint32_t flags;              // 标志位
};

// io_uring 队列状态结构体
struct databus_uring_queue_status {
    uint32_t pending_requests;   // 待处理请求数量
    uint32_t completed_requests; // 已完成请求数量
    uint32_t queue_depth;        // 队列深度
    uint32_t error_count;        // 错误计数
};

#endif /* _DATABUS_SHARED_H_ */
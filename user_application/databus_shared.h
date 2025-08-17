/*
 * DataBus AI - 共享头文件
 * 
 * 本头文件包含内核模块和用户空间应用程序之间的共享定义。
 * 
 * 主要内容：
 * - 数据结构定义（数据包、缓冲区信息等）
 * - IOCTL命令定义和魔数
 * - 常量定义（缓冲区大小、数据包数量等）
 * - 状态码和错误码定义
 * - 用户态和内核态通信协议
 */

#ifndef _DATABUS_SHARED_H_
#define _DATABUS_SHARED_H_

#include <stdint.h>
#include <sys/ioctl.h>

// IOCTL命令的魔数
// 用于验证IOCTL命令的合法性，防止误操作
#define DATABUS_IOCTL_MAGIC 'D'

// IOCTL命令定义
// 定义了用户态与内核态通信的控制命令
#define DATABUS_IOCTL_START_STREAM _IO(DATABUS_IOCTL_MAGIC, 1)   // 启动数据流传输
#define DATABUS_IOCTL_STOP_STREAM  _IO(DATABUS_IOCTL_MAGIC, 2)   // 停止数据流传输
#define DATABUS_IOCTL_GET_BUFFER_INFO _IOR(DATABUS_IOCTL_MAGIC, 3, struct databus_buffer_info)  // 获取缓冲区信息

// 常量定义
// 定义了设备路径和缓冲区配置参数
#define DATABUS_DEVICE_PATH "/dev/databus_ai"  // 设备文件路径
#define MAX_PACKETS 1000                        // 最大数据包数量
#define DMA_BUFFER_SIZE (1024 * 1024)          // DMA缓冲区大小：1MB

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

// AI数据包结构体（必须与内核版本匹配）
// 包含传感器数据和图像张量的完整信息
struct ai_data_packet {
    uint64_t timestamp_ns;       // 纳秒级时间戳（数据采集时间）
    uint32_t sequence_id;        // 数据包序列号（用于检测丢包）
    uint32_t status;             // 数据包状态标志（错误码、质量指标等）
    
    // IMU传感器数据
    int32_t accel_x, accel_y, accel_z;  // 加速度计数据 (mm/s²)
    int32_t gyro_x, gyro_y, gyro_z;     // 陀螺仪数据 (mdps - 毫度每秒)
    
    // 图像张量元数据
    uint32_t tensor_offset;      // 张量数据在缓冲区中的偏移量
    uint32_t tensor_size;        // 张量数据的字节大小
    uint32_t tensor_height;      // 图像高度（像素）
    uint32_t tensor_width;       // 图像宽度（像素）
    uint32_t tensor_format;      // 像素格式（RGB、YUV等）
};

// 环形缓冲区头部结构体（必须与内核版本匹配）
// 用于管理生产者和消费者之间的数据同步
struct ring_buffer_header {
    volatile uint32_t head;      // 生产者索引（写入位置）
    volatile uint32_t tail;      // 消费者索引（读取位置）
    uint32_t capacity;           // 缓冲区容量（最大数据包数量）
};

#endif /* _DATABUS_SHARED_H_ */
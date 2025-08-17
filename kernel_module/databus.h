#ifndef _DATABUS_H_
#define _DATABUS_H_

#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/list.h>

// IOCTL Magic Number
#define DATABUS_IOCTL_MAGIC 'D'

// IOCTL Commands
#define DATABUS_IOCTL_START_STREAM _IO(DATABUS_IOCTL_MAGIC, 1)
#define DATABUS_IOCTL_STOP_STREAM  _IO(DATABUS_IOCTL_MAGIC, 2)
#define DATABUS_IOCTL_GET_BUFFER_INFO _IOR(DATABUS_IOCTL_MAGIC, 3, struct databus_buffer_info)

// Constants
#define DATABUS_DEVICE_NAME "databus_ai"
#define DATABUS_CLASS_NAME "databus_class"
#define MAX_PACKETS 1000
#define DMA_BUFFER_SIZE (1024 * 1024)  // 1MB
#define PACKET_METADATA_SIZE (MAX_PACKETS * sizeof(struct ai_data_packet))
#define DATA_AREA_SIZE (DMA_BUFFER_SIZE - sizeof(struct ring_buffer_header) - PACKET_METADATA_SIZE)

// 用户空间缓冲区信息结构体
// 用于向用户空间程序提供DMA缓冲区的详细信息
struct databus_buffer_info {
    __u64 dma_buffer_size;
    __u32 max_packets;
    __u32 packet_size;
    __u64 header_offset;
    __u64 metadata_offset;
    __u64 data_offset;
};

// AI数据包结构体 - 包含传感器数据和图像张量信息
// 这是系统中传输的核心数据结构，支持多种传感器数据的融合
struct ai_data_packet {
    __u64 timestamp_ns;     // 捕获时间戳（纳秒级，用于数据同步）
    __u32 sequence_id;      // 单调递增的帧ID（用于检测丢包和排序）
    __u32 status;           // 状态标志位
    
    // IMU传感器数据（惯性测量单元）
    __s32 accel_x, accel_y, accel_z;  // 加速度计数据 (m/s²)
    __s32 gyro_x, gyro_y, gyro_z;     // 陀螺仪数据 (rad/s)
    
    // 图像张量信息（相对于DMA缓冲区数据区域起始位置）
    __u32 tensor_offset;    // 图像数据的偏移量
    __u32 tensor_size;      // 图像数据的大小（字节）
    __u32 tensor_height;    // 图像高度（像素）
    __u32 tensor_width;     // 图像宽度（像素）
    __u32 tensor_format;    // V4L2像素格式（例如：V4L2_PIX_FMT_RGB24）
};

// 环形缓冲区头部结构体，位于DMA内存区域的最开始位置
// 使用无锁设计，支持高并发读写操作
struct ring_buffer_header {
    volatile __u32 head; // 写入位置指针（内核写入，用户读取）
    volatile __u32 tail; // 读取位置指针（用户写入，内核读取）
    __u32 capacity;      // 最大数据包数量（MAX_PACKETS）
};

// io_uring请求结构体
// 用于管理异步I/O操作的请求队列
struct databus_uring_req {
    struct list_head list;   // 链表节点，用于请求队列管理
    void __user *user_data;  // 用户数据指针
    size_t len;              // 数据长度
    loff_t offset;           // 数据偏移量
};

// 主设备结构体
// 包含设备的所有状态信息和资源管理
struct databus_dev {
    struct cdev cdev;        // 字符设备结构体
    struct device *device;   // 设备对象指针
    
    // DMA缓冲区管理
    void *dma_buffer;                    // DMA一致性缓冲区虚拟地址
    dma_addr_t dma_handle;               // DMA缓冲区物理地址
    size_t dma_size;                     // DMA缓冲区大小（字节）
    
    // 环形缓冲区指针
    struct ring_buffer_header *ring_header;      // 环形缓冲区头部指针
    struct ai_data_packet *packet_metadata_area; // 数据包元数据区域指针
    void *data_area;                             // 实际数据存储区域指针
    
    // 数据生成内核线程
    struct task_struct *kthread;         // 数据生成线程指针
    atomic_t is_streaming;               // 数据流状态标志（原子操作）
    
    // io_uring异步I/O支持
    struct list_head uring_req_list;     // 待处理的io_uring请求队列
    spinlock_t uring_lock;               // io_uring请求队列的自旋锁
    wait_queue_head_t wait_queue;        // 等待队列头
    
    // 设备管理
    atomic_t ref_count;                  // 引用计数器，管理设备生命周期
    struct mutex dev_mutex;              // 设备互斥锁，保护设备状态
};

// 函数声明
// 内核模块的主要接口函数
long databus_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
int databus_mmap(struct file *filp, struct vm_area_struct *vma);
long databus_io_uring_cmd(struct file *filp, unsigned int cmd, unsigned long arg);

// 全局设备指针
// 用于在模块内部访问设备实例
extern struct databus_dev *g_databus_dev;

#endif /* _DATABUS_H_ */
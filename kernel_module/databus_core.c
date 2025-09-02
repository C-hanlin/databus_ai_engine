#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/time.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/spinlock.h>

#define CREATE_TRACE_POINTS
#include "trace.h"  // 内核跟踪点定义
#include "databus.h"

// 模块信息定义
MODULE_LICENSE("GPL");                                    // GPL许可证
MODULE_AUTHOR("databus_ai Team");                         // 作者信息
MODULE_DESCRIPTION("High-performance AI preprocessing driver engine");  // 模块描述
MODULE_VERSION("1.0");                                   // 版本号

// 全局变量定义
// 用于管理设备实例和系统资源
static int major_number;                               // 主设备号
static struct class *databus_class = NULL;            // 设备类指针
struct databus_dev *g_databus_dev = NULL;             // 全局设备实例指针

// 全局帧序列计数器
// 用于为每个生成的数据包分配唯一的序列号
static u32 frame_sequence = 0;

/**
 * databus_kthread_main - 内核数据生成线程（AI数据处理的核心引擎）
 * @data: 线程参数，指向databus_dev结构体
 * 
 * 【功能概述】
 * 这是整个DataBus AI Engine的心脏，负责高频率的AI数据生成和分发。
 * 该线程模拟真实的AI传感器数据流，为用户空间应用提供连续的数据源。
 * 
 * 【核心职责】
 * 1. 🎯 AI数据生成：模拟IMU传感器（加速度计+陀螺仪）和图像数据
 * 2. 📊 环形缓冲区管理：高效的生产者-消费者模式数据传输
 * 3. ⚡ 异步I/O处理：处理io_uring异步请求队列
 * 4. 🕐 时间同步：维护纳秒级时间戳和单调递增序列号
 * 5. 🔄 内存屏障：确保多核CPU环境下的数据一致性
 * 
 * 【性能特性】
 * - 运行频率：约30Hz（每33ms生成一帧数据）
 * - 零拷贝设计：直接在DMA缓冲区中操作数据
 * - 无锁算法：使用原子操作避免锁竞争
 * - 实时性保证：可被调度器抢占，但保持数据生成的连续性
 * 
 * 【数据流向】
 * 内核线程 → 环形缓冲区 → 用户空间应用
 *     ↓
 * io_uring异步请求处理
 * 
 * 返回值: 线程正常退出时返回0
 */
static int databus_kthread_main(void *data)
{
    struct databus_dev *dev = (struct databus_dev *)data;  // 设备实例指针
    u32 head, tail;                                        // 缓冲区位置指针
    struct ai_data_packet *pkt;                            // 数据包指针
    
    pr_info("databus_ai: 内核线程已启动\n");
    
    // 主循环：持续运行直到线程被停止
    while (!kthread_should_stop()) {
        /* ==================== 线程状态管理 ==================== */
        // 等待流传输启用或线程停止信号（可中断等待，避免无意义的CPU消耗）
        wait_event_interruptible(dev->wait_queue,
            atomic_read(&dev->is_streaming) || kthread_should_stop());
        
        // 检查是否收到线程停止信号（优先级最高的退出条件）
        if (kthread_should_stop())
            break;
            
        // 如果流传输未启用，短暂休眠后继续（避免忙等待）
        if (!atomic_read(&dev->is_streaming)) {
            msleep(10);  // 10ms休眠，降低CPU占用
            continue;
        }
        
        /* ==================== 硬件数据采集模拟 ==================== */
        // --- 真实硬件交互的占位符 ---
        msleep(33); // 模拟约30fps的帧率（1000ms/30 ≈ 33ms）
        
        /* ==================== 环形缓冲区状态检查 ==================== */
        // 获取当前环形缓冲区的读写指针（使用volatile确保每次都从内存读取）
        head = dev->ring_header->head;  // 生产者写入位置（内核控制）
        tail = dev->ring_header->tail;  // 消费者读取位置（用户空间控制）
        
        /* ==================== 缓冲区满检测 ==================== */
        // 检查环形缓冲区是否已满：当head指针的下一个位置等于tail时表示缓冲区满
        // 这是经典的环形缓冲区满判断条件，预留一个空槽位来区分满和空状态
        if (((head + 1) % MAX_PACKETS) == tail) {
            pr_warn("databus_ai: 🔴 环形缓冲区已满 (head=%u, tail=%u)，丢弃当前帧\n", head, tail);
            continue;  // 丢弃当前帧，继续下一次循环
        }
        
        /* ==================== 数据包内存分配 ==================== */
        // 获取当前head位置的数据包指针（直接指向DMA缓冲区中的内存）
        pkt = &dev->packet_metadata_area[head];
        
        /* ==================== 数据包头部填充 ==================== */
        // 填充数据包元数据，这些信息对用户空间应用的数据处理至关重要
        pkt->timestamp_ns = ktime_get_real_ns();  // 📅 实时纳秒时间戳（CLOCK_REALTIME，可用于跨系统同步）
        pkt->sequence_id = frame_sequence++;      // 🔢 帧序列号（单调递增，用于检测丢帧和重排序）
        pkt->status = 0;                          // ✅ 状态标志（0=正常，非0=错误码）
        
        /* ==================== AI传感器数据模拟 ==================== */
        // 模拟6轴IMU传感器数据（3轴加速度计 + 3轴陀螺仪）
        // 使用随机数据模拟真实传感器输出，实际应用中这里会读取硬件寄存器
        get_random_bytes(&pkt->accel_x, sizeof(pkt->accel_x));  // 🏃 X轴加速度（m/s²）
        get_random_bytes(&pkt->accel_y, sizeof(pkt->accel_y));  // 🏃 Y轴加速度（m/s²）
        get_random_bytes(&pkt->accel_z, sizeof(pkt->accel_z));  // 🏃 Z轴加速度（m/s²）
        get_random_bytes(&pkt->gyro_x, sizeof(pkt->gyro_x));    // 🌀 X轴角速度（rad/s）
        get_random_bytes(&pkt->gyro_y, sizeof(pkt->gyro_y));    // 🌀 Y轴角速度（rad/s）
        get_random_bytes(&pkt->gyro_z, sizeof(pkt->gyro_z));    // 🌀 Z轴角速度（rad/s）
        
        /* ==================== 张量数据占位符 ==================== */
        // --- 为未来的图像/张量数据预留字段 ---
        pkt->tensor_offset = 0;   // 📍 张量数据在DMA缓冲区中的偏移量（字节）
        pkt->tensor_size = 0;     // 📏 张量数据总大小（字节）
        pkt->tensor_height = 0;   // 📐 图像高度（像素）
        pkt->tensor_width = 0;    // 📐 图像宽度（像素）
        pkt->tensor_format = 0;   // 🎨 像素格式（RGB、YUV等）
        
        /* ==================== 性能监控和调试 ==================== */
        // 添加跟踪点：记录数据包生成事件，用于性能分析和调试
        // trace_databus_packet_produce(pkt->sequence_id, head, tail);  // 🔍 暂时注释掉
        
        /* ==================== 内存一致性保证 ==================== */
        // 写内存屏障：确保上述所有数据写入在head指针更新之前完成
        // 这对多核CPU环境下的数据一致性至关重要
        smp_wmb();
        
        /* ==================== 环形缓冲区指针更新 ==================== */
        // 原子更新head指针，标志着新数据包可供消费
        // 使用模运算实现环形缓冲区的循环特性
        dev->ring_header->head = (head + 1) % MAX_PACKETS;
        
        /* ==================== 异步I/O处理 ==================== */
        // 处理待处理的io_uring异步请求队列
        // 这允许用户空间应用使用高性能异步I/O接口
        databus_process_uring_reqs(dev);
        
        /* ==================== 异步通知机制 ==================== */
        // 唤醒所有等待数据的用户空间进程（支持多个消费者）
        // 使用可中断等待，避免不必要的系统调用开销
        wake_up_interruptible(&dev->wait_queue);
    }
    
    pr_info("databus_ai: 内核线程已停止\n");
    return 0;
}

/*
 * 文件操作函数集
 * 实现字符设备的基本文件操作接口
 */
static int databus_open(struct inode *inode, struct file *filp)
{
    struct databus_dev *dev;
    
    // 从inode获取设备结构体指针
    dev = container_of(inode->i_cdev, struct databus_dev, cdev);
    filp->private_data = dev;  // 将设备指针保存到文件私有数据中
    
    // 增加设备引用计数
    atomic_inc(&dev->ref_count);
    
    pr_info("databus_ai: 设备已打开 (引用计数=%d)\n", 
            atomic_read(&dev->ref_count));
    
    return 0;
}

static int databus_release(struct inode *inode, struct file *filp)
{
    struct databus_dev *dev = filp->private_data;
    
    // 减少设备引用计数
    atomic_dec(&dev->ref_count);
    
    pr_info("databus_ai: 设备已释放 (引用计数=%d)\n", 
            atomic_read(&dev->ref_count));
    
    return 0;
}

// 文件操作结构体定义
// 定义了字符设备支持的所有文件操作
static const struct file_operations databus_fops = {
    .owner = THIS_MODULE,              // 模块所有者
    .open = databus_open,              // 打开设备文件
    .release = databus_release,        // 关闭设备文件
    .unlocked_ioctl = databus_ioctl,   // 设备控制接口
    .mmap = databus_mmap,              // 内存映射接口
};

/*
 * 模块初始化函数
 * 负责分配设备号、创建设备类、初始化DMA缓冲区等
 */
static int __init databus_module_init(void)
{
    int ret;        // 返回值
    dev_t dev_num;  // 设备号
    
    pr_info("databus_ai: 正在初始化模块\n");
    
    // 分配字符设备号
    ret = alloc_chrdev_region(&dev_num, 0, 1, DATABUS_DEVICE_NAME);
    if (ret < 0) {
        pr_err("databus_ai: 分配设备号失败\n");
        return ret;
    }
    major_number = MAJOR(dev_num);  // 提取主设备号
    
    // 创建设备类
    databus_class = class_create(THIS_MODULE, DATABUS_CLASS_NAME);
    if (IS_ERR(databus_class)) {
        pr_err("databus_ai: 创建设备类失败\n");
        ret = PTR_ERR(databus_class);
        goto cleanup_chrdev;
    }
    
    // 分配设备结构体内存
    g_databus_dev = kzalloc(sizeof(struct databus_dev), GFP_KERNEL);
    if (!g_databus_dev) {
        pr_err("databus_ai: 分配设备结构体失败\n");
        ret = -ENOMEM;
        goto cleanup_class;
    }
    
    // 分配DMA一致性内存缓冲区
    g_databus_dev->dma_size = DMA_BUFFER_SIZE;
    g_databus_dev->dma_buffer = dma_alloc_coherent(NULL, 
        g_databus_dev->dma_size, &g_databus_dev->dma_handle, GFP_KERNEL);
    if (!g_databus_dev->dma_buffer) {
        pr_err("databus_ai: 分配DMA缓冲区失败\n");
        ret = -ENOMEM;
        goto cleanup_dev_struct;
    }
    
    // 初始化环形缓冲区指针
    // DMA缓冲区布局：[环形缓冲区头部][数据包元数据区][实际数据区]
    g_databus_dev->ring_header = (struct ring_buffer_header *)g_databus_dev->dma_buffer;
    g_databus_dev->packet_metadata_area = (struct ai_data_packet *)
        ((char *)g_databus_dev->dma_buffer + sizeof(struct ring_buffer_header));
    g_databus_dev->data_area = (char *)g_databus_dev->dma_buffer + 
        sizeof(struct ring_buffer_header) + PACKET_METADATA_SIZE;
    
    // 初始化环形缓冲区头部
    g_databus_dev->ring_header->head = 0;              // 写入位置（生产者）
    g_databus_dev->ring_header->tail = 0;              // 读取位置（消费者）
    g_databus_dev->ring_header->capacity = MAX_PACKETS; // 缓冲区容量
    
    // 初始化io_uring请求列表和锁
    INIT_LIST_HEAD(&g_databus_dev->uring_req_list);   // 初始化请求链表
    spin_lock_init(&g_databus_dev->uring_lock);        // 初始化自旋锁
    init_waitqueue_head(&g_databus_dev->wait_queue);   // 初始化等待队列
    
    // 初始化字符设备
    cdev_init(&g_databus_dev->cdev, &databus_fops);   // 关联文件操作结构体
    g_databus_dev->cdev.owner = THIS_MODULE;          // 设置模块所有者
    
    // 将字符设备添加到系统
    ret = cdev_add(&g_databus_dev->cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("databus_ai: 添加字符设备失败\n");
        goto cleanup_dma;
    }
    
    // 创建设备节点（/dev/databus_ai）
    g_databus_dev->device = device_create(databus_class, NULL, dev_num, 
                                         NULL, DATABUS_DEVICE_NAME);
    if (IS_ERR(g_databus_dev->device)) {
        pr_err("databus_ai: 创建设备节点失败\n");
        ret = PTR_ERR(g_databus_dev->device);
        goto cleanup_cdev;
    }
    
    // 初始化设备状态
    atomic_set(&g_databus_dev->is_streaming, 0);  // 流传输状态（初始为停止）
    atomic_set(&g_databus_dev->ref_count, 0);     // 引用计数（初始为0）
    mutex_init(&g_databus_dev->dev_mutex);        // 初始化设备互斥锁
    
    // 创建并启动内核线程
    g_databus_dev->kthread = kthread_create(databus_kthread_main, 
                                           g_databus_dev, "databus_kthread");
    if (IS_ERR(g_databus_dev->kthread)) {
        pr_err("databus_ai: 创建内核线程失败\n");
        ret = PTR_ERR(g_databus_dev->kthread);
        goto cleanup_device;
    }
    wake_up_process(g_databus_dev->kthread);  // 唤醒线程开始运行
    
    pr_info("databus_ai: 模块初始化成功 (主设备号=%d)\n", major_number);
    pr_info("databus_ai: DMA缓冲区已分配: %zu 字节，物理地址 0x%llx\n", 
            g_databus_dev->dma_size, (unsigned long long)g_databus_dev->dma_handle);
    
    return 0;
    
// 错误处理：按初始化的逆序清理资源
cleanup_device:
    device_destroy(databus_class, MKDEV(major_number, 0));
cleanup_cdev:
    cdev_del(&g_databus_dev->cdev);
cleanup_dma:
    dma_free_coherent(NULL, g_databus_dev->dma_size, 
                     g_databus_dev->dma_buffer, g_databus_dev->dma_handle);
cleanup_dev_struct:
    kfree(g_databus_dev);
cleanup_class:
    class_destroy(databus_class);
cleanup_chrdev:
    unregister_chrdev_region(MKDEV(major_number, 0), 1);
    return ret;
}

/*
 * 模块清理函数
 * 负责停止内核线程、释放DMA缓冲区、注销设备等
 */
static void __exit databus_module_exit(void)
{
    pr_info("databus_ai: 正在清理模块\n");
    
    if (g_databus_dev) {
        // 停止数据流传输
        atomic_set(&g_databus_dev->is_streaming, 0);
        
        // 停止并清理内核线程
        if (g_databus_dev->kthread) {
            kthread_stop(g_databus_dev->kthread);
        }
        
        // 清理所有待处理的io_uring异步请求
        databus_cleanup_uring_reqs(g_databus_dev);
        
        // 清理设备
        device_destroy(databus_class, MKDEV(major_number, 0));  // 销毁设备节点
        cdev_del(&g_databus_dev->cdev);                         // 删除字符设备
        
        // 释放DMA缓冲区
        if (g_databus_dev->dma_buffer) {
            dma_free_coherent(NULL, g_databus_dev->dma_size, 
                             g_databus_dev->dma_buffer, g_databus_dev->dma_handle);
        }
        
        // 释放设备结构体内存
        kfree(g_databus_dev);
        g_databus_dev = NULL;
    }
    
    // 清理设备类和设备号
    if (databus_class) {
        class_destroy(databus_class);  // 销毁设备类
    }
    unregister_chrdev_region(MKDEV(major_number, 0), 1);  // 注销设备号
    
    pr_info("databus_ai: 模块清理完成\n");
}

// 注册模块初始化和清理函数
module_init(databus_module_init);   // 模块加载时调用
module_exit(databus_module_exit);   // 模块卸载时调用
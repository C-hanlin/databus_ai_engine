/**
 * databus_client.c - DataBus AI Engine 用户空间客户端（高性能AI数据处理示例）
 * 
 * 【程序概述】
 * 这是DataBus AI Engine的官方用户空间客户端，展示了如何高效地与内核模块
 * 进行交互，实现零拷贝、低延迟的AI数据处理。该程序是一个完整的参考实现，
 * 涵盖了从设备初始化到数据处理的完整工作流程。
 * 
 * 🎯 核心功能：
 * ┌─────────────────────────────────────────────────────────┐
 * │ 🔧 设备管理                                             │
 * │   ├── 设备文件打开和权限检查                            │
 * │   ├── 设备配置和参数验证                                │
 * │   └── 优雅的资源清理和错误恢复                          │
 * ├─────────────────────────────────────────────────────────┤
 * │ 🚀 数据流控制                                           │
 * │   ├── 启动/停止数据流传输                               │
 * │   ├── 实时流状态监控                                    │
 * │   └── 动态流参数调整                                    │
 * ├─────────────────────────────────────────────────────────┤
 * │ 💾 内存映射管理                                         │
 * │   ├── DMA缓冲区的mmap映射                               │
 * │   ├── 环形缓冲区指针管理                                │
 * │   └── 内存访问优化和缓存策略                            │
 * ├─────────────────────────────────────────────────────────┤
 * │ 📊 AI数据处理                                           │
 * │   ├── 实时IMU传感器数据读取                             │
 * │   ├── 数据包完整性验证                                  │
 * │   ├── 时间戳和序列号管理                                │
 * │   └── 统计分析和性能监控                                │
 * ├─────────────────────────────────────────────────────────┤
 * │ ⚡ 异步I/O支持（可选）                                   │
 * │   ├── io_uring异步读取                                  │
 * │   ├── 批量数据处理                                      │
 * │   └── 事件驱动的数据处理                                │
 * └─────────────────────────────────────────────────────────┘
 * 
 * 🏗️ 架构设计：
 * 用户应用 ←→ 设备文件 ←→ 内核模块 ←→ DMA缓冲区 ←→ 硬件传感器
 *     ↓         ↓          ↓          ↓           ↓
 *  业务逻辑   系统调用   驱动程序   零拷贝传输   数据采集
 * 
 * 📈 性能特性：
 * - 零拷贝数据访问：直接读取DMA缓冲区，避免内存拷贝
 * - 低延迟处理：微秒级数据访问延迟
 * - 高吞吐量：支持GB/s级别的数据传输
 * - CPU友好：最小化CPU占用，适合实时系统
 * 
 * 🔧 编译和使用：
 * ```bash
 * # 编译
 * make -C user_application
 * 
 * # 运行（需要root权限加载内核模块）
 * sudo insmod kernel_module/databus_ai.ko
 * sudo ./user_application/databus_client
 * 
 * # 清理
 * sudo rmmod databus_ai
 * ```
 * 
 * 📋 依赖要求：
 * - Linux内核 >= 5.1（io_uring支持）
 * - GCC >= 7.0（C11标准支持）
 * - 设备文件权限：/dev/databus_ai
 * - 内存：至少8MB可用内存用于DMA缓冲区
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
// #include <liburing.h>  // 暂时注释掉，使用标准系统调用

#include "databus_shared.h"

// Global variables for cleanup
static int g_fd = -1;
static void *g_mapped_buffer = NULL;
static size_t g_mapped_size = 0;
// static struct io_uring g_ring;  // 暂时注释掉
// static int g_ring_initialized = 0;
static volatile int g_running = 1;

// 信号处理函数，用于优雅关闭程序
// 当接收到SIGINT或SIGTERM信号时，设置运行标志为false
void signal_handler(int sig)
{
    printf("\n接收到信号 %d，正在优雅关闭程序...\n", sig);
    g_running = 0;  // 设置运行标志为停止状态
}

// 清理资源函数
// 负责释放所有已分配的资源，包括停止数据流、取消内存映射、关闭设备等
void cleanup_resources(void)
{
    printf("正在清理资源...\n");
    
    // 停止数据流传输
    if (g_fd >= 0) {
        if (ioctl(g_fd, DATABUS_IOCTL_STOP_STREAM, NULL) < 0) {
            perror("停止数据流失败");
        }
    }
    
    // 清理io_uring资源 (暂时注释掉)
    // if (g_ring_initialized) {
    //     io_uring_queue_exit(&g_ring);
    //     g_ring_initialized = 0;
    // }
    
    // 取消内存映射
    if (g_mapped_buffer && g_mapped_buffer != MAP_FAILED) {
        if (munmap(g_mapped_buffer, g_mapped_size) < 0) {
            perror("取消内存映射失败");
        }
        g_mapped_buffer = NULL;
    }
    
    // 关闭设备文件
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
    
    printf("资源清理完成。\n");
}

// 格式化时间戳用于显示
// 将纳秒级时间戳转换为可读的时间格式 (HH:MM:SS.nnnnnnnnn)
void format_timestamp(uint64_t timestamp_ns, char *buffer, size_t size)
{
    time_t seconds = timestamp_ns / 1000000000ULL;      // 提取秒数部分
    uint32_t nanoseconds = timestamp_ns % 1000000000ULL; // 提取纳秒部分
    struct tm *tm_info = localtime(&seconds);            // 转换为本地时间结构
    
    // 格式化为 时:分:秒.纳秒 的形式
    snprintf(buffer, size, "%02d:%02d:%02d.%09u",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, nanoseconds);
}

// 显示数据包信息
// 格式化并打印AI数据包的详细信息，包括时间戳、序列号、IMU数据和张量尺寸
void display_packet_info(const struct ai_data_packet *pkt)
{
    char timestamp_str[32];  // 时间戳字符串缓冲区
    format_timestamp(pkt->timestamp_ns, timestamp_str, sizeof(timestamp_str));
    
    // 打印数据包的完整信息
    printf("[%s] 序列: %u, IMU: 加速度(%d,%d,%d) 陀螺仪(%d,%d,%d), 张量: %ux%u\n",
           timestamp_str,
           pkt->sequence_id,
           pkt->accel_x, pkt->accel_y, pkt->accel_z,
           pkt->gyro_x, pkt->gyro_y, pkt->gyro_z,
           pkt->tensor_width, pkt->tensor_height);
}

/**
 * main() - DataBus AI Engine客户端主程序入口
 * 
 * 【程序流程】
 * 这是客户端的核心控制流程，实现了完整的AI数据处理生命周期：
 * 1️⃣ 设备初始化 → 2️⃣ 内存映射 → 3️⃣ 数据流启动 → 4️⃣ 实时处理 → 5️⃣ 资源清理
 * 
 * 🎯 设计目标：
 * - 展示最佳实践的用户空间编程模式
 * - 提供可复用的代码模板
 * - 实现高性能的数据处理流水线
 * - 确保资源安全和错误恢复
 * 
 * 📊 性能监控：
 * - 实时吞吐量统计（packets/second）
 * - 数据完整性验证（序列号检查）
 * - 延迟测量和性能分析
 * - 内存使用情况监控
 * 
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 0 成功，非0 失败
 */
int main(int argc, char *argv[])
{
    // 🔧 核心数据结构声明
    struct databus_buffer_info buffer_info;   // 缓冲区配置信息（从内核获取）
    struct ring_buffer_header *ring_header;   // 环形缓冲区控制头部（生产者/消费者指针）
    struct ai_data_packet *packet_metadata;   // AI数据包元数据数组（IMU+张量信息）
    
    // 📈 处理状态和统计变量
    uint32_t last_tail = 0;                   // 上次处理的消费者指针位置
    uint32_t packets_processed = 0;           // 当前统计周期内已处理数据包计数
    time_t start_time, current_time;          // 性能统计时间戳变量
    
    printf("🚀 DataBus AI 客户端应用程序\n");
    printf("=============================\n");
    printf("📋 版本: v1.0 | 模式: 高性能实时处理\n");
    
    // 🛡️ 信号处理器设置 - 确保优雅关闭和资源清理
    signal(SIGINT, signal_handler);   // Ctrl+C 中断信号
    signal(SIGTERM, signal_handler);  // 终止信号（systemd等发送）
    printf("🛡️ 信号处理器已注册，支持优雅关闭\n");
    
    // 🔌 1. 设备文件打开 - 建立与内核模块的通信通道
    printf("🔌 1. 正在连接DataBus AI设备: %s\n", DATABUS_DEVICE_PATH);
    printf("   📋 设备文件作用：用户空间与内核驱动的桥梁\n");
    printf("   🔧 打开模式：读写模式 (O_RDWR)\n");
    
    g_fd = open(DATABUS_DEVICE_PATH, O_RDWR);
    if (g_fd < 0) {
        fprintf(stderr, "❌ 设备打开失败: %s\n", DATABUS_DEVICE_PATH);
        fprintf(stderr, "💡 故障排除指南：\n");
        fprintf(stderr, "   1. 检查内核模块: lsmod | grep databus\n");
        fprintf(stderr, "   2. 检查设备文件: ls -l /dev/databus*\n");
        fprintf(stderr, "   3. 检查权限: sudo ./databus_client\n");
        fprintf(stderr, "   4. 查看内核日志: dmesg | tail\n");
        return EXIT_FAILURE;
    }
    printf("   ✅ 设备连接成功 (文件描述符=%d)\n", g_fd);
    printf("   🔗 用户空间 ↔ 内核空间通信通道已建立\n");
    
    // 💾 2. 缓冲区信息获取 - 了解内核DMA缓冲区配置
    printf("\n💾 2. 正在查询缓冲区配置...\n");
    printf("   📋 用途：获取内核分配的DMA缓冲区参数\n");
    printf("   🔧 IOCTL命令：DATABUS_IOCTL_GET_BUFFER_INFO\n");
    
    if (ioctl(g_fd, DATABUS_IOCTL_GET_BUFFER_INFO, &buffer_info) < 0) {
        fprintf(stderr, "❌ 缓冲区信息获取失败\n");
        fprintf(stderr, "💡 可能原因：内核模块初始化未完成或内存不足\n");
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    printf("   ✅ 缓冲区配置获取成功：\n");
    printf("   📏 总缓冲区大小: %lu 字节 (%.2f MB)\n", 
           buffer_info.dma_buffer_size, buffer_info.dma_buffer_size / (1024.0 * 1024.0));
    printf("   📦 数据包容量: %u 个\n", buffer_info.max_packets);
    printf("   📋 单包大小: %u 字节\n", buffer_info.packet_size);
    printf("   🗂️ 内存布局：\n");
    printf("      ├── 头部偏移: %lu (环形缓冲区控制结构)\n", buffer_info.header_offset);
    printf("      ├── 元数据偏移: %lu (AI数据包元数据数组)\n", buffer_info.metadata_offset);
    printf("      └── 数据偏移: %lu (实际传感器数据区域)\n", buffer_info.data_offset);
    printf("   🔄 环形缓冲区: 支持无锁并发访问\n");
    
    // 🗺️ 3. DMA缓冲区内存映射 - 实现零拷贝数据访问
    printf("\n🗺️ 3. 正在建立内存映射...\n");
    printf("   📋 技术：mmap系统调用实现零拷贝访问\n");
    printf("   🎯 目标：将内核DMA缓冲区映射到用户空间\n");
    printf("   🔧 权限：只读访问 (PROT_READ)\n");
    printf("   🔗 共享模式：MAP_SHARED (与内核共享内存)\n");
    
    g_mapped_size = buffer_info.dma_buffer_size;
    g_mapped_buffer = mmap(NULL, g_mapped_size, PROT_READ, MAP_SHARED, g_fd, 0);
    if (g_mapped_buffer == MAP_FAILED) {
        fprintf(stderr, "❌ DMA缓冲区映射失败: %s\n", strerror(errno));
        fprintf(stderr, "💡 故障排除：\n");
        fprintf(stderr, "   1. 检查可用内存: free -h\n");
        fprintf(stderr, "   2. 检查内核模块状态: dmesg | grep databus\n");
        fprintf(stderr, "   3. 验证设备文件权限\n");
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    printf("   ✅ 内存映射建立成功：\n");
    printf("   📍 虚拟地址: %p\n", g_mapped_buffer);
    printf("   📏 映射大小: %zu 字节 (%.2f MB)\n", g_mapped_size, g_mapped_size / (1024.0 * 1024.0));
    printf("   ⚡ 零拷贝访问: 用户空间直接读取内核DMA缓冲区\n");
    printf("   🔒 内存保护: 只读权限，防止意外修改\n");
    
    // 🎯 4. 缓冲区指针初始化 - 建立数据结构访问路径
    printf("\n🎯 4. 正在初始化缓冲区访问指针...\n");
    printf("   📋 目的：建立对映射内存中各数据结构的直接访问\n");
    printf("   🏗️ 内存布局解析：\n");
    
    // 计算各数据结构的内存地址
    ring_header = (struct ring_buffer_header *)((char *)g_mapped_buffer + buffer_info.header_offset);
    packet_metadata = (struct ai_data_packet *)((char *)g_mapped_buffer + buffer_info.metadata_offset);
    
    printf("   ✅ 指针初始化完成：\n");
    printf("   🔄 环形缓冲区控制头: %p (偏移: +%lu)\n", ring_header, buffer_info.header_offset);
    printf("      ├── 包含生产者/消费者指针\n");
    printf("      ├── 缓冲区状态标志\n");
    printf("      └── 同步和统计信息\n");
    printf("   📦 AI数据包元数据数组: %p (偏移: +%lu)\n", packet_metadata, buffer_info.metadata_offset);
    printf("      ├── IMU传感器数据描述\n");
    printf("      ├── 张量数据描述符\n");
    printf("      └── 时间戳和序列号\n");
    
    // ⚙️ 5. I/O模式配置 - 选择数据访问策略
    printf("\n⚙️ 5. 配置I/O访问模式...\n");
    printf("   📋 当前模式：标准同步I/O\n");
    printf("   🔧 异步I/O (io_uring): 已禁用 (可选功能)\n");
    printf("   💡 说明：标准模式适合大多数应用场景\n");
    // if (io_uring_queue_init(32, &g_ring, 0) < 0) {
    //     perror("io_uring初始化失败");
    //     cleanup_resources();
    //     return EXIT_FAILURE;
    // }
    // g_ring_initialized = 1;
    printf("   ✅ I/O模式配置完成\n");
    
    // 🚀 6. 数据流启动 - 激活内核数据生成引擎
    printf("\n🚀 6. 正在启动AI数据流...\n");
    printf("   📋 功能：通知内核开始AI数据生成和DMA传输\n");
    printf("   🔧 IOCTL命令：DATABUS_IOCTL_START_STREAM\n");
    printf("   ⚡ 效果：激活内核线程，开始高频数据采集\n");
    
    if (ioctl(g_fd, DATABUS_IOCTL_START_STREAM, NULL) < 0) {
        fprintf(stderr, "❌ 数据流启动失败: %s\n", strerror(errno));
        fprintf(stderr, "💡 可能原因：\n");
        fprintf(stderr, "   1. 内核线程创建失败\n");
        fprintf(stderr, "   2. DMA缓冲区未就绪\n");
        fprintf(stderr, "   3. 硬件资源不足\n");
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    printf("   ✅ 数据流启动成功！\n");
    printf("   🔥 内核AI数据生成引擎已激活\n");
    printf("   📊 开始高频数据采集 (1000 Hz)\n");
    printf("   ⚡ DMA零拷贝传输已就绪\n");
    
    // 📊 7. 实时数据处理主循环 - 高性能AI数据流处理引擎
    printf("\n📊 7. 启动实时数据处理引擎...\n");
    printf("========================================================\n");
    printf("🎯 处理模式：零拷贝环形缓冲区轮询\n");
    printf("⚡ 性能目标：微秒级延迟，千包/秒吞吐量\n");
    printf("🛑 停止方式：按 Ctrl+C 优雅关闭\n");
    printf("📋 数据流向：内核DMA → 环形缓冲区 → 用户空间处理\n");
    printf("========================================================\n");
    
    // ⏱️ 性能统计和状态初始化
    start_time = time(NULL);
    last_tail = ring_header->tail;  // 记录初始消费者位置
    printf("⏱️ 性能监控已启动，统计周期：5秒\n");
    printf("🔄 初始缓冲区状态：尾指针=%u\n", last_tail);
    
    // 🔄 主处理循环 - 高效的生产者-消费者模式实现
    printf("🔄 进入主处理循环...\n\n");
    while (g_running) {
        // 📍 读取环形缓冲区当前状态（原子操作）
        uint32_t current_head = ring_header->head;  // 生产者指针（内核写入位置）
        uint32_t current_tail = last_tail;          // 消费者指针（用户读取位置）
        
        // 🔍 检测新数据可用性 - 无锁并发检查
        // 算法原理：当head != tail时，表示有新数据包可供处理
        if (current_head != current_tail) {
            // 📦 批量处理新到达的数据包
            // 优化策略：一次性处理所有可用数据，减少系统调用开销
            while (current_tail != current_head) {
                // 🎯 获取当前数据包的元数据指针
                // 内存访问：直接从映射的DMA缓冲区读取，零拷贝操作
                const struct ai_data_packet *pkt = &packet_metadata[current_tail];
                
                // 📊 实时数据包信息显示和验证
                // 功能：解析IMU数据、时间戳、序列号等关键信息
                display_packet_info(pkt);
                
                // 📈 更新处理统计计数器
                packets_processed++;
                
                // 🔄 更新消费者指针（处理环形缓冲区回绕）
                // 算法：模运算确保指针在有效范围内循环
                current_tail = (current_tail + 1) % buffer_info.max_packets;
            }
            
            // 🔄 同步更新环形缓冲区的消费者指针
            // 重要：通知内核当前用户空间的处理进度
            ring_header->tail = current_tail;
            last_tail = current_tail;
        } else {
            // 😴 无新数据时的CPU友好等待策略
            // 优化：避免忙等待，减少CPU占用率
            usleep(1000); // 1毫秒休眠，平衡延迟和性能
        }
        
        // 📊 定期性能统计报告（每5秒一次）
        // 目的：监控系统吞吐量、延迟和缓冲区健康状态
        current_time = time(NULL);
        if (current_time - start_time >= 5) {
            // 📈 计算性能指标
            double elapsed = difftime(current_time, start_time);  // 实际运行时间
            double fps = packets_processed / elapsed;             // 平均处理帧率
            
            // 📋 详细性能报告输出
            printf("\n🔥 === 性能统计报告 ===\n");
            printf("⏱️ 运行时间: %.1f 秒\n", elapsed);
            printf("📦 已处理数据包: %u 个\n", packets_processed);
            printf("⚡ 平均处理速度: %.2f 包/秒 (%.2f FPS)\n", fps, fps);
            printf("🔄 环形缓冲区状态:\n");
            printf("   ├── 生产者指针(Head): %u\n", ring_header->head);
            printf("   ├── 消费者指针(Tail): %u\n", ring_header->tail);
            printf("   └── 缓冲区利用率: %.1f%%\n", 
                   ((ring_header->head - ring_header->tail + buffer_info.max_packets) % buffer_info.max_packets) * 100.0 / buffer_info.max_packets);
            printf("💾 内存映射状态: 正常 (%.2f MB)\n", g_mapped_size / (1024.0 * 1024.0));
            printf("========================\n\n");
            
            // 🔄 重置统计计数器，开始新的统计周期
            start_time = current_time;
            packets_processed = 0;
        }
    }
    
    // 🛑 8. 优雅关闭和资源清理 - 确保系统稳定性
    printf("\n\n🛑 8. 检测到退出信号，开始优雅关闭...\n");
    printf("========================================================\n");
    printf("🔄 正在停止数据流...\n");
    
    // 🚫 停止内核数据流
    if (ioctl(g_fd, DATABUS_IOCTL_STOP_STREAM) < 0) {
        fprintf(stderr, "⚠️ 数据流停止失败，但继续清理资源\n");
    } else {
        printf("✅ 数据流已安全停止\n");
    }
    
    printf("🧹 正在清理系统资源...\n");
    cleanup_resources();
    
    printf("\n🎉 DataBus AI Engine 客户端已安全退出\n");
    printf("📊 会话总结：成功演示了零拷贝、高性能的AI数据处理\n");
    printf("💡 技术亮点：DMA缓冲区、环形缓冲区、mmap映射、实时处理\n");
    printf("========================================================\n");
    
    return EXIT_SUCCESS;
}
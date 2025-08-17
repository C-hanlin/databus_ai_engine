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

// 主函数
// 程序入口点，协调整个应用程序的执行流程
int main(int argc, char *argv[])
{
    struct databus_buffer_info buffer_info;   // 缓冲区信息结构体
    struct ring_buffer_header *ring_header;   // 环形缓冲区头部指针
    struct ai_data_packet *packet_metadata;   // 数据包元数据数组指针
    uint32_t last_tail = 0;                   // 上次处理的尾指针位置
    uint32_t packets_processed = 0;           // 已处理数据包计数
    time_t start_time, current_time;          // 时间变量用于统计
    
    printf("DataBus AI 客户端应用程序\n");
    printf("=============================\n");
    
    // 设置信号处理器，用于优雅关闭
    signal(SIGINT, signal_handler);   // 处理Ctrl+C信号
    signal(SIGTERM, signal_handler);  // 处理终止信号
    
    // 1. 打开设备文件
    printf("1. 正在打开设备 %s...\n", DATABUS_DEVICE_PATH);
    g_fd = open(DATABUS_DEVICE_PATH, O_RDWR);
    if (g_fd < 0) {
        perror("打开设备失败");
        printf("请确保databus_ai内核模块已加载。\n");
        return EXIT_FAILURE;
    }
    printf("   设备打开成功 (文件描述符=%d)\n", g_fd);
    
    // 2. 获取缓冲区信息
    printf("2. 正在获取缓冲区信息...\n");
    if (ioctl(g_fd, DATABUS_IOCTL_GET_BUFFER_INFO, &buffer_info) < 0) {
        perror("获取缓冲区信息失败");
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    printf("   DMA缓冲区大小: %lu 字节\n", buffer_info.dma_buffer_size);
    printf("   最大数据包数: %u\n", buffer_info.max_packets);
    printf("   数据包大小: %u 字节\n", buffer_info.packet_size);
    printf("   头部偏移: %lu\n", buffer_info.header_offset);
    printf("   元数据偏移: %lu\n", buffer_info.metadata_offset);
    printf("   数据偏移: %lu\n", buffer_info.data_offset);
    
    // 3. 映射DMA缓冲区到用户空间
    printf("3. 正在映射DMA缓冲区...\n");
    g_mapped_size = buffer_info.dma_buffer_size;
    g_mapped_buffer = mmap(NULL, g_mapped_size, PROT_READ, MAP_SHARED, g_fd, 0);
    if (g_mapped_buffer == MAP_FAILED) {
        perror("DMA缓冲区映射失败");
        cleanup_resources();
        return EXIT_FAILURE;
    }
    printf("   缓冲区映射成功，地址: %p (大小=%zu)\n", g_mapped_buffer, g_mapped_size);
    
    // 4. 设置缓冲区指针
    ring_header = (struct ring_buffer_header *)((char *)g_mapped_buffer + buffer_info.header_offset);
    packet_metadata = (struct ai_data_packet *)((char *)g_mapped_buffer + buffer_info.metadata_offset);
    
    printf("   环形缓冲区头部地址: %p\n", ring_header);
    printf("   数据包元数据地址: %p\n", packet_metadata);
    
    // 5. 初始化io_uring (暂时跳过)
    printf("4. 跳过 io_uring 初始化 (使用标准I/O)...\n");
    // if (io_uring_queue_init(32, &g_ring, 0) < 0) {
    //     perror("io_uring初始化失败");
    //     cleanup_resources();
    //     return EXIT_FAILURE;
    // }
    // g_ring_initialized = 1;
    printf("   使用标准I/O模式\n");
    
    // 6. 启动数据流
    printf("5. 正在启动数据流...\n");
    if (ioctl(g_fd, DATABUS_IOCTL_START_STREAM, NULL) < 0) {
        perror("启动数据流失败");
        cleanup_resources();
        return EXIT_FAILURE;
    }
    printf("   数据流启动成功\n");
    
    printf("\n6. 处理数据流 (按Ctrl+C停止)...\n");
    printf("========================================================\n");
    
    start_time = time(NULL);
    last_tail = ring_header->tail;
    
    // 7. 主处理循环
    while (g_running) {
        uint32_t current_head = ring_header->head;  // 获取当前头指针（生产者位置）
        uint32_t current_tail = last_tail;          // 获取当前尾指针（消费者位置）
        
        // 检查是否有新数据可用
        if (current_head != current_tail) {
            // 处理所有可用的数据包
            while (current_tail != current_head) {
                const struct ai_data_packet *pkt = &packet_metadata[current_tail];
                
                // 显示数据包信息
                display_packet_info(pkt);
                
                packets_processed++;
                current_tail = (current_tail + 1) % buffer_info.max_packets;
            }
            
            // 更新尾指针
            ring_header->tail = current_tail;
            last_tail = current_tail;
        } else {
            // 没有新数据，短暂休眠
            usleep(1000); // 1毫秒
        }
        
        // 每5秒打印一次统计信息
        current_time = time(NULL);
        if (current_time - start_time >= 5) {
            double elapsed = difftime(current_time, start_time);
            double fps = packets_processed / elapsed;
            
            printf("\n--- 统计信息 ---\n");
            printf("运行时间: %.1f 秒\n", elapsed);
            printf("已处理数据包: %u\n", packets_processed);
            printf("平均帧率: %.2f FPS\n", fps);
            printf("环形缓冲区: 头指针=%u, 尾指针=%u\n", 
                   ring_header->head, ring_header->tail);
            printf("------------------\n\n");
            
            start_time = current_time;
            packets_processed = 0;
        }
    }
    
    printf("\n正在关闭程序...\n");
    cleanup_resources();
    
    printf("应用程序成功终止。\n");
    return EXIT_SUCCESS;
}
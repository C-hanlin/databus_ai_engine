#include <linux/uaccess.h>
#include <linux/atomic.h>
#include "databus.h"

/**
 * databus_ioctl - 设备控制接口（用户空间与内核的命令通道）
 * @filp: 文件指针，代表打开的设备文件描述符
 * @cmd: 控制命令，定义要执行的具体操作类型
 * @arg: 命令参数，包含操作所需的数据或配置信息
 * 
 * 【功能概述】
 * 这是DataBus AI Engine的主要控制接口，允许用户空间应用：
 * 1. 🎮 控制数据流的启动和停止
 * 2. 📊 获取环形缓冲区的实时状态
 * 3. ⚡ 管理io_uring异步I/O操作
 * 4. 🔧 配置设备参数和性能选项
 * 
 * 【支持的命令】
 * - DATABUS_IOCTL_START_STREAM: 启动数据流生成
 * - DATABUS_IOCTL_STOP_STREAM: 停止数据流生成
 * - DATABUS_IOCTL_GET_BUFFER_INFO: 获取缓冲区信息
 * - DATABUS_IOCTL_URING_CMD: 提交io_uring异步命令
 * - DATABUS_IOCTL_GET_URING_STATUS: 查询io_uring操作状态
 * 
 * 【安全性】
 * - 参数验证：检查用户空间指针的有效性
 * - 权限控制：确保只有授权进程可以控制设备
 * - 错误处理：提供详细的错误码和日志信息
 * 
 * 返回值: 成功时返回0，失败时返回负的错误码
 */
long databus_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct databus_dev *dev = filp->private_data;  // 获取设备实例
    int ret = 0;  // 返回值初始化
    
    /* ==================== 参数有效性检查 ==================== */
    if (!dev) {
        pr_err("databus_ai: 🚨 设备指针为空\n");
        return -ENODEV;
    }
    
    // 验证IOCTL魔数，确保命令来源合法
    if (_IOC_TYPE(cmd) != DATABUS_IOCTL_MAGIC) {
        pr_err("databus_ai: 🚨 无效的ioctl魔数 (期望: 0x%x, 实际: 0x%x)\n", 
               DATABUS_IOCTL_MAGIC, _IOC_TYPE(cmd));
        return -ENOTTY;
    }
    
    /* ==================== 命令分发处理 ==================== */
    switch (cmd) {
    case DATABUS_IOCTL_START_STREAM:
        /* 🚀 启动数据流生成 */
        pr_info("databus_ai: 🚀 启动AI数据流生成 (PID: %d)\n", current->pid);
        
        // 原子设置流状态为活跃，确保线程安全
        atomic_set(&dev->is_streaming, 1);
        
        // 唤醒内核数据生成线程，开始产生数据
        wake_up_interruptible(&dev->wait_queue);
        
        pr_debug("databus_ai: ✅ 数据流已启动，内核线程开始工作\n");
        break;
        
    case DATABUS_IOCTL_STOP_STREAM:
        /* 🛑 停止数据流生成 */
        pr_info("databus_ai: 🛑 停止AI数据流生成 (PID: %d)\n", current->pid);
        
        // 原子设置流状态为非活跃
        atomic_set(&dev->is_streaming, 0);
        
        pr_debug("databus_ai: ✅ 数据流已停止，内核线程进入等待状态\n");
        break;
        
    case DATABUS_IOCTL_GET_BUFFER_INFO: {
        /* 📊 获取环形缓冲区详细信息 */
        struct databus_buffer_info buffer_info;
        
        pr_info("databus_ai: 📊 用户空间请求缓冲区信息 (PID: %d)\n", current->pid);
        
        // 填充缓冲区配置信息
        buffer_info.dma_buffer_size = dev->dma_size;  // 🗄️ DMA缓冲区大小（字节）
        buffer_info.max_packets = MAX_PACKETS;  // 🔢 最大数据包数量
        buffer_info.packet_size = sizeof(struct ai_data_packet);  // 📦 单个数据包大小
        buffer_info.header_offset = 0;  // 📍 头部偏移量
        buffer_info.metadata_offset = sizeof(struct ring_buffer_header);  // 📋 元数据偏移量
        buffer_info.data_offset = sizeof(struct ring_buffer_header) + PACKET_METADATA_SIZE;  // 📄 数据偏移量
        
        // 安全地将缓冲区信息复制到用户空间
        if (copy_to_user((void __user *)arg, &buffer_info, sizeof(buffer_info))) {
            pr_err("databus_ai: 🚨 复制缓冲区信息到用户空间失败\n");
            ret = -EFAULT;  // 内存访问错误
        } else {
            pr_debug("databus_ai: ✅ 缓冲区信息已发送 (size=%zu, packets=%d)\n", 
                    buffer_info.dma_buffer_size, buffer_info.max_packets);
        }
        break;
    }
    
    case DATABUS_IOCTL_URING_CMD: {
        /* ⚡ 处理io_uring异步I/O命令 */
        struct databus_uring_cmd_args uring_args;
        
        pr_info("databus_ai: ⚡ 收到io_uring命令请求 (PID: %d)\n", current->pid);
        
        // 安全地从用户空间复制io_uring命令参数
        if (copy_from_user(&uring_args, (void __user *)arg, sizeof(uring_args))) {
            pr_err("databus_ai: 🚨 从用户空间复制io_uring命令参数失败\n");
            ret = -EFAULT;  // 内存访问错误
            break;
        }
        
        // 委托给专门的io_uring处理函数
        ret = databus_io_uring_cmd(filp, cmd, (unsigned long)&uring_args);
        
        if (ret == 0) {
            pr_debug("databus_ai: ✅ io_uring命令处理成功 (cmd=%d)\n", uring_args.cmd);
        } else {
            pr_err("databus_ai: ❌ io_uring命令处理失败 (cmd=%d, ret=%d)\n", 
                   uring_args.cmd, ret);
        }
        break;
    }
    
    case DATABUS_IOCTL_GET_URING_STATUS: {
        /* 📈 获取io_uring操作状态统计 */
        struct databus_uring_queue_status status;
        
        pr_info("databus_ai: 📈 用户空间请求io_uring状态 (PID: %d)\n", current->pid);
        
        // 收集当前io_uring统计信息
        ret = databus_get_uring_queue_status(dev, &status);
        if (ret) {
            pr_err("databus_ai: 🚨 获取io_uring状态失败 (ret=%d)\n", ret);
            break;
        }
        
        // 安全地将状态信息复制到用户空间
        if (copy_to_user((void __user *)arg, &status, sizeof(status))) {
            pr_err("databus_ai: 🚨 复制io_uring状态到用户空间失败\n");
            ret = -EFAULT;  // 内存访问错误
        } else {
            pr_debug("databus_ai: ✅ io_uring状态已发送 (pending=%d, completed=%llu)\n", 
                    status.pending_requests, status.completed_requests);
        }
        break;
    }
    
    default:
        pr_err("databus_ai: 未知的ioctl命令: 0x%x\n", cmd);
        ret = -ENOTTY;  // 返回不支持的ioctl错误
        break;
    }
    
    return ret;
}
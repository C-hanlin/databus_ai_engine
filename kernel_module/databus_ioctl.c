#include <linux/uaccess.h>
#include <linux/atomic.h>
#include "databus.h"

// IOCTL handler function
long databus_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct databus_dev *dev = filp->private_data;
    int ret = 0;
    
    // 验证设备指针的有效性
    if (!dev) {
        pr_err("databus_ai: IOCTL中设备指针无效\n");
        return -ENODEV;
    }
    
    // 验证IOCTL魔数
    if (_IOC_TYPE(cmd) != DATABUS_IOCTL_MAGIC) {
        pr_err("databus_ai: 无效的ioctl魔数\n");
        return -ENOTTY;
    }
    
    // 处理不同的IOCTL命令
    switch (cmd) {
    case DATABUS_IOCTL_START_STREAM:
        pr_info("databus_ai: 启动流传输\n");
        atomic_set(&dev->is_streaming, 1);  // 设置流传输状态为活跃
        wake_up_interruptible(&dev->wait_queue);  // 唤醒等待的内核线程
        break;
        
    case DATABUS_IOCTL_STOP_STREAM:
        pr_info("databus_ai: 停止流传输\n");
        atomic_set(&dev->is_streaming, 0);  // 设置流传输状态为非活跃
        break;
        
    case DATABUS_IOCTL_GET_BUFFER_INFO: {
        struct databus_buffer_info buffer_info;
        
        pr_info("databus_ai: 获取缓冲区信息\n");
        
        // 填充缓冲区信息结构体
        buffer_info.dma_buffer_size = dev->dma_size;  // DMA缓冲区大小
        buffer_info.max_packets = MAX_PACKETS;  // 最大数据包数量
        buffer_info.packet_size = sizeof(struct ai_data_packet);  // 单个数据包大小
        buffer_info.header_offset = 0;  // 头部偏移量
        buffer_info.metadata_offset = sizeof(struct ring_buffer_header);  // 元数据偏移量
        buffer_info.data_offset = sizeof(struct ring_buffer_header) + PACKET_METADATA_SIZE;  // 数据偏移量
        
        // 将缓冲区信息复制到用户空间
        if (copy_to_user((void __user *)arg, &buffer_info, sizeof(buffer_info))) {
            pr_err("databus_ai: 复制缓冲区信息到用户空间失败\n");
            ret = -EFAULT;
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
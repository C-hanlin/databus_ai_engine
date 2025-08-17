#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include "databus.h"

// 内存映射函数 - 将DMA缓冲区映射到用户空间
int databus_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct databus_dev *dev = filp->private_data;  // 从文件私有数据获取设备指针
    unsigned long size;                            // 映射区域大小
    unsigned long pfn;                             // 物理页帧号
    int ret;                                       // 返回值
    
    // 验证设备和DMA缓冲区的有效性
    if (!dev || !dev->dma_buffer) {
        pr_err("databus_ai: mmap中设备或DMA缓冲区无效\n");
        return -ENODEV;
    }
    
    size = vma->vm_end - vma->vm_start;  // 计算映射区域大小
    
    // 检查请求的映射大小是否有效
    if (size > dev->dma_size) {
        pr_err("databus_ai: mmap大小 (%lu) 超过DMA缓冲区大小 (%zu)\n", 
               size, dev->dma_size);
        return -EINVAL;
    }
    
    // 检查偏移量（当前只支持零偏移）
    if (vma->vm_pgoff != 0) {
        pr_err("databus_ai: mmap偏移量必须为0\n");
        return -EINVAL;
    }
    
    // 设置虚拟内存区域标志
    vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;  // 禁止扩展和转储
    vma->vm_flags &= ~VM_MAYWRITE;                 // 为安全起见设置为只读映射
    
    // 获取物理页帧号
    pfn = virt_to_phys(dev->dma_buffer) >> PAGE_SHIFT;
    
    // 将DMA缓冲区映射到用户空间
    ret = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
    if (ret) {
        pr_err("databus_ai: 将DMA缓冲区映射到用户空间失败 (ret=%d)\n", ret);
        return ret;
    }
    
    pr_info("databus_ai: DMA缓冲区已映射到用户空间 (大小=%lu, 页帧号=0x%lx)\n", 
            size, pfn);
    
    return 0;  // 成功返回
}
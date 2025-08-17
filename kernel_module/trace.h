#undef TRACE_SYSTEM
#define TRACE_SYSTEM databus

#if !defined(_DATABUS_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _DATABUS_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(databus_packet_produce,
    TP_PROTO(u32 sequence_id, u32 head, u32 tail),
    TP_ARGS(sequence_id, head, tail),
    TP_STRUCT__entry(
        __field(u32, sequence_id)
        __field(u32, head)
        __field(u32, tail)
    ),
    TP_fast_assign(
        __entry->sequence_id = sequence_id;
        __entry->head = head;
        __entry->tail = tail;
    ),
    TP_printk("sequence_id=%u head=%u tail=%u",
        __entry->sequence_id, __entry->head, __entry->tail)
);

TRACE_EVENT(databus_uring_complete,
    TP_PROTO(u32 sequence_id, size_t len),
    TP_ARGS(sequence_id, len),
    TP_STRUCT__entry(
        __field(u32, sequence_id)
        __field(size_t, len)
    ),
    TP_fast_assign(
        __entry->sequence_id = sequence_id;
        __entry->len = len;
    ),
    TP_printk("sequence_id=%u len=%zu",
        __entry->sequence_id, __entry->len)
);

#endif /* _DATABUS_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH /home/hl/Desktop/linux/databus_ai_engine/kernel_module
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace
#include <trace/define_trace.h>
#undef TRACE_SYSTEM
#define TRACE_SYSTEM databus

#if !defined(_DATABUS_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _DATABUS_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(databus_packet_produce,
    TP_PROTO(u32 sequence_id, u32 head_pos, u32 tail_pos),
    TP_ARGS(sequence_id, head_pos, tail_pos),
    TP_STRUCT__entry(
        __field(u32, sequence_id)
        __field(u32, head_pos)
        __field(u32, tail_pos)
    ),
    TP_fast_assign(
        __entry->sequence_id = sequence_id;
        __entry->head_pos = head_pos;
        __entry->tail_pos = tail_pos;
    ),
    TP_printk("sequence_id=%u head=%u tail=%u",
        __entry->sequence_id, __entry->head_pos, __entry->tail_pos)
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
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace
#include <trace/define_trace.h>
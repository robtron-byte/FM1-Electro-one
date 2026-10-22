/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Single-producer/single-consumer command ring: the control task writes,
 * the audio interrupt reads. Never blocks, never allocates. */
#ifndef EL_QUEUE_H
#define EL_QUEUE_H
#include "el_config.h"

enum { EL_QUEUE_SIZE = 64 }; /* power of two */

typedef struct {
    uint8_t type, a, b, c;
    int16_t value;
} el_cmd_t;

typedef struct {
    el_cmd_t cmd[EL_QUEUE_SIZE];
    volatile uint32_t head, tail; /* head: written by producer, tail: by consumer */
} el_queue_t;

static inline void el_queue_init(el_queue_t *q) { q->head = q->tail = 0; }

/* Control side. Returns 0 when queued, -1 when full. */
static inline int el_queue_push(el_queue_t *q, el_cmd_t c)
{
    uint32_t head = q->head;
    if (head - q->tail >= EL_QUEUE_SIZE) return -1;
    q->cmd[head & (EL_QUEUE_SIZE - 1)] = c;
    EL_FENCE();
    q->head = head + 1;
    return 0;
}

/* Audio side. Returns 1 and fills *c when a command was taken. */
static inline int el_queue_pop(el_queue_t *q, el_cmd_t *c)
{
    uint32_t tail = q->tail;
    if (tail == q->head) return 0;
    EL_FENCE();
    *c = q->cmd[tail & (EL_QUEUE_SIZE - 1)];
    EL_FENCE();
    q->tail = tail + 1;
    return 1;
}
#endif

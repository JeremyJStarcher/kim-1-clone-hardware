#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
#ifndef RING_SIMPLE_H
#define RING_SIMPLE_H

#include <stdint.h>
#include <stddef.h>

/* ---- configuration -------------------------------------------------- */
#ifndef RING_CAPACITY
#define RING_CAPACITY 1024u /* must be a power of two       */
#endif
    _Static_assert((RING_CAPACITY & (RING_CAPACITY - 1u)) == 0,
                   "RING_CAPACITY must be a power of two");
#define RING_MASK (RING_CAPACITY - 1u)

/* ---- architecture-neutral full memory barrier ----------------------- */
#if defined(__clang__) || defined(__GNUC__)
#define RING_BARRIER() __sync_synchronize()
#else
#include <stdatomic.h>
#define RING_BARRIER() atomic_thread_fence(memory_order_seq_cst)
#endif

    /* ---- ring type ------------------------------------------------------ */
    typedef struct
    {
        volatile size_t head; /* written by producer only     */
        uint8_t buf[RING_CAPACITY];
        volatile size_t tail; /* written by consumer only     */
    } ring_t;

    /* ---- API ------------------------------------------------------------ */
    static inline void ring_init(ring_t *r)
    {
        r->head = r->tail = 0;
    }

    /* producer – returns 0 on success, −1 if full */
    static inline int ring_push(ring_t *r, uint8_t byte)
    {
        size_t head = r->head;
        size_t tail = r->tail;

        if (head - tail == RING_CAPACITY) /* full? */
            return -1;

        r->buf[head & RING_MASK] = byte;
        RING_BARRIER(); /* make data visible first */
        r->head = head + 1;
        return 0;
    }

    /* consumer – returns 0 on success, −1 if empty */
    static inline int ring_pop(ring_t *r, uint8_t *out)
    {
        size_t tail = r->tail;
        size_t head = r->head;

        if (tail == head) /* empty? */
            return -1;

        *out = r->buf[tail & RING_MASK];
        RING_BARRIER(); /* commit before freeing slot */
        r->tail = tail + 1;
        return 0;
    }

    /* optional helpers */
    static inline size_t ring_count(const ring_t *r)
    {
        return r->head - r->tail;
    }
    static inline size_t ring_space(const ring_t *r)
    {
        return RING_CAPACITY - ring_count(r);
    }

#endif /* RING_SIMPLE_H */

#ifdef __cplusplus
}
#endif

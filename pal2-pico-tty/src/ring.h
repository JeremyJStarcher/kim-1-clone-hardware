#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

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
    } bt_ring_t;

    /* ---- API ------------------------------------------------------------ */
    static inline void bt_ring_init(volatile bt_ring_t *r)
    {
        r->head = r->tail = 0;
    }

    static inline bool bt_ring_is_empty(volatile bt_ring_t *r)
    {
        return r->head == r->tail;
    }

    /* producer – returns 0 on success, −1 if full */
    static inline int bt_ring_push(volatile bt_ring_t *r, uint8_t byte)
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
    static inline int bt_ring_pop(volatile bt_ring_t *r, uint8_t *out)
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
    static inline size_t bt_ring_count(const volatile bt_ring_t *r)
    {
        return r->head - r->tail;
    }
    static inline size_t ring_space(const volatile bt_ring_t *r)
    {
        return RING_CAPACITY - bt_ring_count(r);
    }


#ifdef __cplusplus
}
#endif

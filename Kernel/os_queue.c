#include "os_queue.h"

/*
 * Classic bounded producer/consumer ring: two counting semaphores track free
 * and filled slots, a mutex protects head/tail.
 */
void os_queue_init(os_message_queue_t *q, uint32_t *buffer_array, uint32_t capacity)
{
    q->buffer   = buffer_array;
    q->capacity = capacity;
    q->head     = 0;
    q->tail     = 0;

    os_mutex_init(&q->lock);
    os_semaphore_init(&q->empty_slots, capacity, capacity);
    os_semaphore_init(&q->filled_slots, 0, capacity);
}

/*
 * Waits up to `timeout` ticks for a free slot. If none frees up, the oldest
 * sample is overwritten rather than dropping the new one: this queue carries
 * sensor readings, where the freshest value is the one that matters.
 */
uint8_t os_queue_send(os_message_queue_t *q, uint32_t message, uint32_t timeout)
{
    bool had_slot = (os_semaphore_acquire(&q->empty_slots, timeout) == OS_SUCCESS);

    if (os_mutex_acquire(&q->lock, WAIT_FOREVER) != OS_SUCCESS)
    {
        if (had_slot) os_semaphore_release(&q->empty_slots);
        return OS_FAIL;
    }

    q->buffer[q->head] = message;
    q->head = (q->head + 1) % q->capacity;

    if (!had_slot)
    {
        /* Full: the write above landed on the oldest entry, so the reader has
           to skip it. Filled count is unchanged, hence no filled_slots post. */
        q->tail = (q->tail + 1) % q->capacity;
    }

    os_mutex_release(&q->lock);

    if (had_slot) os_semaphore_release(&q->filled_slots);

    return OS_SUCCESS;
}

uint8_t os_queue_receive(os_message_queue_t *q, uint32_t *buffer, uint32_t timeout)
{
    if (os_semaphore_acquire(&q->filled_slots, timeout) != OS_SUCCESS)
    {
        return OS_TIMEOUT;   /* Nothing to read; the mutex was never touched */
    }

    if (os_mutex_acquire(&q->lock, WAIT_FOREVER) != OS_SUCCESS)
    {
        os_semaphore_release(&q->filled_slots);
        return OS_FAIL;
    }

    *buffer = q->buffer[q->tail];
    q->tail = (q->tail + 1) % q->capacity;

    os_mutex_release(&q->lock);

    os_semaphore_release(&q->empty_slots);

    return OS_SUCCESS;
}

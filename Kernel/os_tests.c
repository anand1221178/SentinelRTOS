#include "os_kernel.h"
#include "os_queue.h"
#include "uart.h"

/*
 * Power-on self-test. Runs before os_kernel_launch(), so nothing here may
 * block: every call uses a zero timeout or an uncontested resource, and the
 * kernel refuses to block while !kernel_running anyway.
 */

static void fail(const char *why)
{
    uart_print("FAIL: ");
    uart_print(why);
    uart_print("\r\n");
    for (;;) { }
}

static void test_mutex_basic(void)
{
    os_mutex_t m;
    os_mutex_init(&m);

    uart_print("[POST] Mutex... ");

    if (os_mutex_acquire(&m, 0) != OS_SUCCESS) fail("acquire rejected");
    if (m.lock != 1) fail("lock not set");

    /* Second acquire must not succeed and must not deadlock the caller. */
    if (os_mutex_acquire(&m, 0) == OS_SUCCESS) fail("re-acquired a held mutex");

    os_mutex_release(&m);
    if (m.lock != 0 || m.owner != NULL) fail("lock not cleared");

    uart_print("PASS\r\n");
}

static void test_semaphore_basic(void)
{
    os_semaphore_t s;
    os_semaphore_init(&s, 2, 2);

    uart_print("[POST] Semaphore... ");

    if (os_semaphore_acquire(&s, 0) != OS_SUCCESS || s.count != 1) fail("first take");
    if (os_semaphore_acquire(&s, 0) != OS_SUCCESS || s.count != 0) fail("second take");
    if (os_semaphore_acquire(&s, 0) != OS_TIMEOUT) fail("empty take should time out");

    os_semaphore_release(&s);
    if (s.count != 1) fail("give");

    os_semaphore_release(&s);
    os_semaphore_release(&s);
    if (s.count != 2) fail("count exceeded max");

    uart_print("PASS\r\n");
}

static void test_queue_basic(void)
{
    os_message_queue_t q;
    uint32_t buffer[4];
    uint32_t out = 0;

    uart_print("[POST] Queue... ");

    os_queue_init(&q, buffer, 4);

    os_queue_send(&q, 0xDEADBEEF, 0);
    if (os_queue_receive(&q, &out, 0) != OS_SUCCESS) fail("receive");
    if (out != 0xDEADBEEF) fail("data mismatch");

    /* FIFO order */
    for (uint32_t i = 1; i <= 4; i++) os_queue_send(&q, i, 0);
    for (uint32_t i = 1; i <= 4; i++)
    {
        if (os_queue_receive(&q, &out, 0) != OS_SUCCESS || out != i) fail("FIFO order");
    }

    if (os_queue_receive(&q, &out, 0) != OS_TIMEOUT) fail("empty receive should time out");

    /* Overflow overwrites the oldest sample rather than dropping the newest. */
    for (uint32_t i = 1; i <= 6; i++) os_queue_send(&q, i, 0);
    if (os_queue_receive(&q, &out, 0) != OS_SUCCESS || out != 3) fail("overwrite policy");

    uart_print("PASS\r\n");
}

void os_run_post(void)
{
    uart_print("\r\n--- STARTING KERNEL POST ---\r\n");

    test_mutex_basic();
    test_semaphore_basic();
    test_queue_basic();

    uart_print("--- KERNEL READY ---\r\n\r\n");
}

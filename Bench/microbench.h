#ifndef __MICROBENCH_H__
#define __MICROBENCH_H__

#include "stm32f4xx.h"
#include "os_kernel.h"
#include "uart.h"

#define BENCH_ITERATIONS 100

/** Enables the DWT cycle counter. Call once, before any bench function. */
void microbench_init(void);

/* Single-threaded benchmarks. Safe to run before os_kernel_launch(). */
void bench_mutex_overhead(void);
void bench_semaphore_overhead(void);
void bench_queue_roundtrip(void);
void bench_scheduler_decision(void);

/** Runs every single-threaded benchmark and prints a table over UART. */
void microbench_run_all(void);

/* Context-switch ping-pong. Create both as tasks at equal priority; the ping
 * task prints the per-switch cost and then idles. */
void bench_ping_task(void);
void bench_pong_task(void);

#endif

#ifndef TASKS_H
#define TASKS_H

#include "os_task.h"
#include "os_kernel.h"
#include "os_queue.h"
#include "stm32f4xx.h"

/* Shared resources, defined in main.c */
extern os_message_queue_t sweep_queue;
extern os_semaphore_t echo_ready;
extern os_mutex_t uart_lock;

void os_idle_task(void);
void sweep_task(void);
void radar_task(void);

#endif

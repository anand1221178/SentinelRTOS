# MicroBench — SENTINEL vs FreeRTOS

## What is measured

Every figure is a raw core-cycle count from the Cortex-M4 DWT cycle counter
(`DWT->CYCCNT`), which increments once per CPU clock and costs a single load to
read, so the instrument does not perturb what it measures. At the 16MHz HSI
clock used here, 16 cycles = 1us.

Each benchmark runs `BENCH_ITERATIONS` (100) times and reports the mean, with
the cost of an empty measurement loop subtracted, so what is reported is the
kernel operation rather than the harness around it.

| Benchmark | What it covers |
|---|---|
| Mutex acquire+release | Uncontested lock/unlock, the common path |
| Semaphore take+give | Uncontested counting-semaphore pair |
| Queue send+receive | One 32-bit message through the ring: 2 semaphores + 1 mutex |
| Scheduler decision | `os_scheduler()` alone — the O(tasks) part of a switch |
| Ping-pong round trip | 2 full PendSV context switches + 4 semaphore ops |
| Context switch | Round trip / 2 — the headline switch latency |

## Running it

```bash
make all      # Application build: the single-threaded benches print at boot
make bench    # Replaces the radar tasks with the ping-pong context-switch pair
```

Results come out over USART2 at 115200 baud (ST-LINK virtual COM port).

## The FreeRTOS comparison

The ping-pong benchmark is deliberately the same shape as the standard FreeRTOS
context-switch measurement, so the two numbers describe the same thing:

| SENTINEL | FreeRTOS equivalent |
|---|---|
| `os_task_create(fn, stack, STACK_SIZE, prio)` | `xTaskCreateStatic(fn, ..., prio, stack, &tcb)` |
| `os_semaphore_release(&pong_sem)` | `xSemaphoreGive(pong_sem)` |
| `os_semaphore_acquire(&ping_sem, WAIT_FOREVER)` | `xSemaphoreTake(ping_sem, portMAX_DELAY)` |
| `os_delay(ms)` | `vTaskDelay(pdMS_TO_TICKS(ms))` |

To reproduce the comparison run, build FreeRTOS v10.x for the same board with:
`configCPU_CLOCK_HZ 16000000`, `configTICK_RATE_HZ 1000`,
`configUSE_PREEMPTION 1`, `configUSE_TIME_SLICING 1`,
`configUSE_MUTEXES 1`, `configUSE_TASK_NOTIFICATIONS 0`, heap_1, and the same
`-O2` flag, then port `Bench/microbench.c` one-for-one using the table above.
Identical clock, tick rate and optimisation level are what make the comparison
meaningful; a FreeRTOS build at a different clock is not a comparison.

## Results

Measured on a Nucleo-F411RE at 16MHz.

| Operation | SENTINEL @ -O0 | SENTINEL @ -O2 | FreeRTOS @ -O2 |
|---|---|---|---|
| Mutex acquire+release (uncontested) | 94 cycles (5.9us) | 42 cycles (2.6us) | pending |
| Semaphore take+give | pending | pending | pending |
| Queue send+receive | pending | pending | pending |
| Scheduler decision (3 tasks) | pending | pending | pending |
| Context switch | pending | pending | pending |

The mutex row is from hardware. The remaining rows are pending re-measurement:
the benchmarks exist and build, but the board is not currently on hand, and
publishing numbers that were not actually measured would defeat the point of
owning a cycle counter.

## What the design implies before measuring

Two structural differences from FreeRTOS are worth stating up front, because
they predict which way the numbers should fall:

- **Scheduler cost is O(n) in tasks.** SENTINEL scans the TCB array for the
  highest-priority ready task. FreeRTOS keeps one ready list per priority and
  uses a `CLZ`-based lookup, which is O(1). With `MAX_TASKS` at 6 the linear
  scan is a handful of cycles and wins on constant factors; the ordering would
  invert well before a few dozen tasks.
  *ponytail: linear scan, per-priority ready bitmap if the task count grows.*
- **The context switch itself is the same work.** Both stack R4-R11 in PendSV
  and let the hardware do the rest, so the switch cost should land in the same
  range, with the difference coming from the scheduler decision and from
  FreeRTOS's extra bookkeeping (run-time stats, list housekeeping).

## Caveats

- 16MHz HSI, no PLL, flash wait states at their reset value. Every figure scales
  with the clock, so the cycle counts are the portable number, not the
  microseconds.
- `DWT->CYCCNT` is a 32-bit counter: at 16MHz it wraps roughly every 4.5
  minutes. Unsigned subtraction handles a single wrap, but intervals longer
  than that are meaningless.
- Benchmarks run with interrupts enabled. A SysTick landing inside a measured
  loop shows up as an outlier, which is why the mean over 100 iterations is
  reported rather than a single shot.

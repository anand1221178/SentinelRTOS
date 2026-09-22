# SENTINEL RTOS

SENTINEL is a preemptive real-time kernel for the ARM Cortex-M4 (STM32F411),
written from scratch: no vendor HAL, no CMSIS driver layer, no third-party
kernel. The application on top of it is a radar-style perimeter scanner — a
servo sweeps an ultrasonic sensor across 180 degrees while a second task turns
echo timings into distances.

## Kernel

* **Preemptive scheduler.** Fixed-priority, with round-robin time slicing
  between tasks of equal priority. SysTick pends PendSV on every tick, so a task
  that becomes ready takes the CPU at the next tick boundary rather than waiting
  for the running task to cooperate.
* **Context switching in ARM assembly** (`Kernel/os_kernel_asm.s`). PendSV runs
  at the lowest exception priority and stacks R4-R11 by hand; the hardware
  stacks and unstacks the rest.
* **Dual-stack.** Kernel and ISRs run on MSP, tasks on PSP, so a task blowing
  its stack cannot take the exception handlers down with it. Each stack also
  carries a canary that the scheduler checks on every switch.
* **Synchronization.** Mutexes with **priority inheritance** and direct handoff,
  counting and binary semaphores, and bounded message queues built from both.
  Every blocking call takes a timeout (`0` polls, `WAIT_FOREVER` blocks).
* **POST.** A power-on self-test exercises the primitives over UART before the
  scheduler starts.
* **MicroBench.** Cycle-accurate measurement of kernel operations via the DWT
  counter — see [BENCHMARKS.md](BENCHMARKS.md).

Priority convention: **higher number = higher priority**, and the idle task sits
at 0.

## Drivers

All bare-metal register code, no HAL:

| Driver | Peripheral | Pins |
|---|---|---|
| `uart.c` | USART2, 115200 8N1, polled | PA2 (TX) |
| `gpio.c` | Mode/AF/pull config, atomic BSRR writes | any |
| `pwm.c` | TIM3_CH1, 1us resolution | PA6 |
| `servo.c` | SG90 angle → pulse width, on top of `pwm.c` | PA6 |
| `ultrasonic.c` | HC-SR04, TIM2 both-edge input capture | PA0 (trig), PA1 (echo) |
| `spi.c` | SPI1 master, full duplex, software CS | PB3/4/5, PB6 (CS) |
| `i2c.c` | I2C1 master, 100kHz, bounded waits | PB8 (SCL), PB9 (SDA) |
| `adc.c` | ADC1, 12-bit single conversion | PA0-PA7 |

## Layout

```
Inc/, Kernel/    kernel: scheduler, primitives, PendSV assembly, POST
Drivers/         peripheral drivers
Tasks/           application tasks (sweep, radar, idle)
Bench/           MicroBench suite
Test/            host-side kernel tests (no board required)
Src/main.c       entry point and system wiring
```

## Build

Needs the `arm-none-eabi` toolchain, `make`, and CMSIS headers (point `CMSIS=`
at your copy if it is not in `~/stm32_dev/CMSIS`).

```bash
make all      # Build firmware into build/all.elf
make bench    # Build with the context-switch benchmark tasks instead of the app
make test     # Run the kernel logic tests on the host — no hardware needed
make flash    # Flash via OpenOCD + GDB (make load in another terminal first)
```

`make test` compiles the scheduler, the timeout bookkeeping, the priority
inheritance rules and the queue against a small register stub (`Test/`) and
exercises them with plain assertions, which covers everything about the kernel
that is not assembly or a peripheral register.

## Roadmap

- Per-priority ready bitmap to make scheduling O(1)
- Tickless idle
- Recursive mutexes and per-mutex priority-donation tracking
- Ground station: live radar display over the ESP8266 link

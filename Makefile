# --- Toolchain ---
CC       = arm-none-eabi-gcc
OBJCOPY  = arm-none-eabi-objcopy
SIZE     = arm-none-eabi-size
MACH     = cortex-m4

CMSIS   ?= /Users/anand/stm32_dev/CMSIS

# -O2 roughly halves the kernel primitive cost vs -O0 (see BENCHMARKS.md), so
# the benchmarks and the shipping build both use it.
OPT     ?= -O2

CFLAGS  = -mcpu=$(MACH) -mthumb -std=gnu11 -DSTM32F411xE -g $(OPT) \
          -Wall -Wextra -Wno-unused-parameter -ffunction-sections -fdata-sections
LDFLAGS = -mcpu=$(MACH) -mthumb -nostartfiles -T stm32_ls.ld \
          --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Wl,-Map=build/all.map

INCLUDES = -IInc -ITasks -IKernel -IDrivers/Inc -IBench \
           -I$(CMSIS)/Include -I$(CMSIS)/Device/ST/STM32F4xx/Include

BUILD = build

SRCS = Src/main.c \
       Kernel/os_kernel.c Kernel/os_queue.c Kernel/os_tests.c \
       Tasks/tasks.c \
       Bench/microbench.c \
       Drivers/Src/uart.c Drivers/Src/gpio.c Drivers/Src/pwm.c Drivers/Src/spi.c \
       Drivers/Src/i2c.c Drivers/Src/adc.c Drivers/Src/servo.c Drivers/Src/ultrasonic.c \
       stm32f411_startup.c

ASMS = Kernel/os_kernel_asm.s

OBJS = $(addprefix $(BUILD)/,$(SRCS:.c=.o) $(ASMS:.s=.o))
DEPS = $(OBJS:.o=.d)

# --- Build ---
all: $(BUILD)/all.elf

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(INCLUDES) -MMD -MP $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/all.elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@
	$(SIZE) $@

# Builds the same firmware with the ping-pong context-switch benchmark tasks
# in place of the radar application.
bench:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -DSENTINEL_BENCH" all

# --- Host-side kernel tests (no board required) ---
test:
	gcc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -ITest -IInc \
	    Test/test_kernel.c Kernel/os_kernel.c Kernel/os_queue.c -o $(BUILD)/test_kernel
	./$(BUILD)/test_kernel

# --- Flash ---
flash: $(BUILD)/all.elf
	arm-none-eabi-gdb -batch $(BUILD)/all.elf \
	-ex "target remote localhost:3333" \
	-ex "monitor reset halt" \
	-ex "monitor flash write_image erase $(BUILD)/all.elf" \
	-ex "monitor reset halt" \
	-ex "monitor resume"

load:
	openocd -f board/st_nucleo_f4.cfg

clean:
	rm -rf $(BUILD)

.PHONY: all bench test flash load clean

-include $(DEPS)

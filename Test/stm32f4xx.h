/*
 * Host-side stand-in for the CMSIS device header, so the kernel and queue can
 * be compiled and exercised with plain gcc on a workstation. Only the registers
 * and intrinsics the kernel actually touches are modelled; everything is a
 * plain variable, which also makes the register writes observable from tests.
 */
#ifndef STM32F4XX_H_STUB
#define STM32F4XX_H_STUB

#include <stdint.h>

typedef struct { volatile uint32_t ICSR; } SCB_Type;
typedef struct { volatile uint32_t CTRL, LOAD, VAL; } SysTick_Type;
typedef struct { volatile uint32_t CR; } DBGMCU_Type;

extern SCB_Type     *SCB;
extern SysTick_Type *SysTick;
extern DBGMCU_Type  *DBGMCU;

#define SCB_ICSR_PENDSVSET_Msk      (1UL << 28)
#define SysTick_CTRL_ENABLE_Msk     (1UL << 0)
#define SysTick_CTRL_TICKINT_Msk    (1UL << 1)
#define SysTick_CTRL_CLKSOURCE_Msk  (1UL << 2)

#define DBGMCU_CR_DBG_SLEEP         (1UL << 0)
#define DBGMCU_CR_DBG_STOP          (1UL << 1)
#define DBGMCU_CR_DBG_STANDBY       (1UL << 2)

typedef enum { PendSV_IRQn = -2, SysTick_IRQn = -1 } IRQn_Type;

/* Interrupt masking is modelled by a counter: the tests only need to prove the
   kernel leaves PRIMASK the way it found it, never that interrupts really fire. */
extern uint32_t stub_primask;

static inline uint32_t __get_PRIMASK(void)        { return stub_primask; }
static inline void     __set_PRIMASK(uint32_t v)  { stub_primask = v; }
static inline void     __disable_irq(void)        { stub_primask = 1; }
static inline void     __enable_irq(void)         { stub_primask = 0; }
static inline void     __DSB(void)                { }
static inline void     __ISB(void)                { }
static inline void     __NOP(void)                { }
static inline void     NVIC_SetPriority(IRQn_Type irq, uint32_t prio) { (void)irq; (void)prio; }

#endif

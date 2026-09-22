.syntax unified
.thumb
.text

@ current_tcb->stackPtr is at offset 0 of the TCB (see os_task.h).
.extern current_tcb
.extern os_scheduler

.global os_start_first_task
.global PendSV_Handler

@ ---------------------------------------------------------------------------
@ PendSV_Handler - the context switch.
@ The hardware already stacked R0-R3, R12, LR, PC and xPSR on the outgoing
@ task's PSP before entry, and pops them again on exception return. All this
@ handler owns is R4-R11 (the "software frame") and the PSP bookkeeping.
@ ---------------------------------------------------------------------------
.thumb_func
PendSV_Handler:
    @ --- 1. Save the outgoing context ---
    MRS     R0, PSP                 @ R0 = outgoing task's stack pointer
    STMDB   R0!, {R4-R11}           @ Push R4-R11 onto that task's stack

    LDR     R1, =current_tcb
    LDR     R1, [R1]                @ R1 = current_tcb
    STR     R0, [R1]                @ current_tcb->stackPtr = R0

    @ --- 2. Pick the next task ---
    PUSH    {LR}                    @ EXC_RETURN must survive the C call
    BL      os_scheduler
    POP     {LR}

    @ --- 3. Restore the incoming context ---
    LDR     R1, =current_tcb
    LDR     R1, [R1]                @ R1 = new current_tcb
    LDR     R0, [R1]                @ R0 = new_tcb->stackPtr

    LDMIA   R0!, {R4-R11}           @ Pop its R4-R11
    MSR     PSP, R0                 @ Hardware pops the rest from here

    BX      LR                      @ Exception return into the new task

@ ---------------------------------------------------------------------------
@ os_start_first_task - one-way trip from main() into the first task.
@ Called from Thread mode, where EXC_RETURN is not available, so the initial
@ frame is skipped by hand: set PSP past it and branch to the entry point.
@ ---------------------------------------------------------------------------
.thumb_func
os_start_first_task:
    LDR     R0, =current_tcb
    LDR     R1, [R0]
    LDR     R0, [R1]                @ R0 = stackPtr (bottom of the R4-R11 frame)

    LDR     R1, [R0, #56]           @ PC slot: 8 words (R4-R11) + 6 (R0-R3,R12,LR)
    ADDS    R0, R0, #64             @ Discard the whole initial frame
    MSR     PSP, R0

    MOVS    R2, #2                  @ CONTROL.SPSEL = 1: threads run on PSP,
    MSR     CONTROL, R2             @ handlers keep using MSP
    ISB

    CPSIE   I
    BX      R1                      @ Run the task

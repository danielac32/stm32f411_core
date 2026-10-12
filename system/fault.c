#include <stdint.h>

typedef struct {
    uint32_t magic;   /* 0xDEADBEEF if valid */
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t mmfar;
    uint32_t bfar;
    uint32_t stacked_pc;
    uint32_t stacked_lr;
    uint32_t stacked_sp;
    uint32_t stacked_r0;
    uint32_t stacked_psr;
    uint32_t lr_excep;
    /* Caller return address: the faulting function's pushed {lr} on the
     * stack, located right below the 8-word exception frame (sp[8]). If
     * the faulting function did not push {lr} this is the word below the
     * frame (may be a scratch value). */
    uint32_t stacked_caller;
} fault_info_t;

/* Place at known address in SRAM1 (0x20000000 region) */
__attribute__((section(".fault_info"), used))
volatile fault_info_t _fault_info = {0};

/* Cortex-M4 fault status registers */
#define SCB_CFSR   (*(volatile uint32_t*)0xE000ED28)
#define SCB_HFSR   (*(volatile uint32_t*)0xE000ED2C)
#define SCB_MMAR   (*(volatile uint32_t*)0xE000ED34)
#define SCB_BFAR   (*(volatile uint32_t*)0xE000ED38)
#define SCB_AFSR   (*(volatile uint32_t*)0xE000ED3C)

__attribute__((naked))
void HardFault_Handler(void)
{
    __asm volatile(
        "TST  LR, #4        \n"
        "ITE  EQ            \n"
        "MRSEQ R0, MSP      \n"
        "MRSNE R0, PSP      \n"
        "B    _fault_capture \n"
    );
}

__attribute__((used))
void _fault_capture(uint32_t *sp)
{
    /* Clobber magic first so it's 0 if we're still writing */
    _fault_info.magic = 0;

    _fault_info.stacked_r0  = sp[0];
    _fault_info.stacked_lr  = sp[5];
    _fault_info.stacked_pc  = sp[6];
    _fault_info.stacked_psr = sp[7];
    _fault_info.stacked_caller = sp[8];
    _fault_info.stacked_sp  = (uint32_t)sp;

    _fault_info.cfsr  = SCB_CFSR;
    _fault_info.hfsr  = SCB_HFSR;
    _fault_info.mmfar = SCB_MMAR;
    _fault_info.bfar  = SCB_BFAR;
    _fault_info.lr_excep = sp[5]; /* stacked EXC_RETURN/LR */

    /* Signal valid */
    _fault_info.magic = 0xDEADBEEF;

    /* Busy-loop (not WFI sleep): st-util/ST-LINK SWD wedges when the core
     * is left sleeping in WFI after a fault, and physical replug of the
     * probe was needed to recover. A busy loop keeps the debugger able to
     * halt and read memory. */
    for (;;) {
        __asm volatile("nop");
    }
}

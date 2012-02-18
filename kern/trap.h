/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_TRAP_H
#define JOS_KERN_TRAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/trap.h>
#include <inc/mmu.h>

/* The kernel's interrupt descriptor table */
extern struct Gatedesc idt[];

void idt_init(void);
void print_regs(struct PushRegs *regs);
void print_trapframe(struct Trapframe *tf);
void page_fault_handler(struct Trapframe *);
void backtrace(struct Trapframe *);

void divide_by_zero(void);
void debug_exception(void);
void nmi_exception(void);
void brkpt_exception(void);
void overflow_exception(void);
void bound_exception(void);
void illegal_exception(void);
void device_exception(void);
void dblflt_exception(void);
void tss_exception(void);
void segnp_exception(void);
void stack_exception(void);
void gpflt_exception(void);
void pgflt_exception(void);
void fperr_exception(void);
void align_exception(void);
void mchk_exception(void);
void simderr_exception(void);
void syscall_exception(void);
void interupt_timer_handle(void);
void interupt_spurious( void ) ;
void interupt_ide ( void ) ;
void interupt_kbd ( void ) ;
void interupt_serial( void ) ;
void interupt_nic_card ( void) ;
void default_exception(void);

#endif /* JOS_KERN_TRAP_H */

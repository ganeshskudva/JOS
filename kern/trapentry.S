/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* The TRAPHANDLER macro defines a globally-visible function for handling
 * a trap.  It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 */ 
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(divide_by_zero,0)
TRAPHANDLER_NOEC(debug_exception,T_DEBUG)
TRAPHANDLER_NOEC(nmi_exception,T_NMI)
TRAPHANDLER_NOEC(brkpt_exception,T_BRKPT)
TRAPHANDLER_NOEC(overflow_exception,T_OFLOW)
TRAPHANDLER_NOEC(bound_exception,T_BOUND)
TRAPHANDLER_NOEC(illegal_exception,T_ILLOP)
TRAPHANDLER_NOEC(device_exception,T_DEVICE)
TRAPHANDLER(dblflt_exception,T_DBLFLT)
TRAPHANDLER(tss_exception,T_TSS)
TRAPHANDLER(segnp_exception,T_SEGNP)
TRAPHANDLER(stack_exception,T_STACK)
TRAPHANDLER(gpflt_exception,T_GPFLT)
TRAPHANDLER(pgflt_exception,T_PGFLT)
TRAPHANDLER_NOEC(fperr_exception,T_FPERR)
TRAPHANDLER(align_exception,T_ALIGN)
TRAPHANDLER_NOEC(mchk_exception,T_MCHK);
TRAPHANDLER_NOEC(simderr_exception,T_SIMDERR);
TRAPHANDLER_NOEC(syscall_exception,T_SYSCALL);
TRAPHANDLER_NOEC(interupt_timer_handle,T_SYSTIMER);
TRAPHANDLER_NOEC(interupt_kbd,T_KBD ) ;
TRAPHANDLER_NOEC(interupt_serial,T_SERIAL );
TRAPHANDLER_NOEC(interupt_spurious,T_SPURIOUS);
TRAPHANDLER_NOEC(interupt_nic_card,T_NIC_CARD);
TRAPHANDLER_NOEC(interupt_ide,T_IDE);
TRAPHANDLER_NOEC(default_exception,T_DEFAULT);


/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
/* As per The Intel Manual the INT instruction will save the following 
 * user land values
 * SS,ESP,EFLAGS,CS,EIP , ErrorCode on the stack.
 * the Macro Pushes 0 ( instead of ErrCode on stack ) 
 * The Macro pushes Trap no .
 * Now, we need to push DS,ES and tf_reg contents
 */
 push %ds
 push %es
 pushal
 movw $GD_KD,%ax
 movw %ax,%ds
 movw %ax,%es
 push %esp       /* this is pointer to Trapframe */
 call trap
 addl $0x04,%esp /* Start Pointing at TrapFrame */
 popal           /* Populate the Registers from Trapframe */
 popl %es
 popl %ds
 addl $0x08,%esp /* Just Ignore the Ernno and the trap no */
 iret
 

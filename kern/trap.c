//============================================
// Manikantan S/Ganesh 10/01/11 Lab3
// Manikantan S/Ganesh 10/03/11 Lab3b
//=============================================

#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/time.h>
#include <kern/e100.h>
#include <inc/string.h>

static struct Taskstate ts;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

         cprintf("trapno is %d\n");
	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}


void
idt_init(void)
{
	extern struct Segdesc gdt[];
	// LAB 3: Your code here.
        SETGATE(idt[T_DIVIDE],0,GD_KT,divide_by_zero,3);
        SETGATE(idt[T_DEBUG],0,GD_KT,debug_exception,3);
        SETGATE(idt[T_NMI],0,GD_KT,nmi_exception,3);
        SETGATE(idt[T_BRKPT],0,GD_KT,brkpt_exception,3);
        SETGATE(idt[T_OFLOW],0,GD_KT,overflow_exception,3);
        SETGATE(idt[T_BOUND],0,GD_KT,bound_exception,3);
        SETGATE(idt[T_ILLOP],0,GD_KT,illegal_exception,3);
        SETGATE(idt[T_DEVICE],0,GD_KT,device_exception,3);
        SETGATE(idt[T_DBLFLT],0,GD_KT,dblflt_exception,3);
        SETGATE(idt[T_TSS],0,GD_KT,tss_exception,3);
        SETGATE(idt[T_SEGNP],0,GD_KT,segnp_exception,3);
        SETGATE(idt[T_STACK],0,GD_KT,stack_exception,3);
        SETGATE(idt[T_GPFLT],0,GD_KT,gpflt_exception,3);
        SETGATE(idt[T_PGFLT],0,GD_KT,pgflt_exception,0);
        SETGATE(idt[T_FPERR],0,GD_KT,fperr_exception,3);
        SETGATE(idt[T_ALIGN],0,GD_KT,align_exception,3);
        SETGATE(idt[T_MCHK],0,GD_KT,mchk_exception,3);
        SETGATE(idt[T_SIMDERR],0,GD_KT,simderr_exception,3);
        SETGATE(idt[T_SYSCALL],0,GD_KT,syscall_exception,3);

        //Below are for the Interupts - gate = 0 . DPL doesnot matter.
        SETGATE(idt[T_SYSTIMER],0,GD_KT,interupt_timer_handle,3);
        SETGATE(idt[T_KBD],0,GD_KT,interupt_kbd,3);
        SETGATE(idt[T_SERIAL],0,GD_KT,interupt_serial,3);
        SETGATE(idt[T_SPURIOUS],0,GD_KT,interupt_spurious,3);
        SETGATE(idt[T_NIC_CARD],0,GD_KT,interupt_nic_card,3);
        SETGATE(idt[T_IDE],0,GD_KT,interupt_ide,3);
        SETGATE(idt[T_DEFAULT],0,GD_KT,default_exception,3);


	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;

	// Initialize the TSS field of the gdt.
	gdt[GD_TSS >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate), 0);
	gdt[GD_TSS >> 3].sd_s = 0;

	// Load the TSS
	ltr(GD_TSS);

	// Load the IDT
	asm volatile("lidt idt_pd");
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	cprintf("  err  0x%08x\n", tf->tf_err);
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	cprintf("  esp  0x%08x\n", tf->tf_esp);
	cprintf("  ss   0x----%04x\n", tf->tf_ss);
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	
	// Handle clock interrupts.
	// LAB 4: Your code here.

	// Add time tick increment to clock interrupts.
	// LAB 6: Your code here.

	// Handle spurious interrupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}


	// Handle keyboard and serial interrupts.
	// LAB 7: Your code here.


        switch( tf->tf_trapno)
        {
            case T_KBD :
            {
                  //cprintf("Handling KBD int\n");
                  kbd_intr();
                  //cprintf("DOne Handling KBD int\n");
                  return;
            }
            break;

            case T_SERIAL :
            {
               serial_intr();
            }
            break;

            case T_SYSTIMER :
            {
               time_tick();
               sched_yield();
            }
            break;
            case T_BRKPT:
            {
                 cprintf("BRKPT occured\n");
                 monitor(tf);
            }
            break;
            case T_PGFLT :
            {
                 //cprintf("Page Fault occured\n");
                 page_fault_handler(tf);
                 return;
            }
            break;
            case T_SYSCALL:
            {
                 //cprintf("system call invoked\n");

                 //If user has provided user land address, it is not 
                 //visisble here so switch the Page Directory with 
                 //the user's cr3 value
                 //assert(curenv);
                 //lcr3(curenv->env_cr3);

                 //Create some variable to pass the arguments
                 uint32_t a1,a2,a3,a4,a5,sys_num,retval;
                 sys_num = tf->tf_regs.reg_eax;
                 a1 = tf->tf_regs.reg_edx;
                 a2 = tf->tf_regs.reg_ecx;
                 a3 = tf->tf_regs.reg_ebx;
                 a4 = tf->tf_regs.reg_edi;
                 a5 = tf->tf_regs.reg_esi;

                 retval = syscall(sys_num,a1,a2,a3,a4,a5);

                 //when the system call returns , populate the return val
                 //in eax register of tf
                 tf->tf_regs.reg_eax = retval;

                 //cprintf("Return %d to user from syscall\n",retval);
                 return;
            }
            break;

            case T_NIC_CARD:
            {
                cprintf("MANI - got a NIC interupt\n");
                process_nic_interupt();
                //panic("Interup from NIC card is not handled\n");
                return;
            }
            break;
        }
	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

        //cprintf("Trap number is %d\n",tf->tf_trapno);
	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		assert(curenv);
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}
	
	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNABLE)
		env_run(curenv);
	else
		sched_yield();

}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();


        //cprintf("pagefault occured at %x\n",fault_va);
	// Handle kernel-mode page faults.
	if ((tf->tf_cs & 3) == 0)
        {
            //Page Fault occured in kernel mode
            panic("Page Fault occured in kernel mode for address %x error %d\n",fault_va,tf->tf_err);
         }

	
	// LAB 3: Your code here.

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.

        /*  Push the contents esp,eflags,eip,pushal @ UXSTACKTOP
         *  Push tf_err,fault_va   
         *  Move UXSTACKTOP to ESP
         *  Move handle to EIP
         */

         //Check if user has provided the handled 
         if( curenv->env_pgfault_upcall == NULL )
         {
             //User has not registed the PageFaultHandler . Kill it
              
	     cprintf("[%08x] user fault va %08x ip %08x\n",
	     	curenv->env_id, fault_va, tf->tf_eip);
             print_trapframe(tf);
             env_destroy(curenv);
 
             return ; //for compilation
         }
         else
         {


            //First we need to push UTrapframe to the Exception stack 
            //So populate it 
            struct UTrapframe xframe;
            xframe.utf_fault_va = fault_va;
            xframe.utf_err      = tf->tf_err;
            memmove((void*)&xframe.utf_regs,(void*)&tf->tf_regs,sizeof(struct PushRegs));
            xframe.utf_eip      = tf->tf_eip;
            xframe.utf_eflags   = tf->tf_eflags;
            xframe.utf_esp      = tf->tf_esp;
   
            //Now copy it to the user exception stack , depending on 
            //whether this is the first call on stack or consequent call
   
            uint32_t esp_for_xframe = 0x00;
            //check if there is already a User Exception being handled on
            // exception stack 
            if(( tf->tf_esp >= (UXSTACKTOP - PGSIZE) ) &&
               ( tf->tf_esp <= (UXSTACKTOP - 1) )
              )
              {
                  //cprintf("[%08x] : A Page fault has occured in User space,when handling another page fault VA %x erro %d\n",curenv->env_id,fault_va,tf->tf_err);

                  //Since there is already a Page fault exception happening 
                  //on the User Exception stack . Leave  one word below 
                  //tf->tf_esp and then copy the xframe.
   
                  esp_for_xframe  = tf->tf_esp - 0x04; 
                  esp_for_xframe = esp_for_xframe - sizeof(struct UTrapframe); 
              }
              else
              {
   
                  //cprintf("size : %d top %x\n",sizeof(struct UTrapframe),UXSTACKTOP);
              //cprintf("[%08x] : Page Fault occured in User Space ( first time) move to exception stack va %x error %d\n",curenv->env_id,fault_va,tf->tf_err);
                   esp_for_xframe = UXSTACKTOP - sizeof(struct UTrapframe);
               }
         //check if the user had allocated a exception stack
         user_mem_assert(curenv,(void*)esp_for_xframe,sizeof(struct UTrapframe),PTE_U);

              //The user code should have mapped a page for UXSTACK
              memmove((void*) esp_for_xframe,
                     (void*)&xframe,
                     sizeof(struct UTrapframe));
   
   
   
              tf->tf_esp = esp_for_xframe;
              tf->tf_eip = (uint32_t)curenv->env_pgfault_upcall;

  
 //             cprintf("REGISTER VALYES ARE - \n");
              
//             cprintf("va %x utf_err %x \n",xframe.utf_fault_va,xframe.utf_err);
//	     print_regs(&xframe.utf_regs);
 //            cprintf("eip %x eflags %x esp %x\n",xframe.utf_eip,xframe.utf_eflags,xframe.utf_esp);

              env_run(curenv);
         }
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return.  In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free.  In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	// LAB 4: Your code here.

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}


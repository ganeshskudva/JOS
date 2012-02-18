#include <inc/mmu.h>
#include <inc/memlayout.h>


// Page fault upcall entrypoint.

// This is where we ask the kernel to redirect us to whenever we cause
// a page fault in user space (see the call to sys_set_pgfault_handler
// in pgfault.c).
//
// When a page fault actually occurs, the kernel switches our ESP to
// point to the user exception stack if we're not already on the user
// exception stack, and then it pushes a UTrapframe onto our user
// exception stack:
//
//	trap-time esp
//	trap-time eflags
//	trap-time eip
//	utf_regs.reg_eax
//	...
//	utf_regs.reg_esi
//	utf_regs.reg_edi
//	utf_err (error code)
//	utf_fault_va            <-- %esp
//
// If this is a recursive fault, the kernel will reserve for us a
// blank word above the trap-time esp for scratch work when we unwind
// the recursive call.
//
// We then have call up to the appropriate page fault handler in C
// code, pointed to by the global variable '_pgfault_handler'.

.text
.globl _pgfault_upcall
_pgfault_upcall:
	// Call the C page fault handler.
	pushl %esp			// function argument: pointer to UTF
	movl _pgfault_handler, %eax
	call *%eax
	addl $4, %esp			// pop function argument
	
	// Now the C page fault handler has returned and you must return
	// to the trap time state.
	// Push trap-time %eip onto the trap-time stack.
	//
	// Explanation:
	//   We must prepare the trap-time stack for our eventual return to
	//   re-execute the instruction that faulted.
	//   Unfortunately, we can't return directly from the exception stack:
	//   We can't call 'jmp', since that requires that we load the address
	//   into a register, and all registers must have their trap-time
	//   values after the return.
	//   We can't call 'ret' from the exception stack either, since if we
	//   did, %esp would have the wrong value.
	//   So instead, we push the trap-time %eip onto the *trap-time* stack!
	//   Below we'll switch to that stack and call 'ret', which will
	//   restore %eip to its pre-fault value.
	//
	//   In the case of a recursive fault on the exception stack,
	//   note that the word we're pushing now will fit in the
	//   blank word that the kernel reserved for us.
	//
	// Throughout the remaining code, think carefully about what
	// registers are available for intermediate calculations.  You
	// may find that you have to rearrange your code in non-obvious
	// ways as registers become unavailable as scratch space.
	//
	// LAB 4: Your code here.

        //I need to move the UTF.EIP to Trapframe.traptime.esp
        //First get UTF.EIP into eax
        //then get Trapframe.traptime.esp iinto ebx
        //then move the  contents of eax to (%ebx)
        addl $0x28 ,%esp    //Point ESP to UTF.eip
        popl %eax           //Pop the value to eax
        subl $0x04,%esp     //adjust esp since pop incremtned by 4
        addl $0x08, %esp    //point ESP to UTF.esp

        //Now update UTF.ESP = UTF.ESP-4
        movl (%esp),%edx
        subl $0x04,%edx
        movl %edx,(%esp)

        popl %ebx           //pop the value to ebx
        subl $0x04,%esp     //adjust esp since pop incremented by 4

        movl %eax,(%ebx)   //Move the eip in eax to stack pointed by ebx
        
        subl $0x08,%esp
        subl $0x28,%esp   //Move the ESP back to fault_va


	// Restore the trap-time registers.  After you do this, you
	// can no longer modify any general-purpose registers.
	// LAB 4: Your code here.

        //point your ESP to start of utf.registers and then popal
        addl $0x08,%esp
        popal
        subl $0x20,%esp  //Undo what popa did to ESP
        subl $0x08,%esp

	// Restore eflags from the stack.  After you do this, you can
	// no longer use arithmetic operations or anything else that
	// modifies eflags.
	// LAB 4: Your code here.

        //Move the esp to point to utf.eflags and pop it to eflags
        addl $0x2C,%esp
        popfl
      

	// Switch back to the adjusted trap-time stack.
	// LAB 4: Your code here.
        popl %esp

	// Return to re-execute the instruction that faulted.
	// LAB 4: Your code here.
        ret


/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e100.h>
#include <inc/module_util.h>

static int sys_env_destroy(envid_t envid);

int sys_load_kernel_module(uint32_t * load_addr,uint32_t filesize)
{
     cprintf("USer invoked the system call for lkm %x %d freemem %x\n",load_addr,filesize,boot_freemem);

     load_kernel_module(load_addr,filesize);
     return 0;
}

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.
         //cprintf("sys_cputs is called with addr %x\n",s);
	
         //cprintf("s is %x len %d\n",s,len);
	// LAB 3: Your code here.
          user_mem_assert(curenv, s, len, PTE_U);
          //cprintf("MEM check fone\n");

         //If page is *not* mapped , then let the Page Fault 
         //Make the kernel bring the page in to RAM.

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
        //cprintf("syscall - sys_getenvid called\n");
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
        //First get the environment
         struct Env *child_env_ptr = NULL;
        
         //Assuming that this function always runs the context of te
         //process, which invoked this system call, use curenv to get ppid.
         int ret_val = env_alloc(&child_env_ptr,curenv->env_id); 
         if ( ret_val != 0 ) 
         {
             panic("Unable to alloc a Environment\n");
             return ( ret_val );
         }

         //Let the parent process decide as to when its child is ready to 
         //Run , so mark the new environment as NOT_RUNNABLE.
         child_env_ptr->env_status = ENV_NOT_RUNNABLE;

          memmove(&child_env_ptr->env_tf,&curenv->env_tf,sizeof(struct Trapframe));

         //env_alloc has copied all the needed registers except eip.
         //child returns right after sys_exofork call
         //child_env_ptr->env_tf.tf_eip = curenv->env_tf.tf_eip;

     

         //when the child takes birth from sys_exofork, it expects to 
         //to see 0 in EAX register ( child pid as given by fork )
         child_env_ptr->env_tf.tf_regs.reg_eax = 0;

         return(child_env_ptr->env_id);
	//panic("sys_exofork not implemented");
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.

        //sanity on input
        if( ( status != ENV_RUNNABLE ) &&
            ( status != ENV_NOT_RUNNABLE)
          )
          {
              return ( -E_INVAL);
          }

        struct Env * env = NULL;
        int retval = 0;
        retval = envid2env(envid,&env,1); //check the permissions.

        if( retval != 0 )
         {
             return retval ;
         }

         //set the status 
         env->env_status = status;

         return 0;
	//panic("sys_env_set_status not implemented");
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
        struct Env * env;
        int r ;
        r = envid2env(envid,&env,1);
 
       //cprintf("MANI envid %x , env %x esp %x  r = %x\n",envid,env,tf->tf_esp,r);
       if(r< 0 ) 
       {
           return r;
       }
       
        //cprintf("eip %x\n",tf->tf_eip);
        env->env_tf = *tf;
 
       return 0;
	//panic("sys_env_set_trapframe not implemented");
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
       struct Env * myenv = NULL;
       int retval = 0;
       retval = envid2env(envid,&myenv,1); //Make sure that the current env has permission to get the value of envid related env structure.
       if( retval < 0 ) 
       {
           return ( retval);
       }
       else
       {
           //cprintf("Setting the page fault handler\n");
           myenv->env_pgfault_upcall = func;
       }

      return(0) ;
       //panic("sys_env_set_pgfault_upcall not implemented");
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_USER in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
        //check ibnout parameter.
        uint32_t pg_boundary_va = ROUNDDOWN((uint32_t)va,PGSIZE);
        if( ((uint32_t)va >= UTOP) ||
            ( pg_boundary_va != (uint32_t)va )
          )
        {
            return ( -E_INVAL);
        }
        //TODO : Check for permission bits.

       //Get the env
        struct Env * env_ptr = NULL;                 
        int retval;
        retval = envid2env(envid,&env_ptr,1);
        if( retval < 0)
        {
             return( retval);
        }

        struct Page * newpage_ptr = NULL;
        retval = page_alloc(&newpage_ptr);

        if( retval <0)
        { 
           //return failure.
           return(retval);
        }
        else
        {
             memset(page2kva(newpage_ptr),0x00,PGSIZE);
             retval = page_insert(env_ptr->env_pgdir,newpage_ptr,va,perm | PTE_P);

             if(retval < 0 ) 
             {
                //Free the page allocated.
                page_free(newpage_ptr);
                return ( retval);
             }
             //cprintf("MANI: New Page mapped at %x for VA %x : id %d\n",page2kva(newpage_ptr),va,envid);
        }
        return 0;
	//panic("sys_page_alloc not implemented");
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.

         //A bunch of sanity checks.
         struct Env *src_env_ptr ,*dst_env_ptr;
         int retval;
         retval = envid2env( srcenvid,&src_env_ptr,1);
          if( retval < 0)
          {
              return -E_BAD_ENV;
          }

         retval = envid2env( dstenvid,&dst_env_ptr,1);

          if( retval < 0)
          {
              return -E_BAD_ENV;
          }

        uint32_t pg_boundary_srcva = ROUNDDOWN((uint32_t)srcva,PGSIZE);
        uint32_t pg_boundary_dstva = ROUNDDOWN((uint32_t)dstva,PGSIZE);

          if(((uint32_t)srcva >= UTOP) ||
             ((uint32_t)dstva >= UTOP) ||
             ( pg_boundary_srcva != (uint32_t)srcva ) ||
             ( pg_boundary_dstva != (uint32_t)dstva )   
             )
         {
             return -E_INVAL;
         }

         //check if srcva is mapped in srcdestid 
         pte_t * ptep = pgdir_walk(src_env_ptr->env_pgdir,srcva,0);
         if( (*ptep & PTE_P) == 0 ) 
         {
              //The given source Va is not mapped in source ENV
              return( -E_INVAL);
         }

         //TODO: permission check pending 

         //check the permission in srcenv.
         if( ( perm & PTE_W ) == 1 )
         {
            //see if the Source is mapped as Read only
            if( ( *ptep & PTE_W ) == 0 ) 
            {
               return ( -E_INVAL);
            }
         } 

         physaddr_t page_addr= PTE_ADDR(*ptep);
         struct Page * pageptr = pa2page(page_addr);
           

         //OK enough of sanity check , now do the actual work
         retval = page_insert(dst_env_ptr->env_pgdir,pageptr,dstva,perm);
         if( retval < 0 ) 
         {
               return ( -E_NO_MEM);
         }

        return(0);   
	//panic("sys_page_map not implemented"); 
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().
	// LAB 4: Your code here.
       //Get the env
        uint32_t pg_boundary_va = ROUNDDOWN((uint32_t)va,PGSIZE);
        int retval;
        if( ((uint32_t)va >= UTOP) ||
            ( pg_boundary_va != (uint32_t)va )
          )
        {
            return ( -E_INVAL);
        }

        struct Env * env_ptr = NULL;                 
        retval = envid2env(envid,&env_ptr,1);
        if( retval < 0)
        {
             return( retval);
        }

         page_remove(env_ptr->env_pgdir,va);
        
        return(retval);

//	panic("sys_page_unmap not implemented");
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
      //First Do sanity on the given inputs . 
      /** 
          - Get the Env Corresponding to given envid 
          - If Get Fails  return error
          - Now check , if the receive_env is waiting for ipc
          - If not return error.
       */
      struct Env * recv_env = 0x00;
      int result = 0;

       //cprintf("MANI : KERN SPACE : IPC SEND Requesting IPC to be sent to %x value %d perm 0x%x\n",envid,value,perm);
      //Make sure the check is set to 0, so that any Env can query the recv process env
      result = envid2env(envid ,&recv_env,0); 

      if( result != 0 )
      {
          return ( result );
      }

      //OK , Now check if the recv Process is waiting for IPC ? 
      if( recv_env ->env_ipc_recving != 1 )
      {
           //the receiving process doesnot want any sharing
       //cprintf("MANI KERN SPACE  IPC SEND env %x is not ready to receive IPC ",envid);
            return ( -E_IPC_NOT_RECV);
      }

       //Good to go - Proceed further and check sanity on given parameters
       if (
             ( (uint32_t)srcva != 0xFFFFFFFF) &&
             ( (uint32_t)srcva > UTOP )
          )
       {
           //Bad address
           return  ( -E_INVAL ) ;
       }
      
       if ( 
            ( (uint32_t)srcva != 0xFFFFFFFF) &&
            ( (uint32_t)srcva % PGSIZE != 0 ) 
         )
       {
           //Bad address - Not page aligned
           return  ( -E_INVAL ) ;
       }
 
      if( (uint32_t)srcva == 0xFFFFFFFF)
      {
           //Sending process has not given any page = 
           //set the fields in recv env and return
           result = 0 ;
           //cprintf("MANI KERN SPACE : srcva %x\n",srcva);
           goto update_values_and_return;
      }

       //check for permission bits 
       if ( 
             ( (perm & PTE_P) != PTE_P )  ||
             ( (perm & PTE_U) != PTE_U ) 
          )
       {
            //Error handling 
            return ( -E_INVAL ) ;
       } 


       //cprintf("doing a walk for %x\n",srcva);
       //OK. is the page mapped ? 
       //Just do a walk and see if address is mapped
       pte_t * entry = pgdir_walk(curenv->env_pgdir,srcva,0);
       //cprintf("walk done\n");

       if ( ( entry == NULL ) || 
            ( (*entry & PTE_P ) == 0 )
          ) 
       {
          //Page is not mapped - return error
          return ( -E_INVAL ) ;
       }

       if(
             ( (*entry & PTE_W ) == 0 ) &&
             ( ( perm & PTE_W )  == PTE_W )
          )
          {
              //user page is mapped has READ_ONLY , but it is giving the 
              //write permission on same page, to receiving process
              return ( -E_INVAL ) ;
          }

         //Set result=0, so that the recv_env fields are updated ,even if page is not mapped.
         result = 0;

        //Check if the recv process wants a page ? 
        if( (uint32_t) srcva  != 0xFFFFFFFF)
        {
             //Given pte entry , get me the page structure.
             physaddr_t pa = PTE_ADDR( *entry);
             struct Page * pageptr = pa2page(pa);

            //Now the actual work - Make sure that Reciever has requested for a page to be mapped
            if( (uint32_t)recv_env->env_ipc_dstva != 0xFFFFFFFF)
            {
              //cprintf("MANI - doing a pageinsert for recvr address %x\n",recv_env->env_ipc_dstva);
               result = page_insert(recv_env->env_pgdir,pageptr,recv_env->env_ipc_dstva,perm);
            }
            ////cprintf("page insert done\n");
        }
 
update_values_and_return : 
         if ( result == 0 )
         {
           //was able to insert the page in the recv env - update the fields
           recv_env->env_ipc_recving = 0;
           recv_env->env_ipc_value = value;
           recv_env->env_ipc_from  = curenv->env_id;
           recv_env->env_ipc_perm = perm;

           //cprintf("MANI KERN SPACE : Setting the env_id as %x\n",curenv->env_id);

           //When the recv process comes out of the ipc_recv system call
           //Ofcourse , it is * NOT * a actual return .
           //in ipc_recv , it made itself not runnable and called yield.
           //so when the scheduler runs the recv task , it will just come back to life 
           // just after int 0x30 instruction in the syscall ipc_recv in the user space.
           //there it will expect to see our return value in register EAX .
   
            recv_env->env_tf.tf_regs.reg_eax = 0;

           //Now before going out , unblock the recv env
           recv_env->env_status = ENV_RUNNABLE;

           /*** IMPORTANT - We should make the process RUNNABLE only after we set the retval in eax */

           return(0);
        }

        return ( result ) ;
      
	// LAB 4: Your code here.
	//panic("sys_ipc_try_send not implemented");
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.

        //cprintf("MANI KERN SPACE ipc recv called by %x dstva %x\n",curenv->env_id,dstva);
        if (
             ( (uint32_t)dstva != 0xFFFFFFFF ) &&
             ( (uint32_t)dstva % PGSIZE != 0 )
           )
        {
            //cprintf("MANI KERN SPACE IPC RECV - Address nt page alligned\n");
            //Not Page Aligned
            return ( - E_INVAL );
        }

        if( ((uint32_t)dstva != 0xFFFFFFFF) &&
            ((uint32_t)dstva > UTOP ) 
          )
        {
            //cprintf("MANI KERN SPACE IPC RECV - Range address incorrect\n");
            return( -E_INVAL );
        }
 
        curenv->env_ipc_recving = 1;
        curenv->env_ipc_dstva = dstva;

         //Make myself not runnable
         curenv->env_status = ENV_NOT_RUNNABLE;

         //cprintf("I am giving up the CPU \n");

         //cprintf("MANI KERN SPACE IPC RECV - %x give up CPU\n",curenv->env_id);
         //give up the CPU - Dont return back from this point
         // I will come back to CPU in the user space where this 
         // system call was invoked using int x30;
         sched_yield();

	//panic("sys_ipc_recv not implemented");
	return 0;
}

// Return the current time.
static int
sys_time_msec(void) 
{
	// LAB 6: Your code here.
	panic("sys_time_msec not implemented");
}

static
int sys_set_sched_priority(envid_t envid,int pri)
{
          
        struct Env * env_ptr = NULL;                 
        int retval = envid2env(envid,&env_ptr,1);
        if( retval < 0)
        {
             return( retval);
        }

        env_ptr->priority = pri;
        return(0);
}

static 
int sys_rcv_network_packet(uint32_t size,void * buff ) 
{
     //do some sanity of the input argument
     int result;
     user_mem_assert(curenv, buff, size, PTE_U);
     result = e100_rcv_network_packet(size,buff)  ;

     //cprintf("system call for send network packet returns\n");
     return result;
}

static 
int sys_send_network_packet(uint32_t size,void * buff ) 
{
     //do some sanity of the input argument
     user_mem_assert(curenv, buff, size, PTE_U);
     e100_send_network_packet(size,buff)  ;

     //cprintf("system call for send network packet returns\n");
     return 0;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.
        int32_t retval = 0;
        //cprintf("MANIx : sycall number %d is invoked\n",syscallno);

        switch(syscallno)
        {
            case SYS_cputs:
             sys_cputs((char*)a1,a2);
            break;
            case SYS_cgetc :
              retval = sys_cgetc();
            break;
            case SYS_getenvid:
             retval = sys_getenvid();
            break;
            case SYS_env_destroy : 
             retval = sys_env_destroy((envid_t)a1);
            break;
            case SYS_yield : 
             sys_yield();
             break;
            case SYS_exofork:
             retval = sys_exofork();
             //cprintf("sys_exofork is called . return %d\n",retval);
             break;
             case SYS_env_set_status:
             retval = sys_env_set_status(a1,a2);
             break;
             case SYS_page_alloc:
             retval = sys_page_alloc(a1,(void*)a2,a3);
             break;
             case SYS_page_map:
             retval = sys_page_map(a1,(void*)a2,a3,(void*)a4,a5);
             break;
             case SYS_page_unmap:
             retval = sys_page_unmap(a1,(void*)a2);
             break;
             case SYS_set_sched_priority:
             retval = sys_set_sched_priority(a1,a2);
             break;
             case SYS_env_set_pgfault_upcall:
             retval = sys_env_set_pgfault_upcall(a1,(void*)a2);
             break; 
             case SYS_ipc_try_send :
             retval = sys_ipc_try_send(a1,a2,(void*)a3,a4);
             break;
             case SYS_ipc_recv : 
             retval = sys_ipc_recv((void*)a1);
             break;
             case SYS_env_set_trapframe :
             retval = sys_env_set_trapframe(a1,(void*)a2);
             break;
             case SYS_time_msec :
             retval = time_msec();
             break;
             case SYS_send_network_packet:
             retval = sys_send_network_packet(a1,(void*)a2);
             break;
             case SYS_rcv_network_packet:
             retval = sys_rcv_network_packet(a1,(void*)a2);
             break;
             case SYS_load_kernel_module:
             retval = sys_load_kernel_module((void*)a1,a2);
             break;
            default :
            panic("syscall %d is not implemented\n",syscallno); 
            retval = -E_INVAL;
        }

	//panic("syscall not implemented");
         return(retval);
}


void testme()
{
}

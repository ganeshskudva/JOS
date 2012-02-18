// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
#define PTE_SHARED      0x400

void _pgfault_upcall(void);

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{

	void *addr = (void *) utf->utf_fault_va;
        uint32_t boundary_addr = ROUNDDOWN((uint32_t)addr,PGSIZE);
        uint32_t vpt_index = boundary_addr/PGSIZE;

	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

        //The user has faulted at VA utf_faultva;
       //cprintf("User Process %08x faulted with err %d: va %x\n",sys_getenvid(),err,addr);
 
        if( ( err & FEC_WR)  == FEC_WR)
        {
             //user faulted due to write operation 
             //Get the index into to vpt array , which 
             //points at this addr.
            if ( ( vpt[vpt_index] & PTE_COW ) != PTE_COW ) 
            {
                panic("Page Fault - occured with Write to ReadOnly Paghe at va %x\n",addr);
                return ;
            }
        }

	// LAB 4: Your code here.

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
        //allocated a new pahe to copy the old page 
        //TODO - have to updated the permission from old Page table
        r = sys_page_alloc(0,PFTEMP,PTE_U|PTE_W|PTE_P);
        if( r < 0 ) 
        {
             panic("unable to allocated page");
        }

        //Now the copy the content from the old page to new 
        memmove((void*)PFTEMP,(void*)boundary_addr,PGSIZE);

        //Map the new page in old place 
        r=sys_page_map(0,(void*)PFTEMP,0,(void*)boundary_addr,PTE_P|PTE_U|PTE_W);

        if( r < 0 ) 
        {
             panic("unable to map page");
        }

        //unmap the pftemp page 
        sys_page_unmap(0,(void*)PFTEMP);

        if( r < 0 ) 
        {
             panic("unable to unmap page");
        }

	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
// 
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
        int i=0;
        int dir_index = 0 ;
        uint32_t perm = 0 ;

        //check if vpt[i] is marked as Write or cow, if so 
        //mark it as COW  for child and parent

   
         //cprintf("i %d vpt[i] %x \n",i,&vpt[i]); 
        //Make sure that this page is not the exception stack
       if ( pn * PGSIZE >= ( UXSTACKTOP - PGSIZE ) &&
            pn * PGSIZE <= ( UXSTACKTOP )
          )
          {
             //cprintf("Found a Exception stack at %d\n",pn);
              return 0;
          }
  
        if( ( vpt[pn] & PTE_P ) == PTE_P ) 
        {
             perm = vpt[pn] & 0xFFF; //Get last 12 bits

            if(( vpt[pn] & PTE_SHARED) == PTE_SHARED)
            {
                perm |= PTE_SHARED;
            }
            else if( ( ( vpt[pn] & PTE_W )   == PTE_W ) ||
                    ( ( vpt[pn] & PTE_COW)  == PTE_COW)
                  )
              {
                 perm = perm & ~PTE_W;
                 perm |= PTE_COW;
                //cprintf("Page %d has Write or cow\n",i);
              }
          

              //cprintf("Mapping the page for child %d\n",pn);
              //insert this page in child's PG DIR 
              r = sys_page_map(0,(void*)( pn * PGSIZE), envid,(void*)(pn*PGSIZE),perm | PTE_P);
              if( r < 0 ) 
              {
                   panic("Unable to map a page %d\n",pn);
              }

              //cprintf("Mapping the page for parent %d\n",pn);
              //update the Parent PGDIR to reflect new permission
             r = sys_page_map(0,(void*)( pn * PGSIZE), 0,(void*)(pn*PGSIZE),perm | PTE_P);

              if( r < 0 ) 
              {
                   panic("Unable to map a page %d\n",pn);
              }
        }

 
	// LAB 4: Your code here.
	//panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "env" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{

      int r;
      uint8_t* addr;
      extern char end[];

     //set up the page fault handler
     set_pgfault_handler(pgfault);

     //Create a New Environment for child by calling sys_exo_fork
     envid_t env_id ;
     env_id = sys_exofork();

    if( env_id == 0 )
    {
         //I am child - do something and get out
          //cprintf("Hiiii i I am child\n");
          env = &envs[ENVX(sys_getenvid())];
    }
    else if(env_id > 0)
    {
          //I am parent - Set the pagefault to child. 
          //set the exception stack for child
          //mark the pages as COW 

       //setup my pagefault handler for the child.
       if ((r = sys_page_alloc(env_id, (void*)(UXSTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
            panic("sys_page_alloc: %e", r);

         sys_env_set_pgfault_upcall(env_id,_pgfault_upcall);

        int dir_index = 0 ;
        int i = 0 ;
        int pn;
        for ( dir_index = 0 ; dir_index < PDX(UTOP) ; dir_index++ )
        {

           //check if vpd[dir-index] is present 
           if( vpd[dir_index] & PTE_P ) 
           {    
                for(i=0;i< NPTENTRIES;i++)
                {
                   pn =  dir_index * NPTENTRIES + i ;
                   duppage(env_id,pn );
                }
           }
        }

         //cprintf("[%08x] is made Runnable\n",env_id);

         // Start the child environment running
        if ((r = sys_env_set_status(env_id, ENV_RUNNABLE)) < 0)
                panic("sys_env_set_status: %e", r);


    }
    else
    {
          panic("Fork Failed\n");
          //return -EINVAL;
    }
    return env_id;
	// LAB 4: Your code here.
        //panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}

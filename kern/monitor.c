// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

/* 
 * =============================================
 *  Name                    Date        Changes
 * ==============================================
 *  Manikantan S/Ganesh      09/17/11     Lab2
 */



#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line

static int
free_page(int argc, char **argv, struct Trapframe *tf);
static int
alloc_page(int argc, char **argv, struct Trapframe *tf);
static int get_value(char * str );
static int 
mem_stat(int argc, char **argv, struct Trapframe *tf);
static int
page_status(int argc, char **argv, struct Trapframe *tf);
static int
showmappings(int argc, char **argv, struct Trapframe *tf);

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
        { "backtrace", "Display stack", mon_backtrace},
        { "alloc_page", "Allocates a page" ,alloc_page },
        { "page_status", "Get the page status",page_status},
        { "free_page", "Free the give page" ,free_page},
        { "mem_stat","Memory status",mem_stat},
        { "showmappings" , "Shows the Virtual to Physical mapping of range of addresses",showmappings},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start %08x (virt)  %08x (phys)\n", _start, _start - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		(end-_start+1023)/1024);
	return 0;
}

int get_value(char * str )
{
    int value =0 ;
    char * end;

    value = strtol(str,&end,16);

    return value;
}

int
mem_stat(int argc, char **argv, struct Trapframe *tf)
{
    uint32_t total_mem = ( npage * PGSIZE ) / 1024;
    uint32_t num_free_page = 0;
    int i;

    for(i=0;i<npage ;i++)
    {
      if ( pages[i].pp_ref == 0 )
        num_free_page++;
   }

    cprintf("total memory : %d KB ( %d Pages)\n",total_mem,npage);
    cprintf("Used Memory  : %d KB ( %d Pages)\n",(npage - num_free_page)*4,npage-num_free_page);
    cprintf("Free Memory  : %d KB ( %d Pages)\n",num_free_page*4,num_free_page);

    return 0;
}
int
page_status(int argc, char **argv, struct Trapframe *tf)
{
     uint32_t pa = get_value(argv[1]);

     //cprintf("pa is %x\n",pa);
     struct Page * pp = pa2page(pa);

     if( pp == NULL)
     {
          cprintf("Invalid address 0x%x\n",pa);
          return 0;
     }

     if ( pp->pp_ref == 0 ) 
     {
        cprintf("Page 0x%x is Free\n",pa);
     }
     else
     {
        cprintf("Page 0x%x is allocated\n",pa);
     }

     return 0;
}

int
free_page(int argc, char **argv, struct Trapframe *tf)
{
  if( argc > 1 )
  {
     uint32_t pa = get_value(argv[1]);
     struct Page * pp = pa2page(pa);

     if( pp != NULL)
     {
        if( pp->pp_ref == 0)
        {
           cprintf("Page 0x%x already free'd\n",pa);
        }
        else 
        { 
            pp->pp_ref--;

            if ( pp->pp_ref == 0 )
            {
                 page_free( pp );
            }
        }
     }
     else
     {
         cprintf("Invalid page address 0x%x\n",pa);
     }

  }
     return 0;
}    
    
int
alloc_page(int argc, char **argv, struct Trapframe *tf)
{
    //Call our page allocation functions to allocate a page.

    struct Page * ptr ;
    int result = page_alloc( &ptr );

    if(result == 0 )
    {
       cprintf("Allocated physical page %x\n",page2pa( ptr ) );
       ptr->pp_ref = 1;
    }
    else
    {
      cprintf("Allocation status : Failed - No Memory \n");
    }
    
    return 0;

}


int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
        //First Lets us read ebp
        int i=0;
        uint32_t addr =0 ;
        struct Eipdebuginfo info;
        int local_ebp = read_ebp();
        //int local_eip = read_eip();
 
#if 0

        //A BUNCH OF DEBUG INFORMATION HERE 
        
        //In the Lab, we are not printing the current frame
        int my_eip = read_eip();
        debuginfo_eip( my_eip, &info);

        int sec_eip = read_eip();
         int sec_esp = read_esp();
        debuginfo_eip( sec_eip, &info);
        cprintf("sec_ip %08x esp %08x\n",sec_eip,sec_esp);
        cprintf("       %s:%d: ",info.eip_file,info.eip_line);
        cprintf("%.*s\n",info.eip_fn_namelen);

        int sec1_eip = read_eip();
        int sec1_esp = read_esp();
        debuginfo_eip( sec1_eip, &info);
        cprintf("sec1_ip %08x esp %08x\n",sec1_eip,sec1_esp);
        cprintf("       %s:%d: ",info.eip_file,info.eip_line);
        cprintf("%.*s\n",info.eip_fn_namelen);
 
        cprintf("eip %08x  computed eip %08x , %s\n",my_eip,*((int*)local_ebp + 1),info.eip_fn_name);
        addr = my_eip;
        //Set the 
        cprintf("       %s:%d: ",info.eip_file,info.eip_line);
        cprintf("%.*s+%d\n",info.eip_fn_namelen,info.eip_fn_name,
         (int)(addr - info.eip_fn_addr));
#endif

        while ( local_ebp != 0x00 ) 
         {
              //cprintf("ebp %08x eip %08x",local_ebp,local_eip);
              cprintf("ebp %08x eip %08x",local_ebp,*((int*)local_ebp+1));
              //Now print the 5 words after the memory where "return" adress
              // is stored.
              cprintf("  args ");
              for ( i=0 ;i<5 ;i++)
              {
                 cprintf(" %08x", *((int*)local_ebp+i+2));
              }
              cprintf("\n");
              //Get the Debug information for the "return address" 
              // localebp + 1 points to the return address stored
              // by the caller
              debuginfo_eip( * ((int*)local_ebp + 1), &info);
              //debuginfo_eip( local_eip,&info);
               addr = * ((int*)local_ebp + 1);
              // addr = local_eip;

              cprintf("       %s:%d: ",info.eip_file,info.eip_line);
              cprintf("%.*s+%d\n",info.eip_fn_namelen,info.eip_fn_name,
                    (int)(addr - info.eip_fn_addr));
               //cprintf("--- %x %x ---\n",addr, info.eip_fn_addr);

             //updae the EBP here to unwind to previous frameA
             //To get the EBP of caller , go to the adress on stack
             // Pointed by EBP on callee
             local_ebp = *(int*)local_ebp;
             //local_eip = * ( (int *) local_ebp + 1);

         }
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

// return EIP of caller.
// does not work if inlined.
// putting at the end of the file seems to prevent inlining.
unsigned
read_eip()
{
	uint32_t callerpc;
	__asm __volatile("movl 4(%%ebp), %0" : "=r" (callerpc));
	return callerpc;
}


int
showmappings(int argc, char **argv, struct Trapframe *tf)
{

      int i;
      pte_t * ptr = NULL;
      uint32_t start,end;
      //cprintf("argc is %d boot_pgdir is %x\n",argc, boot_pgdir);

      if ( argc > 2 )
      {
        start = get_value(argv[1]);
        end   =  get_value(argv[2]);
      }
      else  if ( argc > 1 )
      {
           start = end = get_value( argv[1]);
      }

      if(argc > 1)
      {

         for(i=start ; i<=end; i+= PGSIZE)
         {
             ptr = pgdir_walk(boot_pgdir,(void *) i,0);
            if(!ptr)
            {
                cprintf("No mappings found for 0x%08x\n",i);
            }
           else
            {
               if( *ptr & PTE_P )
               {
                 
                 cprintf("VA 0x%08x mapped at PA  0x%08x : Perm 0x%03x\n",i,PTE_ADDR(*ptr),( *ptr & 0xFFF));
               }
               else
               {
                   cprintf("No mappings found for 0x%08x\n",i);
               }
           }
         }
     }

     return 0;
}

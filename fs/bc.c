
#include "fs.h"

// Return the virtual address of this disk block.
void*
diskaddr(uint32_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (vpd[PDX(va)] & PTE_P) && (vpt[VPN(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
     //Logically we should also check for VPD present bit
	return (vpt[VPN(va)] & PTE_D) != 0;
}

// Fault any disk block that is read or written in to memory by
// loading it from disk.
// Hint: Use ide_read and BLKSECTS.
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
        uint32_t sectorno;

        //cprintf("bc_pgflt invoked with fault addr %x \n",addr);

        //BLKSIZE and PGSIZE are same value 
        uint32_t block_aligned_addr = ROUNDDOWN((uint32_t)addr,BLKSIZE);
	int r;

	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_eip, addr, utf->utf_err);

	// Allocate a page in the disk map region and read the
	// contents of the block from the disk into that page.
	//
	// LAB 5: Your code here


         //Assumption : if the FS process write to a block , it is assumed 
         //that FS has made sure that a given ENV has write permission on the file
         //If the FS has read a block and faulted , then we need to allocate 
         //a page , map it at fault'ed address ( block aligned ) , copy 
         //the contents from har disk to given address.
         r = sys_page_alloc(0,(void*)block_aligned_addr,PTE_U|PTE_W|PTE_P);

         if(r<0)
         {
             panic("FS : Could not allocate a page to read a block from disk\n");
         }

         //Now that page ia allocated , read from disk and fill it 
         //ide_read can read upto to 256 sector - we need to fill a page
         //just read 8 sectors.
         sectorno = blockno * BLKSECTS;

         //read 8 sector ( 1 block)
        r = ide_read(sectorno,(void*)block_aligned_addr,BLKSECTS);

/* === A BUNCH OF DEBUG ==
        int ij=0;
        char c;
        char * array = (char *) block_aligned_addr;
        cprintf("MANI - PRINTING THE CONTENTS OF BLOCK %d\n",blockno); 
        cprintf("***********************************************\n");
        for(ij=0;ij<BLKSIZE;ij++)
        {  c= array[ij];
           cprintf("%d ", c);
        }
        cprintf("\n***********************************************\n");
*/

	// Sanity check the block number. (exercise for the reader:
	// why do we do this *after* reading the block in?)

        //If we try to check if a given block is free at the top ,
        //we will be using the bitmap address. however, if the 
        //currently pagefault is being handled for bitmap address
        //then we go-on recursive, exceed the exception stack frame
        //and then die

       //Same argument for super address also- if we put the blockno check 
       //at the begining of pgfault handler ( before ide_read) and if 
       //current handler was to read the block-1 ( super ) , then we are 
       //in trouble.
        
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Check that the block we read was allocated.
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_USER constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
        uint32_t block_aligned_addr = ROUNDDOWN((uint32_t)addr,BLKSIZE);
        uint32_t sectorno = 0;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

         sectorno = blockno * BLKSECTS;

         //check if we really need to commit the code to hard disk ?
        // See if the processor has set the DIRTY bit for the page 
        // at which the addr is mapped. if So , we have to commit the code 
        // to hard disk and reset the dirty bit . 
        uint32_t mapped = 0 ;
        uint32_t pn = (uint32_t)addr / PGSIZE;
        int r ;

        mapped = va_is_mapped( addr );
        if(mapped)
        {
            if( va_is_dirty( addr ) )
            {
                r = ide_write(sectorno,(const void*)block_aligned_addr,BLKSECTS);
                if ( r < 0 ) 
                {
                   panic("FS : Unable to Write to the block %d\n",blockno);
                }
                //reset the dirty flag
                r = sys_page_map(0,(void*) block_aligned_addr,0,(void*)block_aligned_addr,PTE_USER);
              
                if ( r < 0 ) 
                {
                   panic("FS : Unable to  clear the dirty bit\n");
                }
            }
       }

	// LAB 5: Your code here.
	//panic("flush_block not implemented");
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

        //cprintf("MANI : FS : about to acces block-1 - pgflt shd happen\n");
	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it 
        //cprintf("MANI : FS : about to acces block-1 - pgflt shd not  happen\n");
	strcpy(diskaddr(1), "OOPS!\n");
        //cprintf("MANI : FS : flushing the block-1 to HD \n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
        //cprintf("MANI : FS : page for block-1 is unmapped - so pgflt shd occur\n");
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{
	set_pgfault_handler(bc_pgfault);
	check_bc();
}


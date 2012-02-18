//-------
// Mani
//------
#include <inc/string.h>

#include "fs.h"

// --------------------------------------------------------------
// Super block
// --------------------------------------------------------------

// Validate the file system super-block.
void
check_super(void)
{
	if (super->s_magic != FS_MAGIC)
		panic("bad file system magic number");

	if (super->s_nblocks > DISKSIZE/BLKSIZE)
		panic("file system is too large");

	cprintf("superblock is good\n");
}

// --------------------------------------------------------------
// Free block bitmap
// --------------------------------------------------------------

// Check to see if the block bitmap indicates that block 'blockno' is free.
// Return 1 if the block is free, 0 if not.
bool
block_is_free(uint32_t blockno)
{
        //cprintf("MANI - block_is_free called with blockno %d\n",blockno);
	if (super == 0 || blockno >= super->s_nblocks)
		return 0;
	if (bitmap[blockno / 32] & (1 << (blockno % 32)))
		return 1;
	return 0;
}

// Mark a block free in the bitmap
void
free_block(uint32_t blockno)
{
	// Blockno zero is the null pointer of block numbers.
	if (blockno == 0)
		panic("attempt to free zero block");
	bitmap[blockno/32] |= 1<<(blockno%32);
}

int 
set_block_allocated(uint32_t blockno) 
{
   //TODO : is it ok to keep as uint32_t
   uint32_t mask =  1 << (blockno%32) ;
   //cprintf("MANI : blockno %d before %x \n",blockno, bitmap[blockno/32]);
   bitmap[blockno/32] &= ~mask;

  //Now that we have written to bitmap - make sure to flush it 
   flush_block((void*)&bitmap[blockno/32]);
   //cprintf("MANI : blockno %d aftyer %x \n",blockno, bitmap[blockno/32]);

   return 0;
}
// Search the bitmap for a free block and allocate it.  When you
// allocate a block, immediately flush the changed bitmap block
// to disk.
// 
// Return block number allocated on success,
// -E_NO_DISK if we are out of blocks.
//
// Hint: use free_block as an example for manipulating the bitmap.
int
alloc_block(void)
{
	// The bitmap consists of one or more blocks.  A single bitmap block
	// contains the in-use bits for BLKBITSIZE blocks.  There are
	// super->s_nblocks blocks in the disk altogether.

        uint32_t blockno=0;
        for(;blockno<super->s_nblocks;blockno++)
        {
            
            if( block_is_free(blockno) )
            {
                //we found a free block @ blockno. Mark it as 
                //used before returning and flush it to HD
                set_block_allocated(blockno);
                return( blockno ) ;
            }
            //cprintf("MANI - blockno %d nore free \n",blockno);
        }
	// LAB 5: Your code here.
	//panic("alloc_block not implemented");
	return -E_NO_DISK;
}

// Validate the file system bitmap.
//
// Check that all reserved blocks -- 0, 1, and the bitmap blocks themselves --
// are all marked as in-use.
void
check_bitmap(void)
{
	uint32_t i;

         //cprintf("s_nblocks is %d\n", super->s_nblocks);
	// Make sure all bitmap blocks are marked in-use
	for (i = 0; i * BLKBITSIZE < super->s_nblocks; i++)
		assert(!block_is_free(2+i));

	// Make sure the reserved and root blocks are marked in-use.
	assert(!block_is_free(0));
	assert(!block_is_free(1));

	cprintf("bitmap is good\n");
}

// --------------------------------------------------------------
// File system structures
// --------------------------------------------------------------

// Initialize the file system
void
fs_init(void)
{
	static_assert(sizeof(struct File) == 256);

	// Find a JOS disk.  Use the second IDE disk (number 1) if available.
	if (ide_probe_disk1())
		ide_set_disk(1);
	else
		ide_set_disk(0);
	
	bc_init();

	// Set "super" to point to the super block.
	super = diskaddr(1);
	// Set "bitmap" to the beginning of the first bitmap block.
	bitmap = diskaddr(2);

	check_super();
	check_bitmap();
}

// Find the disk block number slot for the 'filebno'th block in file 'f'.
// Set '*ppdiskbno' to point to that slot.
// The slot will be one of the f->f_direct[] entries,
// or an entry in the indirect block.
// When 'alloc' is set, this function will allocate an indirect block
// if necessary.
//
// Returns:
//	0 on success (but note that *ppdiskbno might equal 0).
//	-E_NOT_FOUND if the function needed to allocate an indirect block, but
//		alloc was 0.
//	-E_NO_DISK if there's no space on the disk for an indirect block.
//	-E_INVAL if filebno is out of range (it's >= NDIRECT + NINDIRECT).
//
// Analogy: This is like pgdir_walk for files.  
// Hint: Don't forget to clear any block you allocate.
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
	// LAB 5: Your code here.

         uint32_t * array_ptr;
        //Given the Nth block in a file, we are required to provide
        //the corresponding physical block number back.
        //if the filbeno < 10 , then , it should be in f_direct array
        //otherwise , we have to check in the indirect block.

        //if the alloc is provided as 1, we will be required to 
        //allocate one indirect block.

        //TODO : Assumption - I think, if a file doesnot have 
        //a block then , the corresponding blockno should be marked as 
        // 0 in the direct_array.

        //First set the ppdiskbno to NULL.
        if( ppdiskbno ) 
         *ppdiskbno = NULL;

        if ( filebno >= NDIRECT + NINDIRECT)
        {
            return ( -E_INVAL ) ;
        }

        if ( filebno < NDIRECT )
        {
           //check if a Physical block has been allocaed ? 
          //cprintf("MANI  : Sending the pysucal block ar %d\n",f->f_direct[filebno]);
          if( ppdiskbno )
          {
               *ppdiskbno = &f->f_direct[filebno];
          }
           return 0;
        }
        else
        {
              //check if indirect block exists ? 
              if(f->f_indirect !=0 ) 
              {
                 //Get the virtual address of this block
                 array_ptr =  diskaddr( f->f_indirect );
            
                 //Run through this block and get the filebno-10 th block.
                 //we have a block number for the requested Nth block.
                  if( ppdiskbno )
                  {
                      *ppdiskbno = &array_ptr[filebno - NDIRECT];
                  }
                 return 0;
                 
              }
              else
              {
                     //User requested Nth block should be inside of indirect block
                     //However, we dont have the indirect block - 
                     //CHeck if we need to allocate it .
                     if( alloc != 0 )
                     {
                         //Allocate a block  to represent indirect_block and reqturn it 
                          uint32_t new_block;
                          new_block = alloc_block();
                          if( new_block == -E_NO_DISK)
                          {
                              return( -E_NO_DISK);
                          }
                          else
                          {
                              //a new block is allocated
                              f->f_indirect = new_block;

                             //Get the virtual address of this block
                             array_ptr =  diskaddr(  f->f_indirect );
                             
                             //initialise the new block with 0x00
                             memset(array_ptr,0x00,BLKSIZE);
                             flush_block(array_ptr);

                             if( ppdiskbno )
                             {
                                  *ppdiskbno = &array_ptr[filebno - NDIRECT];
                             }

                             //We just updated the block containing f->f_indirect.
                             //flush it to HD              
                             flush_block( &f->f_indirect);
                             return ( 0 );
                          }
                     }
                     else
                     {
                         return ( - E_NOT_FOUND );
                     }
              }
        }

	//panic("file_block_walk not implemented");
}

// Set *blk to point at the filebno'th block in file 'f'.
// Allocate the block if it doesn't yet exist.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_NO_DISK if a block needed to be allocated but the disk is full.
//	-E_INVAL if filebno is out of range.
//
// Hint: Use file_block_walk and alloc_block.
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
	// LAB 5: Your code here.
        if( filebno >= NDIRECT + NINDIRECT)
        {
               return ( -E_INVAL ) ;
        }

        //Given the Nth block no , find the physical block numnber
        uint32_t * ptr;
        int r ;
        char * addr;
        r = file_block_walk( f,filebno,&ptr,1);
        
        if( r == 0 ) 
        {          
           //There is a Physical block present for the requested Nth block
           //Find the address, fill it and send it 
           if( *ptr )
           {
               addr = (void* ) diskaddr( *ptr );
               if(blk)
                 *blk = (char*)addr;
               return 0;
           }
           else
           {
              //I requested for the Nth block , however, there was no physical block backing it 
              //Now allocate a block and update my file meta-data to point to this block
              int new_block = alloc_block();
              if( new_block > 0 ) 
              {
                   //Got a new block.
                   *ptr = new_block;
                    
                   addr =  diskaddr( *ptr );

                   //initialise the new block with 0x00
                   memset(addr,0x00,BLKSIZE);
                   flush_block(addr);

                   if(blk)
                     *blk = (char*)addr;

                     //We just wrote to the directory entry - so flush it 
                     //worstcase TODO : Flush the whole directory
                    flush_block( ptr );
                    return 0;
              }

              //Allocating the new-block failed - return error
              return ( -E_NO_DISK);
           }
        }
        else if ( r == -E_NO_DISK )
        {
              cprintf("DISK IS FULL\n");
              return r;
        }
        else if ( r == -E_NOT_FOUND)
        {
              return ( r);
        }

        
         return(-E_NO_DISK);
	//panic("file_get_block not implemented");
}

// Try to find a file named "name" in dir.  If so, set *file to it.
//
// Returns 0 and sets *file on success, < 0 on error.  Errors are:
//	-E_NOT_FOUND if the file is not found
static int
dir_lookup(struct File *dir, const char *name, struct File **file)
{
	int r;
	uint32_t i, j, nblock;
	char *blk;
	struct File *f;

	// Search dir for name.
	// We maintain the invariant that the size of a directory-file
	// is always a multiple of the file system's block size.
	assert((dir->f_size % BLKSIZE) == 0);
	nblock = dir->f_size / BLKSIZE;
	for (i = 0; i < nblock; i++) {
		if ((r = file_get_block(dir, i, &blk)) < 0)
			return r;
		f = (struct File*) blk;
		for (j = 0; j < BLKFILES; j++)
			if (strcmp(f[j].f_name, name) == 0) {
				*file = &f[j];
				return 0;
			}
	}
	return -E_NOT_FOUND;
}

// Set *file to point at a free File structure in dir.  The caller is
// responsible for filling in the File fields.
static int
dir_alloc_file(struct File *dir, struct File **file)
{
	int r;
	uint32_t nblock, i, j;
	char *blk;
	struct File *f;

	assert((dir->f_size % BLKSIZE) == 0);
	nblock = dir->f_size / BLKSIZE;
	for (i = 0; i < nblock; i++) {
		if ((r = file_get_block(dir, i, &blk)) < 0)
			return r;
		f = (struct File*) blk;
		for (j = 0; j < BLKFILES; j++)
			if (f[j].f_name[0] == '\0') {
				*file = &f[j];
				return 0;
			}
	}
	dir->f_size += BLKSIZE;
	if ((r = file_get_block(dir, i, &blk)) < 0)
		return r;
	f = (struct File*) blk;
	*file = &f[0];
	return 0;
}

// Skip over slashes.
static const char*
skip_slash(const char *p)
{
	while (*p == '/')
		p++;
	return p;
}

// Evaluate a path name, starting at the root.
// On success, set *pf to the file we found
// and set *pdir to the directory the file is in.
// If we cannot find the file but find the directory
// it should be in, set *pdir and copy the final path
// element into lastelem.
static int
walk_path(const char *path, struct File **pdir, struct File **pf, char *lastelem)
{
	const char *p;
	char name[MAXNAMELEN];
	struct File *dir, *f;
	int r;

	// if (*path != '/')
	//	return -E_BAD_PATH;
	path = skip_slash(path);
	f = &super->s_root;
	dir = 0;
	name[0] = 0;

	if (pdir)
		*pdir = 0;
	*pf = 0;
	while (*path != '\0') {
		dir = f;
		p = path;
		while (*path != '/' && *path != '\0')
			path++;
		if (path - p >= MAXNAMELEN)
			return -E_BAD_PATH;
		memmove(name, p, path - p);
		name[path - p] = '\0';
		path = skip_slash(path);

		if (dir->f_type != FTYPE_DIR)
			return -E_NOT_FOUND;

		if ((r = dir_lookup(dir, name, &f)) < 0) {
			if (r == -E_NOT_FOUND && *path == '\0') {
				if (pdir)
					*pdir = dir;
				if (lastelem)
					strcpy(lastelem, name);
				*pf = 0;
			}
			return r;
		}
	}

	if (pdir)
		*pdir = dir;
	*pf = f;
	return 0;
}

// --------------------------------------------------------------
// File operations
// --------------------------------------------------------------

// Create "path".  On success set *pf to point at the file and return 0.
// On error return < 0.
int
file_create(const char *path, struct File **pf)
{
	char name[MAXNAMELEN];
	int r;
	struct File *dir, *f;

	if ((r = walk_path(path, &dir, &f, name)) == 0)
		return -E_FILE_EXISTS;
	if (r != -E_NOT_FOUND || dir == 0)
		return r;
	if (dir_alloc_file(dir, &f) < 0)
		return r;
	strcpy(f->f_name, name);
	*pf = f;
	file_flush(dir);   //Is this Error ? should it not be &f->f_name
	return 0;
}

// Open "path".  On success set *pf to point at the file and return 0.
// On error return < 0.
int
file_open(const char *path, struct File **pf)
{
	return walk_path(path, 0, pf, 0);
}

// Read count bytes from f into buf, starting from seek position
// offset.  This meant to mimic the standard pread function.
// Returns the number of bytes read, < 0 on error.
ssize_t
file_read(struct File *f, void *buf, size_t count, off_t offset)
{
	int r, bn;
	off_t pos;
	char *blk;

	if (offset >= f->f_size)
		return 0;

	count = MIN(count, f->f_size - offset);

	for (pos = offset; pos < offset + count; ) {
		if ((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
			return r;
		bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
		memmove(buf, blk + pos % BLKSIZE, bn);
		pos += bn;
		buf += bn;
	}

       //cprintf("MANI file_read count %d\n",count);
	return count;
}

// Write count bytes from buf into f, starting at seek position
// offset.  This is meant to mimic the standard pwrite function.
// Extends the file if necessary.
// Returns the number of bytes written, < 0 on error.
int
file_write(struct File *f, const void *buf, size_t count, off_t offset)
{
	int r, bn;
	off_t pos;
	char *blk;

	// Extend file if necessary
	if (offset + count > f->f_size)
		if ((r = file_set_size(f, offset + count)) < 0)
			return r;

	for (pos = offset; pos < offset + count; ) {
		if ((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
			return r;
		bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
		memmove(blk + pos % BLKSIZE, buf, bn);
		pos += bn;
		buf += bn;
	}

	return count;
}

// Remove a block from file f.  If it's not there, just silently succeed.
// Returns 0 on success, < 0 on error.
static int
file_free_block(struct File *f, uint32_t filebno)
{
	int r;
	uint32_t *ptr;

	if ((r = file_block_walk(f, filebno, &ptr, 0)) < 0)
		return r;
	if (*ptr) {
		free_block(*ptr);
		*ptr = 0;
	}
	return 0;
}

// Remove any blocks currently used by file 'f',
// but not necessary for a file of size 'newsize'.
// For both the old and new sizes, figure out the number of blocks required,
// and then clear the blocks from new_nblocks to old_nblocks.
// If the new_nblocks is no more than NDIRECT, and the indirect block has
// been allocated (f->f_indirect != 0), then free the indirect block too.
// (Remember to clear the f->f_indirect pointer so you'll know
// whether it's valid!)
// Do not change f->f_size.
static void
file_truncate_blocks(struct File *f, off_t newsize)
{
	int r;
	uint32_t bno, old_nblocks, new_nblocks;

	old_nblocks = (f->f_size + BLKSIZE - 1) / BLKSIZE;
	new_nblocks = (newsize + BLKSIZE - 1) / BLKSIZE;
	for (bno = new_nblocks; bno < old_nblocks; bno++)
		if ((r = file_free_block(f, bno)) < 0)
			cprintf("warning: file_free_block: %e", r);

	if (new_nblocks <= NDIRECT && f->f_indirect) {
		free_block(f->f_indirect);
		f->f_indirect = 0;
	}
}

// Set the size of file f, truncating or extending as necessary.
int
file_set_size(struct File *f, off_t newsize)
{
	if (f->f_size > newsize)
		file_truncate_blocks(f, newsize);
	f->f_size = newsize;
	flush_block(f);
	return 0;
}

// Flush the contents and metadata of file f out to disk.
// Loop over all the blocks in file.
// Translate the file block number into a disk block number
// and then check whether that disk block is dirty.  If so, write it out.
void
file_flush(struct File *f)
{
	int i;
	uint32_t *pdiskbno;

	for (i = 0; i < (f->f_size + BLKSIZE - 1) / BLKSIZE; i++) {
		if (file_block_walk(f, i, &pdiskbno, 0) < 0 ||
		    pdiskbno == NULL || *pdiskbno == 0)
			continue;
		flush_block(diskaddr(*pdiskbno));
	}
	flush_block(f);
	if (f->f_indirect)
		flush_block(diskaddr(f->f_indirect));
}

// Remove a file by truncating it and then zeroing the name.
int
file_remove(const char *path)
{
	int r;
	struct File *f;

	if ((r = walk_path(path, 0, &f, 0)) < 0)
		return r;

	file_truncate_blocks(f, 0);
	f->f_name[0] = '\0';
	f->f_size = 0;
	flush_block(f);

	return 0;
}

// Sync the entire file system.  A big hammer.
void
fs_sync(void)
{
	int i;
	for (i = 1; i < super->s_nblocks; i++)
		flush_block(diskaddr(i));
}


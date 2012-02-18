#include <inc/lib.h>
#include <inc/elf.h>
#include <inc/types.h>


int read_file_into_memory(char * filename );
void read_and_print_section_headers(void * file);
void read_and_print_relocation_section(void * file, struct Secthdr * shptr);
void read_and_print_symbol_table(void * file, struct Secthdr * shptr);
char * get_symbol_name_ptr(void * file , int index,int strtable_index );

void
umain(int argc, char **argv)
{
	int i, r, x, want,fd;
        char * file;
        unsigned char elf_buf[512];
        struct Elf *elf;
        void * fileaddr;


	cprintf("process_module started\n");

        if ((r = open("testmodule.o", O_RDONLY)) < 0)
                return ;
        fd = r;


        // Read elf header
        elf = (struct Elf*) elf_buf;
        if (read(fd, elf_buf, sizeof(elf_buf)) != sizeof(elf_buf)
            || elf->e_magic != ELF_MAGIC ) {
                close(fd);
                cprintf("elf magic %08x want %08x\n", elf->e_magic, ELF_MAGIC);
                return ;
        }


        close(fd);

       //open the file testmodule.o and load it into buffer for processing
       r = read_file_into_memory("testmodule.o");
       if( r < 0) 
       {
          cprintf("Could not open file testmodule.o\n");
          return ;
       }

      //Now that file is read @ given address r . 
      file = ( void * ) r ;

       sys_load_kernel_module(UTEMP,PGSIZE);
      read_and_print_section_headers(file);
}

void read_and_print_section_headers(void * file)
{
       struct Elf *elf;
       elf = (struct Elf * ) file;
       struct Secthdr * shptr;
       int i;
       uint32_t strtab;


      cprintf("File Information - ELF Header \n");
      cprintf("FIle Magic %x e_shstrndx %x \n",elf->e_magic,elf->e_shstrndx);
      cprintf("FIle type %d entry %x phoff %d shoff %x phnum %d shnum %d\n",elf->e_type,elf->e_entry,elf->e_phoff,elf->e_shoff,elf->e_phnum,elf->e_shnum);

      shptr = ( struct Secthdr *  ) ( (char * ) file + elf->e_shoff) ;

      //print Info on each section .
      for (i=0;i<elf->e_shnum;i++)
      {
          cprintf("Section : %d name %d type %d , flags %d , addr %x ,offset %x ,size %x info %d align %x , entsize %d \n",i,shptr[i].sh_name,shptr[i].sh_type,shptr[i].sh_flags,shptr[i].sh_addr,shptr[i].sh_offset,shptr[i].sh_size,shptr[i].sh_info,shptr[i].sh_addralign,shptr[i].sh_entsize);
      }
      read_and_print_relocation_section(file,&shptr[2]);
      //read_and_print_symtab_section(file);
      read_and_print_symbol_table(file,&shptr[13]);

      return ;
}
void read_and_print_symbol_table(void * file, struct Secthdr * shptr)
{
     int i=0;
     int chars_printed=0;
     char * index = (char*) file + shptr->sh_offset;

     cprintf("offset %x size %d\n",shptr->sh_offset,shptr->sh_size);

     while(chars_printed < shptr-> sh_size)
     {
           if(index[chars_printed] == '\0')
           {
               i++;
               cprintf("%c",' ');
           }
         
           cprintf("%c",index[chars_printed]);
           chars_printed++;
     }
 
     cprintf("\nNumber of chars %d\n",i);
     return;

     while(chars_printed < shptr-> sh_size)
     {
          cprintf("Num %d \t ",i++);
          cprintf("str %s \t",index);
          chars_printed+= strlen(index);
          index+= chars_printed;
     }
#if 0
     while(chars_printed < shptr-> sh_size)
     {
         if(index[chars_printed] == '\0')
         cprintf("%c",' ');
         else
         cprintf("%c",index[chars_printed]);
        
          chars_printed++;
     }
#endif

    
}

void read_and_print_relocation_section(void * file, struct Secthdr * shptr)
{
       uint32_t num_entries ;
       int i=0;
       struct relhdr *relptr;
       struct symtab *symptr;
       struct Secthdr * shptr_array;
       char * symbol_name_ptr;

       int type,symbol,sym_info ;
       uint32_t link = shptr->sh_link; //This Section gives the symbols information for current relocation section.
       struct Elf *elf;
       elf = (struct Elf * ) file;

       cprintf("Type is %d \n",shptr->sh_type);

      shptr_array = ( struct Secthdr *  ) ( (char * ) file + elf->e_shoff) ;

       num_entries = shptr->sh_size/ shptr->sh_entsize ;
       symptr = (struct symtab *) ( (char*) file + shptr_array[link].sh_offset);
       relptr = (struct relhdr  * ) ( (char * ) file + shptr->sh_offset) ;

       cprintf("SYMTAB5 name %d\n",symptr[5].st_name);
       cprintf("SYMTAB5 value %d\n",symptr[5].st_value);
       cprintf("SYMTAB5 sizre %d\n",symptr[5].st_size);
       cprintf("SYMTAB5 info %d\n",symptr[5].st_info);
       cprintf("SYMTAB5 other %d\n",symptr[5].st_other);
       cprintf("SYMTAB5 shnindx %d\n",symptr[5].st_shndx);
       cprintf("SYMTAB5 shnindxforcprintf  %d\n",symptr[11].st_shndx);
       for(i=0;i<num_entries;i++)
       {
            symbol = ELF32_R_SYM(relptr[i].rel_info);
            type = ELF32_R_TYPE(relptr[i].rel_info);
            sym_info = ELF32_R_INFO(symbol,type);

          //find out the actual symbol name and its vaue recorded symbol table 
          if( symptr[symbol].st_name != 0 ) 
          {
            //get the symbol name from strtab @ 13 - Hardcoded 
            symbol_name_ptr = get_symbol_name_ptr(file,symptr[symbol].st_name,13); //hardcode
           }
           else
           { 
               //Check the shndx of this symtab entry
               int index2strtab = shptr_array[symptr[symbol].st_shndx].sh_name;
            symbol_name_ptr = get_symbol_name_ptr(file,index2strtab,11); //hardcode
           }
           cprintf("REL : %d , rel_offset %x , ym index %d type %d info %x  value %x symbol name : %s\n",i,relptr[i].rel_offset,symbol,type,sym_info,symptr[symbol].st_value,symbol_name_ptr );

       }
}

char * get_symbol_name_ptr(void * file , int index,int strtable_index )
{
       struct Elf *elf;
       struct Secthdr * shptr_array;
       char * ptr;
       static int debug=0;

       elf = (struct Elf * ) file;

      //find where is the string in table ? 
      shptr_array = ( struct Secthdr *  ) ( (char * ) file + elf->e_shoff) ;
      ptr = (char * ) file + shptr_array[strtable_index].sh_offset;

      cprintf("MANI - index %d str %s\n",index,ptr+index);
      return ( ptr+index);
}

int read_file_into_memory(char * filename )
{
       //open the given file.
       int r = 0;
       struct Stat filestat;
       uint32_t file_size;
       int fdnum;
       int npages;
       int bytes_to_read = 0,i=0;
       void * addr;

       fdnum = open(filename ,O_RDONLY);

       if(fdnum<0 ) 
       return fdnum;

       r=fstat(fdnum,&filestat);

       if(r<0) return r ;

      file_size = filestat.st_size;

      npages = file_size/PGSIZE + 1 ;

      //how many pages do we need to read this file into memory
      cprintf("Mani file size %d pages needed %d\n",file_size,npages);

      //load the file at UTEMP
      i =0;
      addr = (char * ) UTEMP;
      bytes_to_read = file_size;
      while(bytes_to_read > 0 ) 
      {
            cprintf("Reading page %d\n",i++);

            //Allocate a page . Map it at addr.
           if ((r = sys_page_alloc(0, (void*) addr, PTE_P|PTE_U|PTE_W)) < 0)
                return r;

           //read the contents from file , one page at a time .
           if ((r = read(fdnum, addr, MIN(PGSIZE,bytes_to_read ))) < 0)
              return r;
 
           bytes_to_read -= r;
           addr += r;
      }
 
       cprintf("File read done\n");
       return (int)UTEMP;
}

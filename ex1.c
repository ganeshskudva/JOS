//
// Simple inline assembly example
//
// For JOS lab 1 exercise 1
//

#include <stdio.h>

int
main(int argc, char **argv)
{
  int x = 1;
  printf("Hello x = %d\n", x);

  //
  // Put in-line assembly here to increment
  // the value of x by 1 using in-line assembly
  //
#if 0
        asm ("pushl %%eax;"
             "movl %0,%%eax;"
             "inc %%eax;"
             "movl %%eax,%0;"
             "popl %%eax;"
             :"=r"(x)        /* output */
             :"r"(x)         /* input */
             :"%eax"         /* clobbered register */
             );       
#endif 
        asm ("inc %0;"
             :"=r"(x)        
             :"r"(x)        
             );       

  printf("Hello x = %d after increment\n", x);

  if(x == 2){
    printf("OK\n");
  }
  else{
    printf("ERROR\n");
  }
}

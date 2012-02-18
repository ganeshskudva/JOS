#include<inc/stdio.h>

int module_init(int a)
{
      //ucprintf();
      cprintf("Hello this is Module entry\n %d ",a);
      cprintf("Hello this is Module entry\n %d ",a);
      //cprintf("Yest another message\n");
      return 0;
}

int module_exit(int a)
{
      return 0;
}

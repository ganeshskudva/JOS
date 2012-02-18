#include "ns.h"

extern union Nsipc nsipcbuf;

int try_and_get_packet(void *pg,uint32_t len);

void
sleep(int sec)
{
	unsigned now = sys_time_msec();
	unsigned end = now + sec * 1000;

	if ((int)now < 0 && (int)now >= -MAXERROR)
		panic("sys_time_msec: %e", (int)now);
	if (end < now)
		panic("sleep: wrap");

	while (sys_time_msec() < end)
		sys_yield();
}

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.

       //Allocate a page and map it at &nsipcbuf - we can use 2 pages

       while ( 1 ) 
       {
             int r = sys_page_alloc(0, &nsipcbuf, PTE_P|PTE_U|PTE_W);

             if(r < 0 ) 
             panic("Could not allocate Page\n");

            //do a system call and see if you get some result
            try_and_get_packet(&nsipcbuf,PGSIZE);

             //cprintf("send the packet t0 the user %x \n",&nsipcbuf);
            //ok, now i got a packet - send it up
            ipc_send(ns_envid,NSREQ_INPUT,&nsipcbuf,PTE_P|PTE_U|PTE_W);
             //cprintf("IPC Send completd\n");
           
             sys_page_unmap(0,&nsipcbuf);
 
            //temp hack 
            //sleep(1); //Need to ensure that the Reciever has unmaped the pahgge 
       }
}

int try_and_get_packet(void *pg,uint32_t len)
{
       int32_t result ;
       while(1)
       {
             result = sys_rcv_network_packet(len,pg);

             if(result >= 0 ) 
             {
                  //cprintf("MANI - Got a Packet on length %x - sent it up\n",result);
                  return result;
             }
             else
             {
                //cprintf("M");
                sys_yield();
             }
       }

       return -1;
}

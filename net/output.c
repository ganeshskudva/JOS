#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";
        
        envid_t from;
        void *va;
        int result ;
        int perm;

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
        while(1)
        {
             result = ipc_recv((int32_t*)&from,&nsipcbuf,&perm);

            //check who sent a message to you and what request ? 
            if( ( result == NSREQ_OUTPUT ) && ( from == ns_envid ) )
            {
                 //re-interpret the address,where page is mapped, as nsipcbuf
                 struct jif_pkt *ptr = (struct jif_pkt*) &nsipcbuf;

                 //invoke the driver system call to send the data 
                 result = sys_send_network_packet(ptr->jp_len,&ptr->jp_data[0]);

                sys_page_unmap(0,&nsipcbuf);
            }
        }
}

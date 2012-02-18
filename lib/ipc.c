// User-level IPC library routines

#include <inc/lib.h>

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
//	that address.
// If 'from_env_store' is nonnull, then store the IPC sender's envid in
//	*from_env_store.
// If 'perm_store' is nonnull, then store the IPC sender's page permission
//	in *perm_store (this is nonzero iff a page was successfully
//	transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
//	they're nonnull) and return the error.
// Otherwise, return the value sent by the sender
//
// Hint:
//   Use 'env' to discover the value and who sent it.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value, since that's
//   a perfectly valid place to map a page.)
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
     int32_t result = 0;
     void * addr_to_map = 0x00;
     int32_t value_rcvd = 0x00;

     //cprintf("a Process called ipc_recv with %x \n",pg);
     if( pg != NULL )
     {
         addr_to_map = (void * ) pg;
         //cprintf("since pg is not null , set the addr_to_map as %x\n",addr_to_map);
     }
     else
     {
        addr_to_map = (void * ) 0xFFFFFFFF;  
         //cprintf("since pg is null , set the addr_to_map as %x\n",addr_to_map);
     }

     result = sys_ipc_recv(addr_to_map);

     //cprintf("MANI : USERCALL : IPC RECV result %d\n",result);  
     //we just got a new ipc message . Fill the Information from env
     if( from_env_store != NULL ) 
     {
        if( result == 0 ) 
        {
           //cprintf("MANI : USERCALL : IPC RECV from %x\n",env->env_ipc_from);
             *from_env_store = env->env_ipc_from;
        }
        else
        {
             *from_env_store = 0;
        }
     } 

     if( perm_store != NULL ) 
     {
         if(result == 0 )
         {
             *perm_store = env->env_ipc_perm;
         }
         else
         {
             *perm_store = 0;
         }
     }

     if( result == 0 ) 
     {
        value_rcvd = env->env_ipc_value;
     }

	// LAB 4: Your code here.
	//panic("ipc_recv not implemented");
	return value_rcvd;
}

// Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// It should panic() on any error other than -E_IPC_NOT_RECV.
//
// Hint:
//   Use sys_yield() to be CPU-friendly.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
     //TODO : Sanity check
     int result = 0;
     void * addr_to_map;
  
     if( pg == NULL )
     {
          addr_to_map = (void*) 0xFFFFFFFF;
     }
     else
     {
          addr_to_map = pg;
     }

     //cprintf("MANI USER SEND IPC addr %x\n",addr_to_map);
     //cprintf("MANI : USER SEND IPC values to : %x , val %x , addr %x ,perm %x\n",to_env,val,pg,perm);
     do
     {
         //cprintf("trying to send again\n");
     //cprintf("MANI : USER SEND IPC values to : %x , val %x , addr %x ,perm %x\n",to_env,val,pg,perm);
         result = sys_ipc_try_send(to_env,val,addr_to_map,perm);
         
         //cprintf("sending ipc resulted in %d\n",result);

         if( ( result != 0 ) && 
             ( result != -E_IPC_NOT_RECV)
           )
           {
               panic("Unable to send IPC \n");
           }
           else if ( result != 0 ) 
           {
                 //Give up the CPU - we will come back again 
                 sys_yield();
           }  

     } while ( result != 0 ) ;
    
        
      return ;
	// LAB 4: Your code here.
	//panic("ipc_send not implemented");
}

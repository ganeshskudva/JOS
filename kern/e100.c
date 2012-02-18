// LAB 6: Your driver code here
#include "kern/e100.h"
#include "inc/string.h"
#include "kern/pmap.h"
#include "kern/picirq.h"
#include "kern/pci.h"
#include "inc/x86.h"
#include "inc/ns.h"

//way too many Global variables .

uint32_t e100_tx_buff ;     //The VA address of Tx Base will be stored in this variable
uint32_t e100_rx_buff ;     //The VA address of Rx Base will be stored in this variable

struct tx_info e100_tx_info;     //This structure is used for book-keeping Tx DMA buffers.
struct rx_info e100_rx_info;     //This structure is used for book-keeping Rx DMA buffers.
struct command_block e100_cb;    
struct pci_func pci_e100_info;  //This Structure holds the PCI properties of E100 NIC 

static void delay(int n)
{
    int i=0;

    for(i=0;i<n;i++)
    inb(0x84);
}
static void
hexdump(const char *prefix, const void *data, int len)
{
	int i;
	char buf[80];
	char *end = buf + sizeof(buf);
	char *out = NULL;
	for (i = 0; i < len; i++) {
		if (i % 16 == 0)
			out = buf + snprintf(buf, end - buf,
					     "%s%04x   ", prefix, i);
		out += snprintf(out, end - out, "%02x", ((uint8_t*)data)[i]);
		if (i % 16 == 15 || i == len - 1)
			cprintf("%.*s\n", out - buf, buf);
		if (i % 2 == 1)
			*(out++) = ' ';
		if (i % 16 == 7)
			*(out++) = ' ';
	}
}


//Allocates contiguous set of pages for Tx DMA and links them
void e100_initialise(void )
{
      //First take care of init'ing the TX Buffer information.
     struct command_block * ptrcb;
     struct rfd * ptrrfd;
     int i=0;
     uint32_t start_addr_of_page ;
     uint16_t suspend_on;

     //cprintf("MANI - e100 init\n");
     memset((void*)&e100_tx_info,0x00,sizeof(struct tx_info));
     e100_tx_info.tx_buff_va = e100_tx_buff;
     e100_tx_info.tx_buff_pa = PADDR ( e100_tx_info.tx_buff_va );
     e100_tx_info.free_count = E100_MAX_TX_BUFFERS;
     e100_tx_info.end = e100_tx_info.current_block = 0;
    

   //Now do the Linking for all the Tx Pages.
   //One Page for one CB is too Much , but then .... deadline  for Lab6 ...

   for(i=0;i<E100_MAX_TX_BUFFERS - 1 ;i++ )
   {
      start_addr_of_page = e100_tx_buff + i * PGSIZE;
      ptrcb = ( struct command_block * ) start_addr_of_page;

      //For each of the block - set the suspend bit.
      //after execution of any command, the CU goes to suspend mode
      //and the Driver will then clear the S bit and send a resume 
      suspend_on = ( 1 << 14 );
      ptrcb->cmd = suspend_on;

     //Set the link to point to next page
     ptrcb->link = PADDR( start_addr_of_page + PGSIZE ) ; 
   }

    //special handling for the last block
    start_addr_of_page = e100_tx_buff + ( E100_MAX_TX_BUFFERS - 1 ) * PGSIZE;
    ptrcb = ( struct command_block * ) start_addr_of_page;
    suspend_on = ( 1 << 14 );
    ptrcb->cmd = suspend_on;
    ptrcb->link =  e100_tx_info.tx_buff_pa ;

    //Linking of the command blocks are done - now Load the CU base address in E100

     //cprintf("MANI - e100_rx_buff %x\n",e100_rx_buff);

     memset((void*)&e100_rx_info,0x00,sizeof(struct rx_info));
     e100_rx_info.tx_buff_va = e100_rx_buff;
     e100_rx_info.tx_buff_pa = PADDR ( e100_rx_info.tx_buff_va );
     e100_rx_info.free_count = E100_MAX_RX_BUFFERS;
     e100_rx_info.served = e100_rx_info.current_block = 0;
     e100_rx_info.is_ru_suspended = 0;

   for(i=0;i<E100_MAX_RX_BUFFERS - 1 ;i++ )
   {
      start_addr_of_page = e100_rx_buff + i * PGSIZE;
      ptrrfd = ( struct rfd* ) start_addr_of_page;

      //In Each of the RFD block , indicate to suspend after 
      //the RU fills the packet.
       suspend_on = ( 1 << 14 );
       ptrrfd->cmd = suspend_on;

     //Set the link to point to next page
     ptrrfd->link = PADDR( start_addr_of_page + PGSIZE ) ; 


     //Set the count field as 0
     ptrrfd->count_field = 0x00;
 
     //set the size_field - which indicates the amount of data RU can fill
     ptrrfd->size_field = PGSIZE - 16 ;  //16 bytes header size;
   }

    //special handling for the last block
    start_addr_of_page = e100_rx_buff + ( E100_MAX_RX_BUFFERS - 1 ) * PGSIZE;
    ptrrfd = ( struct rfd* ) start_addr_of_page;
    suspend_on = ( 1 << 14 );
    ptrrfd->cmd = suspend_on;
    ptrrfd->link =  e100_rx_info.tx_buff_pa ;
    ptrrfd->count_field = 0x00;
    ptrrfd->size_field = PGSIZE - 16 ;  //16 bytes header size;

    //read the status word.
    if ( read_csr_status() != 0)
    {
         panic("CU is Not in Idle Mode\n");
    }

    //Now load the CU base address 
    //When Processing a CB, the CU adds th value of link to the base register.
    //so Keep the base register as 0x00.
    issue_cu_command(LOAD_CU_BASE,  0x00);

    //Add a Delay 
    delay(100);

     //Let the RU Unit start receiving the packets 
    issue_ru_command(LOAD_RU_BASE,0x00);

    //Add a Delay 
    delay(100);
    issue_ru_command(RU_START,e100_rx_info.tx_buff_pa);
    delay(100);
    cprintf("After RU start status %x\n",read_csr_status());
}

int e100_rcv_network_packet(uint32_t len,void * buf)
{
    //user wants recieved a packet 
    uint32_t next_block;
    struct rfd * ptr;
    uint32_t length;

    //cprintf("MANI - status is %x\n",read_csr_status());
    if( e100_rx_info.current_block == e100_rx_info.served ) 
    {
        //there are no new packets - All packets have been handed over
        //return -1;
        //cprintf("MANI - No New Packets\n");
        return -1;
    }
    else
    {
         //Give the packet next to currently served.
          cprintf("MANI - RCV IN served %d current %d\n",e100_rx_info.served,e100_rx_info.current_block);
         //Make a simplyfying assumption that user allocated one page .if not panic
         assert( len == PGSIZE);
         next_block = increment_and_wrap_index(e100_rx_info.served,E100_MAX_RX_BUFFERS);
 
         //Make sure that F and EOF bit are set in next_block by RU
         ptr = (struct rfd *) ( e100_rx_info.tx_buff_va + e100_rx_info.served * PGSIZE);
          
         
         assert( ( ptr->count_field & 0xC000 ) == 0xC000 );
         
         //Good . Copy the content to user buffer and free this rfd.
         uint32_t length =  ptr->count_field & ~0xC000;
         //cprintf("MANI - give %x bytes to user from %d \n",length,next_block);
         //Interpret the given buffer as 
         struct jif_pkt * pkt = buf;
         pkt->jp_len = length;
         memmove(pkt->jp_data,ptr->data,length);
         
         //update the served pointer
        e100_rx_info.served = next_block;

         //Reset the F anc EOF bits 
         ptr->count_field &= 0x03FF;
      //cprintf("\n============================\n");
      hexdump("INCOMINGDATA:", pkt->jp_data,pkt->jp_len);
      //cprintf("\n============================\n");
          cprintf("MANI - RCV OUT served %d current %d\n",e100_rx_info.served,e100_rx_info.current_block);

         //Before going away - see if we had suspended RU ? 
         if(e100_rx_info.is_ru_suspended == 1 )
         {
              //Resume RU and increment the current counter .
              e100_rx_info.current_block = increment_and_wrap_index( e100_rx_info.current_block ,E100_MAX_RX_BUFFERS);
              e100_rx_info.is_ru_suspended = 0;
              issue_ru_command(RU_RESUME,0x00);
              
         }
         return length;
    }
    return -1;
}
//send a Packet ( Max of 1518 size from the user space to E100 card ) 
int e100_send_network_packet(uint32_t len,void * buf)
{
    
      //user has sent a new packet to be sent . A
      //place the packet in the next free block 
       static int packetcount=0;

       uint16_t status_byte;
       uint32_t va = 0x00;

       packetcount++;

      cprintf("Sending packet req %d\n",packetcount);
      //cprintf("\n============================\n");
      hexdump("SENDINGDATA:",buf,len);
      //cprintf("\n============================\n");
      //cprintf("MANI - send-packet cur %d end %d\n",e100_tx_info.current_block,e100_tx_info.end);
      //if CU is in suspend mde resume it 
      //if CU is in idle mode - start it 

      if(e100_tx_info.free_count == 0 ) 
      {
         cprintf("TX BUFFER FULL - Cannot transmit packet\n");
         return -1;
     }

     //Ok , we have a free buffer - copy the user provided content to it 
     //Copy to block pointed by end - which contains values 0-MAX-1

     uint16_t cmd=0x00;

     va = e100_tx_info.tx_buff_va + e100_tx_info.end * PGSIZE  ;
     //cprintf("send-packet end %d va %x\n",e100_tx_info.end,va);

     //Now update the command word appropriately.
     cmd = 0x00;
     cmd = command_block_set_cmd(cmd,CMD_TRANSMIT);
     cmd = command_block_set_sbit(cmd);
     cmd = command_block_set_ibit(cmd);

     //cprintf("send-packet cmd %x\n",cmd);

     //len_f
     uint32_t len_f = len;
     len_f |= 0x00E00000;
     
     struct command_block * blockptr = (struct command_block *) va;
     blockptr->status = 0;  //TODO - RESET status ????????
     blockptr->cmd = cmd;
     blockptr->tbd = 0xFFFFFFFF;
     blockptr->len_field = len_f;
     memmove((void*)blockptr->data,buf,len);
    
     //cprintf("send-packet - next links %x\n",blockptr->link);
     e100_tx_info.free_count--;
     e100_tx_info.end = increment_and_wrap_index(e100_tx_info.end,E100_MAX_TX_BUFFERS);

     //switch based on the state of CU
     status_byte =  read_csr_status();

     //cprintf("MANI - send-packet csr_status %x\n",status_byte);
     
     //Determine the current CU State
     uint32_t cu_state = status_byte & CU_STATE_MASK_IN_CSR;

     if(cu_state == CU_STATE_IDLE_MASK)
     {
          cu_state = CU_IDLE_STATE;
     }
     else if ( cu_state == CU_STATE_SUSP_MASK)
     {
          cu_state = CU_SUSP_STATE;
     }
     else
     {
           cu_state = CU_ACTIVE_STATE;
     }

      switch ( cu_state)
      {
          case CU_IDLE_STATE : 
          //send a start command
          //cprintf("MANI - send-packet status is idle\n");
          issue_cu_command(CU_START,e100_tx_info.tx_buff_pa);
          e100_tx_info.current_block=0;
          //issue_cu_command(CU_START,0);
          break;

          case CU_SUSP_STATE: 
          //send a resume command
          //cprintf("MANI - send-packet status is suspended\n");
         e100_tx_info.current_block =  increment_and_wrap_index(e100_tx_info.current_block,E100_MAX_TX_BUFFERS);
          issue_cu_command(CU_RESUME,0);
          break;
          case CU_ACTIVE_STATE : 
          //cprintf("send-packet : CU in active state , wait until it moves to suspend\n");
          break;
      }
      //cprintf("MANI - send-packet  out cur %d end %d\n",e100_tx_info.current_block,e100_tx_info.end);
      //cprintf("Send packet return back\n");
      delay(25);
     return 0;
}

int32_t issue_ru_command( uint32_t commandType , uint32_t general_pointer)
{
     int result = 0 ;

     switch( commandType ) 
     {
          case LOAD_RU_BASE : 
          { 
             //First store the general pointer .
             //cprintf("MANI : storing GP %x\n",general_pointer);
             store_csr_general_pointer(general_pointer);

             //cprintf("MANI : storing cmd %x\n",LOAD_RU_BASE_MASK);
             store_csr_command_word   ( LOAD_RU_BASE_MASK );
          }
          break;

          case RU_START :
          {
             store_csr_general_pointer(general_pointer);
             uint16_t cmd = RU_START_MASK;
             //cmd = csr_command_set_cxbit(cmd);
             store_csr_command_word(cmd);
          }
          break;

          case RU_RESUME:
          {
             uint16_t cmd = RU_RESUME_MASK;
             //cmd = csr_command_set_cxbit(cmd);
             cprintf("MANI - setting it to RESUME \n");
             store_csr_command_word(cmd);
          }
          break;

          default :
           cprintf("UNKNOWN RU COMMAND %d\n",commandType ) ;
           return -1;
     } 

      return 0;
}


//This function issues commands to CU unit
int32_t issue_cu_command( uint32_t commandType , uint32_t general_pointer)
{
     int result = 0 ;

     switch( commandType ) 
     {
          case LOAD_CU_BASE : 
          { 
             //First store the general pointer .
             //cprintf("MANI : storing GP %x\n",general_pointer);
             store_csr_general_pointer(general_pointer);

             //cprintf("MANI : storing cmd %x\n",LOAD_CU_BASE_MASK);
             store_csr_command_word   ( LOAD_CU_BASE_MASK );
          }
          break;

          case CU_START :
          {
             store_csr_general_pointer(general_pointer);
             uint16_t cmd = CU_START_MASK;
             //cmd = csr_command_set_cxbit(cmd);
             store_csr_command_word(cmd);
          }
          break;

          case CU_RESUME:
          {
             uint16_t cmd = CU_RESUME_MASK;
             //cmd = csr_command_set_cxbit(cmd);
             store_csr_command_word(cmd);
          }
          break;

          default :
           cprintf("UNKNOWN CU COMMAND %d\n",commandType ) ;
           return -1;
     } 

      return 0;
}


void test_rx_blocks()
{
   uint32_t start = e100_rx_info.tx_buff_va;
   struct rfd * ptrrfd;
   int *ptr;
   int i ;

    cprintf(" base va %x base Pa %x \n",start,PADDR(start));
   cprintf("\n=====================================\n");
   for ( i=0; i < E100_MAX_RX_BUFFERS ; i++)
   {
        ptrrfd =(struct rfd * ) (  start + i * PGSIZE  ) ;
        ptr = (int * ) ptrrfd;
       cprintf(" [%d] : status %x , cmd : %x , link : %x  size %x\n",i,ptrrfd->status,ptrrfd->cmd,ptrrfd->link,ptr[3]);
   }
   cprintf("\n=====================================\n");
}

//test function to see if the Tx Buffers are properly linked.
void test_tx_blocks()
{
   uint32_t start = e100_tx_info.tx_buff_va;
   struct command_block * ptrcb;
   int i ;

    cprintf(" base va %x base Pa %x \n",start,PADDR(start));
   cprintf("\n=====================================\n");
   for ( i=0; i < E100_MAX_TX_BUFFERS ; i++)
   {
        ptrcb =(struct command_block * ) (  start + i * PGSIZE  ) ;
        cprintf(" [%d] : status %d , cmd : %d , link : %x \n",i,ptrcb->status,ptrcb->cmd,ptrcb->link); 
   }
   cprintf("\n=====================================\n");
}

//Function to read the  "status " word from CSR 
uint16_t read_csr_status(void)
{
      uint32_t csr_base = pci_e100_info.reg_base[1];
      uint32_t status_byte_address = csr_base + CSR_STATUS_WORD_OFFSET;

      return ( inw(status_byte_address) );
}

uint16_t update_csr_status(uint16_t status)
{
      uint32_t csr_base = pci_e100_info.reg_base[1];
      uint32_t status_byte_address = csr_base + CSR_STATUS_WORD_OFFSET;

       outw(status_byte_address,status) ;

       return 0;
}

uint16_t store_csr_command_word(uint16_t command)
{
      uint32_t csr_base = pci_e100_info.reg_base[1];
      uint32_t cmd_address = csr_base + CSR_COMMAND_WORD_OFFSET;

       outw(cmd_address,command) ;
      return 0;
}

uint16_t store_csr_general_pointer(uint32_t gp)
{
      uint32_t csr_base = pci_e100_info.reg_base[1];
      uint32_t gp_address = csr_base + CSR_GENERAL_POINTER_OFFSET;

      outl(gp_address,gp) ;
      return 0;
}

uint32_t increment_and_wrap_index(uint32_t what , uint32_t wrap)
{
       what++;
       return ( what % wrap );
}

uint16_t command_block_set_cmd(uint16_t cmd,uint32_t type)
{
       if( type == CMD_TRANSMIT)
       {
             cmd |= CMDBLOCK_TRANSMIT_MASK;
       }
       return ( cmd ) ;
}

uint16_t command_block_set_sbit(uint16_t cmd)
{
      cmd |= CMDBLOCK_SBIT_MASK;
      return(cmd ) ;
}

uint16_t command_block_unset_sbit(uint16_t cmd)
{
      cmd &= ~CMDBLOCK_SBIT_MASK;
      return(cmd ) ;
}

uint16_t command_block_set_ibit(uint16_t cmd)
{
      cmd |= CMDBLOCK_IBIT_MASK;
      return(cmd ) ;
}

uint16_t csr_command_set_mbit(uint16_t cmd)
{
      cmd |= CSR_MBIT_MASK;
      return(cmd ) ;
}

uint16_t csr_command_set_cxbit(uint16_t cmd)
{
      cmd |= CSR_CXBIT_MASK;
      return(cmd ) ;
}

void process_nic_interupt(void ) 
{
      //check the status byte of the CSR to determine which interupts are 
      //enabled.
      uint16_t csr_status;
      csr_status = read_csr_status();
     
      update_csr_status(csr_status );
      cprintf("MANI - processing nic , status %x\n",csr_status);

      //Now see which bits are enabled in the STAT/ACK byte
      if( is_cx_enabled( csr_status) ) 
      {
           process_cx_bit_in_stat(csr_status);
      }
     if ( is_fr_enabled(csr_status))
      {
           process_fr_bit_in_stat(csr_status);
      }
     if ( is_cna_enabled(csr_status))
      {
           process_cna_bit_in_stat(csr_status);
      }
      if ( is_rnr_enabled(csr_status))
      {
           process_rnr_bit_in_stat(csr_status);
      }
      if ( is_mdi_enabled(csr_status))
      {
           process_mdi_bit_in_stat(csr_status);
      }
      if ( is_swi_enabled(csr_status))
      {
           process_swi_bit_in_stat(csr_status);
      }
      if ( is_fcp_enabled(csr_status))
      {
           process_fcp_bit_in_stat(csr_status);
      }

      irq_eoi();
      return;
}

void process_fr_bit_in_stat(uint16_t status)
{
    cprintf("process_fr_bit_in_stat stat : %x\n",status );
    //The RU unit has just wrote a Packet . Decide, if we 
    //want the Resume RU or Keep it suspended.
    uint32_t new_block = increment_and_wrap_index(e100_rx_info.current_block,E100_MAX_RX_BUFFERS);
    struct rfd * ptrrfd = (struct rfd * ) ( e100_rx_info.tx_buff_va + new_block * PGSIZE ) ;
 
    //check the value filled by RU to current_block;
   struct rfd * ptrc = (struct rfd * ) ( e100_rx_info.tx_buff_va + e100_rx_info.current_block*PGSIZE );
    //cprintf("MANI - RU has filled count field as %x  size field is %x \n",ptrc->count_field,ptrc->size_field); 
     cprintf("MANI new %d served %d current %d \n",new_block,e100_rx_info.served,e100_rx_info.current_block);
    if(new_block != e100_rx_info.served )
    { 
        //we have more blocks available - ask RU to resume.
        e100_rx_info.current_block = new_block;
        //update_csr_status(status |CU_FRBIT_IN_CSR_STAT);

        //before resuming the RU, make sure that EOF and F bit are reset.
        //ptrrfd->count_field = 0x00;
         int a = ptrc->count_field & ~0xC000;

         hexdump("RU:",ptrc->data,a);
        cprintf("we have some buffers avilabel in RFD - Resume RF\n");
        issue_ru_command(RU_RESUME,0x00);
    }
    else
    {
        //we dont have anymore buffer -let us stop RU.
        //it is already suspended. let it be there.
       cprintf("RU IS SUSPENDED\n");
        e100_rx_info.is_ru_suspended = 1;
    }
     cprintf("MANI out served %d current %d \n",e100_rx_info.served,e100_rx_info.current_block);
}

void process_cna_bit_in_stat(uint16_t status)
{
     //cprintf("process_cna_bit_in_stat\n");
    //update_csr_status(status | CU_CNABIT_IN_CSR_STAT);
}

void process_rnr_bit_in_stat(uint16_t status)
{
     cprintf("process_rnr_bit_in_stat\n");
    //update_csr_status(status | CU_RNRBIT_IN_CSR_STAT);
}

void process_mdi_bit_in_stat(uint16_t status)
{
     //cprintf("process_mdi_bit_in_stat\n");
    //update_csr_status(status | CU_MDIBIT_IN_CSR_STAT);
}

void process_swi_bit_in_stat(uint16_t status)
{
    //cprintf("process_swi_bit_in_stat\n");
    //update_csr_status(status | CU_SWIBIT_IN_CSR_STAT);
}

void process_fcp_bit_in_stat(uint16_t status)
{
    //cprintf("process_fcp_bit_in_stat\n");
    //update_csr_status(status | CU_FCPBIT_IN_CSR_STAT);
}

void process_cx_bit_in_stat(uint16_t status)
{
    //we just received an interupt fromt he NIC Card and we see that 
    //Cx Bit in ON in STAT/ACK, which means ,the card just sent a packet.
    
     //cprintf("Cx bit on \n");
    //Completed sending a packet  - increment free_count;
    e100_tx_info.free_count++;

    struct command_block * curptr;
    uint16_t new_cb;
    //1.Take care of STAT/ACK
    //2.Take care of current CB

    //Before that acknowledge the interupt
    //cprintf("Ack the CX and CNA interupt\n");
    //update_csr_status(status | CU_CXBIT_IN_CSR_STAT);

    curptr = (struct command_block *) e100_tx_info.tx_buff_va +
                                      e100_tx_info.current_block * PGSIZE;

    //cprintf("Before curr block %d cmd %x\n",e100_tx_info.current_block,curptr->cmd);
    curptr->cmd &= ~CMDBLOCK_SBIT_MASK;
    //cprintf("After curr block %d cmd %x\n",e100_tx_info.current_block,curptr->cmd);

    //when incrementing current, if it becomes equal to end, then
    //we are at the last packet - keep CU in suspend state ( maybe idle??)
    new_cb = increment_and_wrap_index(e100_tx_info.current_block,E100_MAX_TX_BUFFERS) ;

    //cprintf("cur block %d end %d new block %d\n",e100_tx_info.current_block,e100_tx_info.end,new_cb);

    if( new_cb == e100_tx_info.end )
    {
         cprintf("No More packets to send - stay in suspend mode\n");
         return ; 
    }
    else
    {
       //there are some more packets queued up
       //Ask CU to dispatch them.

       //cprintf("we have a packet to send - Resume CU\n");

      //Unset the s bit in the current cb
       
       e100_tx_info.current_block = increment_and_wrap_index(e100_tx_info.current_block,E100_MAX_TX_BUFFERS);
       issue_cu_command(CU_RESUME,0);
    }

    return;
}

uint16_t is_cx_enabled(uint16_t status)
{
     return ( status & CU_CXBIT_IN_CSR_STAT );
}

uint16_t is_mdi_enabled(uint16_t status)
{
     return ( status & CU_MDIBIT_IN_CSR_STAT );
}

uint16_t is_fr_enabled(uint16_t status)
{
     return ( status & CU_FRBIT_IN_CSR_STAT );
}

uint16_t is_cna_enabled(uint16_t status)
{
     return ( status & CU_CNABIT_IN_CSR_STAT );
}

uint16_t is_rnr_enabled(uint16_t status)
{
     return ( status & CU_RNRBIT_IN_CSR_STAT );
}

uint16_t is_swi_enabled(uint16_t status)
{
     return ( status & CU_SWIBIT_IN_CSR_STAT );
}

uint16_t is_fcp_enabled(uint16_t status)
{
     return ( status & CU_FCPBIT_IN_CSR_STAT );
}

#ifndef JOS_KERN_E100_H
#define JOS_KERN_E100_H

#include "inc/mmu.h"

#define E100_MAX_TX_BUFFERS  10
#define E100_MAX_RX_BUFFERS  10

#define CSR_STATUS_WORD_OFFSET          0x00
#define CSR_COMMAND_WORD_OFFSET         0x02
#define CSR_GENERAL_POINTER_OFFSET      0x04
#define CSR_PORT_OFFSET                 0x08

//CU Command types 
#define LOAD_CU_BASE 0x00
#define CU_START     0x01
#define CU_RESUME    0x02

//CU States Masks
#define CU_STATE_IDLE_MASK   0x0000
#define CU_STATE_SUSP_MASK   0x0040

//CU states
#define CU_IDLE_STATE   0x00
#define CU_SUSP_STATE   0x01
#define CU_ACTIVE_STATE 0x02

//CSR status masks
#define CU_STATE_MASK_IN_CSR 0x00C0
#define CU_CXBIT_IN_CSR_STAT 0x8000
#define CU_FRBIT_IN_CSR_STAT 0x4000
#define CU_CNABIT_IN_CSR_STAT 0x2000
#define CU_RNRBIT_IN_CSR_STAT 0x1000
#define CU_MDIBIT_IN_CSR_STAT 0x0800
#define CU_SWIBIT_IN_CSR_STAT 0x0400
#define CU_RSRVDBIT_IN_CSR_STAT 0x0200
#define CU_FCPBIT_IN_CSR_STAT  0x0100

#define CSR_MBIT_MASK 0x0100
#define CSR_CXBIT_MASK 0x8000

//Command Masks
#define LOAD_CU_BASE_MASK 0x0060
#define CU_START_MASK     0x0010
#define CU_RESUME_MASK    0x0020

//command block masks
#define CMDBLOCK_TRANSMIT_MASK  0x0004
#define CMDBLOCK_SBIT_MASK      0x4000
#define CMDBLOCK_IBIT_MASK      0x2000

//define command block command types
#define CMD_TRANSMIT 0x01

//RU Command types 
#define LOAD_RU_BASE 0x00
#define RU_START     0x01
#define RU_RESUME    0x02

//Command Masks
#define LOAD_RU_BASE_MASK 0x0006
#define RU_START_MASK     0x0001
#define RU_RESUME_MASK    0x0002

struct  rx_info
{
    uint32_t tx_buff_va ;
    uint32_t tx_buff_pa ;
    uint32_t free_count ;
    uint32_t current_block; //where the RU will fill the data 
    uint32_t served;        //Where the user will get the data 
    uint32_t is_ru_suspended;
} ;

struct  tx_info
{
    uint32_t tx_buff_va ;
    uint32_t tx_buff_pa ;
    uint32_t free_count ;
    uint32_t current_block;
    uint32_t end;
    uint32_t free_map[E100_MAX_TX_BUFFERS];
} ;

struct command_block
{
    volatile uint16_t status;
    uint16_t cmd;
    uint32_t link;
    uint32_t tbd;
    uint32_t len_field;  //TODO - this needs to be proper
    uint8_t data[0];
};

struct rfd
{
   volatile uint16_t status;
   uint16_t cmd;
   uint32_t link;
   uint32_t reserved;
   uint16_t count_field;
   uint16_t size_field;
   uint8_t data[0];
};

extern uint32_t e100_tx_buff ;
extern uint32_t e100_rx_buff ;
extern struct pci_func pci_e100_info;

void test_tx_blocks ( void );
void test_rx_blocks(void);
void e100_initialise( void ) ;
int32_t issue_cu_command( uint32_t commandType , uint32_t general_pointer);
int32_t issue_ru_command( uint32_t commandType , uint32_t general_pointer);

int e100_rcv_network_packet(uint32_t len,void * buf);
int e100_send_network_packet(uint32_t len,void * buf);

//some helper functions.
uint16_t read_csr_status(void );
uint16_t update_csr_status(uint16_t status);
uint16_t store_csr_general_pointer(uint32_t gp);
uint16_t store_csr_command_word(uint16_t command);

uint16_t csr_command_set_cxbit(uint16_t cmd);
uint16_t csr_command_set_mbit(uint16_t cmd);

uint16_t command_block_set_cmd(uint16_t cmd,uint32_t type);
uint16_t command_block_unset_sbit(uint16_t cmd);
uint16_t command_block_set_sbit(uint16_t cmd);
uint16_t command_block_set_ibit(uint16_t cmd);

//util functions
uint32_t increment_and_wrap_index(uint32_t what , uint32_t wrap);
uint16_t is_cx_enabled(uint16_t status);
uint16_t is_fr_enabled(uint16_t status);
uint16_t is_cna_enabled(uint16_t status);
uint16_t is_rnr_enabled(uint16_t status);
uint16_t is_mdi_enabled(uint16_t status);
uint16_t is_swi_enabled(uint16_t status);
uint16_t is_fcp_enabled(uint16_t status);

void process_cx_bit_in_stat(uint16_t status);
void process_fr_bit_in_stat(uint16_t status);
void process_cna_bit_in_stat(uint16_t status);
void process_rnr_bit_in_stat(uint16_t status);
void process_mdi_bit_in_stat(uint16_t status);
void process_swi_bit_in_stat(uint16_t status);
void process_fcp_bit_in_stat(uint16_t status);

void process_nic_interupt(void);
#endif	// JOS_KERN_E100_H

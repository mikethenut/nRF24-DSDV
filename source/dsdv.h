#ifndef DSDV_HEADRF
#define DSDV_HEADRF

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOSConfig.h"
#include "espressif/esp_common.h"
#include "FreeRTOS.h"
#include "esp/uart.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "RF24/nRF24L01.h"
#include "RF24/RF24.h"

/*
** USER CONFIGURATION
** CHANGE AS YOU LIKE
*/

// Network channel and address (used to communicate with any active device)
const uint8_t channel = 78;
const uint8_t network_address[] = {0x6E, 0x52, 0x46};

// If enabled, application prints every packet received/sent; can be reconfigured dynamically
extern bool print_incoming_packet;
extern bool print_outgoing_packet;

//If enabled, application prints every core function invocation; can be reconfigured dynamically
extern bool verbose;

// Determines how many destinations can be stored in routing table at initialization
// The table size is doubled when full and new destination is to be added
#define TABLE_SIZE_INIT	4

// Determines how often incremental changes should be broadcast (in sec)
#define BRCST_INTERVAL	8

// Determines how often full table dump should be broadcast (in sec, should be multiple of BRCST_INTERVAL)
#define DUMP_INTERVAL	48

// Determines how often table should be checked for dead entries (in sec, should be multiple of BRCST_INTERVAL)
#define CHECK_INTERVAL	24

// Determines when an entry is considered dead after no communication (in sec)
#define TIMEOUT			40

// Determines when an entry is deleted after no communication (in sec)
#define ENTRY_DELETE	80


/*
** NETWORK CONFIGURATION
** ONLY CHANGE IF NECESSARY
*/

// device configuration
#define SCL 			14
#define CE_NRF			3
#define CS_NRF			0

// packet configuration
#define MSG_LEN			32
#define ROWS_PER_MSG	4
#define MSG_ROW_LEN		8
#define ADDR_LEN		3
#define SQNC_LEN		4


/*
** DATA STRUCTURES & PUBLIC DATA FIELDS
*/

struct update_row {
	uint8_t destination[ADDR_LEN];
    uint8_t source[ADDR_LEN];
	uint32_t sequence_number;
    uint8_t hops;
};

struct routing_row {
    uint8_t destination[ADDR_LEN];
    uint8_t next_hop[ADDR_LEN];
	uint32_t sequence_number;
    uint8_t hops;
	bool modified;
	TickType_t last_rcvd;
};
 
// Data field for received DSDV packet (32bit)
extern uint8_t* dsdvRecv;
// Data field for DSDV packet to be sent (32bit)
extern uint8_t* dsdvSend;

// Data field for packets addressed to this device and length of packet
extern uint8_t* dataRecv;
extern int dataLen;

// Semaphor activated when device receives packed addressed to it
extern SemaphoreHandle_t semphr_trgt_packet;

// Semaphor for triggering parsing of dsdvRecv (incoming protocol data)
extern SemaphoreHandle_t semphr_dsdv_packet;

// Contains data parsed from received packet (get_utable_size() entries)
extern update_row* update_data;
// Contains current routing data for device (get_rtable_size() entries)
extern routing_row* routing_table;


/*
** EXPOSED FUNCTIONS
*/

// Initializes device and network for DSDV protocol
// Expects device address as byte array of size ADDR_LEN
void DSDV_init(uint8_t* local_address);

// Forwards data packet of length len according to routing table to destination
// Returns false if data too large or destination is not found
bool forward_data(uint8_t* data, int len, uint8_t* destination);

// Returns address of network (for broadcasts)
const uint8_t* get_network_address();

// Returns address of this device
uint8_t* get_device_address();

// Returns size of routing table
uint8_t get_rtable_size();

// Returns size of update table (ROWS_PER_MSG)
uint8_t get_utable_size();

// Prints full routing table to stdout
void print_table();

// Prints len bytes to stdout as hex
void print_bytes(uint8_t* data, int len);

#endif


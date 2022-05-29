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

// Set this to 1 if you want to use I2C bus in your application
#define INCLUDE_I2C_BUS_SUPPORT		1

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

// Determines when an entry is deleted after determined dead (in sec)
#define ENTRY_DELETE	200

// If enabled, application prints every packet received/sent; can be reconfigured dynamically
extern bool print_incoming_packet;
extern bool print_outgoing_packet;

// If enabled, application flashes LEDs when receiving/sending packets
// led1=snd_dsdv, led2=rcv_dsdv, led3=snd_data, led4=rcv_data
// MUST INCLUDE I2C_BUS_SUPPORT TO USE
extern bool comm_led_flash;

//If enabled, application prints every core function invocation; can be reconfigured dynamically
extern bool verbose;


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
 
// Semaphor activated when device receives packed addressed to it
extern SemaphoreHandle_t semphr_trgt_packet;

// Data field for packets addressed to this device and length of packet
extern uint8_t* dataRecv;
extern int dataLen;

// Semaphor for triggering parsing of dsdvRecv (incoming protocol data)
extern SemaphoreHandle_t semphr_dsdv_packet;

// Data field for received DSDV packet (32bit)
extern uint8_t* dsdvRecv;
// Data field for DSDV packet to be sent (32bit)
extern uint8_t* dsdvSend;

// Mutex semaphor for taking direct access to the routing table.
extern SemaphoreHandle_t semphr_dsdv_table;

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


/*
** I2C BUS USAGE
*/

#if INCLUDE_I2C_BUS_SUPPORT

#include "i2c/i2c.h"

#define BUS_I2C		0
#define PCF_ADDRESS	0x38
#define SDA 		12

#define button1		0x20	// 0b ??0? ????
#define button2		0x10	// 0b ???0 ????
#define button3		0x80	// 0b 0??? ????
#define button4		0x40	// 0b ?0?? ????
#define clr_btn		0xf0

#define led1 		0xfe	// 0b ???? ???0
#define led2 		0xfd	// 0b ???? ??0?
#define led3 		0xfb	// 0b ???? ?0??
#define led4 		0xf7	// 0b ???? 0???
#define clr_all		0xff

// write byte to PCF on I2C bus
void write_byte_pcf(uint8_t data);

// read byte from PCF on I2C bus
uint8_t read_byte_pcf();

#endif

#endif


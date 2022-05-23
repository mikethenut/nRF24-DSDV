#ifndef DSDV_HEADRF
#define DSDV_HEADRF

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "espressif/esp_common.h"
#include "FreeRTOS.h"
#include "esp/uart.h"
#include "task.h"
#include "RF24/nRF24L01.h"
#include "RF24/RF24.h"

/*
** USER CONFIGURATION
*/

// Network channel and address (used to communicate with any on-line device)
const uint8_t channel = 78;
const uint8_t network_address[] = {0x6E, 0x52, 0x46};

// Determines how many destinations can be stored in routing table at initialization
// The table size is doubled when full and new destination is to be added
#define TABLE_SIZE_INIT	4

// Determines how often incremental changes should be broadcast (in sec)
#define BRCST_INTERVAL	5

// Determines how often full table dump should be broadcast (in sec)
#define DUMP_INTERVAL	60

// Determines how often table should be checked for dead entries (in sec)
#define CHECK_INTERVAL	60

// Determines when an entry is considered dead after no communication (in sec)
#define TIMEOUT			180

// Determines when an entry is deleted after no communication (in sec)
#define ENTRY_DELETE	300


/*
** NETWORK CONFIGURATION
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
** EXPOSED FUNCTIONS
*/

// Prints full routing table to stdout
void print_table(void *pvParameters);

// Initializes device and network for DSDV protocol
void DSDV_init(void *pvParameters);

#endif
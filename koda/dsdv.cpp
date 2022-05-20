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
** DATA STRUCTURES
*/

struct routing_row {
    uint8_t destination[ADDR_LEN];
    uint8_t next_hop[ADDR_LEN];
	uint32_t sequence_number;
    uint8_t hops;
	bool modified;
	TickType_t last_rcvd;
};

struct update_row {
	uint8_t destination[ADDR_LEN];
    uint8_t source[ADDR_LEN];
	uint32_t sequence_number;
    uint8_t hops;
};

routing_row* routing_table;
update_row* update_data;

uint8_t table_size_max;
uint8_t table_size_cur;

uint8_t device_address[ADDR_LEN];
static RF24 radio(CE_NRF, CS_NRF);

uint8_t dataRecv[MSG_LEN];
uint8_t dataSend[MSG_LEN];

TickType_t last_rcvd;
TickType_t last_brcst;
TickType_t last_dump;


/*
** GENERAL HELPER FUNCTIONS
*/

// Checks if two addresses are equal
bool equal_addr(uint8_t* addr1, uint8_t* addr2) {
	bool equal = true;
	for(int i = 0; i < ADDR_LEN; i++) {
		if(addr1[i] != addr2[i])
			equal = false;
	}
	return equal;
}

// Finds destination address index in routing table, returns -1 otherwise
int addr_index(uint8_t* addr) {
	int addr_ind = -1;
	for(int i = 0; i < table_size_cur; i++) {
		if(equal_addr(routing_table[i].destination, addr))
			addr_ind = i;
	}
	return addr_ind;
}

// Send nRF24 packet
void nRF24_transmit(void *pvParameters) {
	radio.stopListening();
	radio.write(&dataSend, sizeof(dataSend));
	radio.startListening();
}


/*
** MAIN DSDV FUNCTIONS
*/

// This function dumps entire table
void full_table_dump(void *pvParameters) {
	// Write own information in first row (for all packets)
	for(int j = 0; j < ADDR_LEN; j++)
		dataSend[j] = routing_table[0].destination[j];
	for(int j = 0; j < SQNC_LEN; j++)
		dataSend[ADDR_LEN + j] = (uint8_t) (routing_table[0].sequence_number >> 8*j) & 0xFF;
	dataSend[ADDR_LEN + SQNC_LEN] = routing_table[0].hops;

	int row = 1;
	for(int i = 1; i < table_size_cur; i++) {
		// The first 3 bytes contain routing destination
		for(int j = 0; j < ADDR_LEN; j++)
			dataSend[row*MSG_ROW_LEN + j] = routing_table[i].destination[j];
		// The 4 bytes after contain the sequence number
		for(int j = 0; j < SQNC_LEN; j++)
			dataSend[row*MSG_ROW_LEN + ADDR_LEN + j] = (uint8_t) (routing_table[i].sequence_number >> 8*j) & 0xFF;
		// The last byte in row contains number of hops
		dataSend[row*MSG_ROW_LEN + ADDR_LEN + SQNC_LEN] = routing_table[i].hops;
		routing_table[i].modified = false;

		row++;
		if(row == ROWS_PER_MSG) {
			nRF24_transmit(NULL);
			row = 1;
		}
	}

	// Send rest of data (if any)
	if(row > 1) {
		// Insert dummy rows with network address for unused space
		for(int i = row; i < ROWS_PER_MSG; i++) {
			for(int j = 0; j < ADDR_LEN; j++)
				dataSend[row*MSG_ROW_LEN + j] = network_address[j];
		}
		nRF24_transmit(NULL);
	}
		
	// Update timers
	last_brcst = xTaskGetTickCount();
	last_dump = xTaskGetTickCount();

	// Update own entry
	routing_table[0].sequence_number += 2;
	routing_table[0].last_rcvd = last_brcst;
}

// This function prepares data for incremental table update and schedules transmission
void format_packet(void *pvParameters) {
	// Check number of rows to send
	int updated_rows = 0;
	for(int i = 0; i < table_size_cur; i++) {
		if(routing_table[i].modified)
			updated_rows++;
	}

	TickType_t current_time = xTaskGetTickCount();
	// If updates cannot be sent in a single message, or enough time has passed, dump entire table
	if(updated_rows > ROWS_PER_MSG || current_time - last_dump > pdMS_TO_TICKS(DUMP_INTERVAL * 1000))
		xTaskCreate(full_table_dump, "table_dump", 1024, NULL, 7, NULL);

	else {
		// Otherwise write rows into packet
		int row = 0;
		for(int i = 0; i < table_size_cur; i++) {
			if(routing_table[i].modified) {
				// The first 3 bytes of each row entry contain routing destination
				for(int j = 0; j < ADDR_LEN; j++)
					dataSend[row*MSG_ROW_LEN + j] = routing_table[i].destination[j];

				// The 4 bytes after destination contain the sequence number
				for(int j = 0; j < SQNC_LEN; j++)
					dataSend[row*MSG_ROW_LEN + ADDR_LEN + j] = (uint8_t) (routing_table[i].sequence_number >> 8*j) & 0xFF;

				// The last byte in row contains number of hops
				dataSend[row*MSG_ROW_LEN + ADDR_LEN + SQNC_LEN] = routing_table[i].hops;

				row++;
			}
		}

		// Insert dummy rows with network address for unused space
		for(int i = row; i < ROWS_PER_MSG; i++) {
			for(int j = 0; j < ADDR_LEN; j++)
				dataSend[row*MSG_ROW_LEN + j] = network_address[j];
		}

		// Send packet
		xTaskCreate(nRF24_transmit, "nRF24_transmit", 1024, NULL, 8, NULL);
		last_brcst = xTaskGetTickCount();

		// Update own entry
		routing_table[0].sequence_number += 2;
		routing_table[0].last_rcvd = last_brcst;
	}
}

// This function checks table for dead entries
void check_table(void *pvParameters) {
	TickType_t current_time = xTaskGetTickCount();
	int to_remove = -1;

	for(int i = 0; i < table_size_cur; i++) {
		// If the entry is already dead (odd sequence number), leave it alone
		if(routing_table[i].sequence_number % 2 != 1 && current_time - routing_table[i].last_rcvd > pdMS_TO_TICKS(TIMEOUT * 1000)) {
			routing_table[i].sequence_number++;
			routing_table[i].hops = 255;
			routing_table[i].modified = true;
		}

		// If the entry has been dead for a while, stage it for removal
		// This is done naively by storing the index; only one row is removed per function call
		if(current_time - routing_table[i].last_rcvd > pdMS_TO_TICKS(ENTRY_DELETE * 1000))
			to_remove = i;
	}

	// Remove row if dead for long time
	if(to_remove != -1) {
		for(int i = to_remove; i < table_size_cur - 1; i++)
			routing_table[i] = routing_table[i+1];
		table_size_cur--;
	}
}

// This function updates table based on received data
void update_table(void *pvParameters) {
	for(int i = 0; i < ROWS_PER_MSG; i++) {
		// If the address is equal to network address, discard it
		if(equal_addr(update_data[i].destination, (uint8_t *) network_address))
			continue;

		// Find routing table row for address
		int addr_ind = addr_index(update_data[i].destination);
		if(addr_ind == -1) {
			// New entry
			if(table_size_cur == table_size_max) {
				// Resize table
				table_size_max *= 2;
				routing_table = (routing_row *) realloc(routing_table, table_size_max*sizeof(routing_row));
			}

			for(int j = 0; j < ADDR_LEN; j++)
				routing_table[table_size_cur].destination[j] = update_data[i].destination[j];
			for(int j = 0; j < ADDR_LEN; j++)
				routing_table[table_size_cur].next_hop[j] = update_data[i].source[j];
			routing_table[table_size_cur].hops = update_data[i].hops + 1;
			routing_table[table_size_cur].sequence_number = update_data[i].sequence_number;
			routing_table[table_size_cur].modified = true;
			routing_table[table_size_cur].last_rcvd = last_rcvd;

			table_size_cur++;

		} else {
			// Update row if sequence number is greater or equal with fewer hops
			if(update_data[i].sequence_number > routing_table[addr_ind].sequence_number ||
			   		(update_data[i].sequence_number == routing_table[addr_ind].sequence_number && 
			   		 update_data[i].hops + 1 < routing_table[addr_ind].hops)	) {
				
				routing_table[addr_ind].sequence_number = update_data[i].sequence_number;
				routing_table[addr_ind].last_rcvd = last_rcvd;

				if(routing_table[addr_ind].hops != update_data[i].hops + 1) {
					routing_table[addr_ind].hops = update_data[i].hops + 1;
					routing_table[addr_ind].modified = true;
				}

				if(!equal_addr(routing_table[addr_ind].next_hop, update_data[i].source)) {
					for(int j = 0; j < ADDR_LEN; j++)
						routing_table[addr_ind].next_hop[j] = update_data[i].source[j];
					routing_table[addr_ind].modified = true;
				}
				
			}
		}
	}
}

// This function reads incoming packets
void parse_packet(void *pvParameters) {
	// The packet contains 4 entries of 8 bytes, the first being the sender
	for(int i = 0; i < ROWS_PER_MSG; i++) {
		
		// The first 3 bytes of each row entry contain routing destination
		for(int j = 0; j < ADDR_LEN; j++)
			update_data[i].destination[j] = dataRecv[i*MSG_ROW_LEN + j];
		
		// If the address is equal to network address, discard it
		if(equal_addr(update_data[i].destination, (uint8_t *) network_address))
			continue;

		// Otherwise, copy rest of data
		// The first 3 bytes of first row contain senders address
		for(int j = 0; j < ADDR_LEN; j++)
			update_data[i].source[j] = dataRecv[j];
		
		// The 4 bytes after destination contain the sequence number
		update_data[i].sequence_number = 0;
		for(int j = 0; j < SQNC_LEN; j++)
			update_data[i].sequence_number |= (((uint32_t) dataRecv[i*MSG_ROW_LEN + ADDR_LEN + j]) << 8*j);

		// The last byte in row contains number of hops
		update_data[i].hops = dataRecv[i*MSG_ROW_LEN + ADDR_LEN + SQNC_LEN];
	}

	// Schedule task to update table info
	xTaskCreate(update_table, "update_table", 1024, NULL, 5, NULL);
}


/*
** nRF24 RADIO LOOP AND INITIALIZATION
*/


// Listen for packets
void nRF24_listen(void *pvParameters) {
	while (1) {
		if (radio.available()) {
			radio.read(&dataRecv, sizeof(dataRecv));
			last_rcvd = xTaskGetTickCount();
			xTaskCreate(parse_packet, "parse_packet", 1024, NULL, 3, NULL);
		}

		// Check if it is time to check table or broadcast info
		TickType_t current_time = xTaskGetTickCount();
		if(current_time - last_brcst > pdMS_TO_TICKS(CHECK_INTERVAL * 1000))
			xTaskCreate(check_table, "check_table", 1024, NULL, 4, NULL);
		if(current_time - last_brcst > pdMS_TO_TICKS(BRCST_INTERVAL * 1000))
			xTaskCreate(format_packet, "format_packet", 1024, NULL, 6, NULL);

		// Sleep for 200 ms
		radio.powerDown();
		vTaskDelay(pdMS_TO_TICKS(200));
		radio.powerUp();
	}
}

void DSDV_init(void *pvParameters) {
	uart_set_baud(0, 115200);
	gpio_enable(SCL, GPIO_OUTPUT);
	gpio_enable(CS_NRF, GPIO_OUTPUT);

	// TODO: Set unique device local id
	for (int i = 0; i < ADDR_LEN; i++) {
		device_address[i] = 0x00;
	}

	// Set broadcast and dump timers
	last_brcst = xTaskGetTickCount();
	last_dump = xTaskGetTickCount();

	// Initialize data tables
	table_size_max = TABLE_SIZE_INIT;
	table_size_cur = 1;
	routing_table = (routing_row*) malloc(table_size_max * sizeof(routing_row));
	update_data = (update_row*) malloc(ROWS_PER_MSG * sizeof(update_row));

	// Write device address as first entry in routing table
	for(int i = 0; i < ADDR_LEN; i++) {
		routing_table[0].destination[i] = device_address[i];
		routing_table[0].next_hop[i] = device_address[i];
	}
	routing_table[0].hops = 0;
	routing_table[0].sequence_number = 0;
	routing_table[0].modified = true;
	routing_table[0].last_rcvd = last_brcst;

	// Radio initialization
	radio.begin();
	radio.setChannel(channel);
	radio.openWritingPipe(network_address);
    radio.openReadingPipe(1, network_address);
	radio.startListening();

	// TODO: Listen for messages directed to this device specifically

	// Start listening for incoming packets and broadcast timers
	xTaskCreate(nRF24_listen, "nRF24_listen", 1024, NULL, 2, NULL);
}


extern "C" void user_init(void) {
	xTaskCreate(DSDV_init, "DSDV_init", 1024, NULL, 1, NULL);
}

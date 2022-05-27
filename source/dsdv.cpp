#include "dsdv.h"

/*
** DATA FIELDS
*/

update_row* update_data;
routing_row* routing_table;

uint8_t* dsdvRecv;
uint8_t* dsdvSend;

uint8_t* forwardRecv;
uint8_t* forwardSend;

uint8_t* trash;

// Data field for packets addressed to this device and length of packet
uint8_t* dataRecv;
int dataLen;

uint8_t table_size_max;
uint8_t table_size_cur;

uint8_t device_address[ADDR_LEN];
static RF24 radio(CE_NRF, CS_NRF);

SemaphoreHandle_t semphr_dsdv_packet;
SemaphoreHandle_t semphr_dsdv_table;
SemaphoreHandle_t semphr_trgt_packet;

TickType_t last_rcvd;
TickType_t last_check;
TickType_t last_brcst;
TickType_t last_dump;

// If enabled, application prints every packet received/sent
bool print_incoming_packet = false;
bool print_outgoing_packet = false;

//If enabled, application prints every core function invocation
bool verbose = false;

// If enabled, application flashes LEDs when receiving/sending packets
bool comm_led_flash = false;


/*
** I2C FUNCTIONS
*/

#if INCLUDE_I2C_BUS_SUPPORT

// write byte to PCF on I2C bus
void write_byte_pcf(uint8_t data) {

	// disable radio
	gpio_write(CS_NRF, 1);
	// reinitialize i2c
	i2c_init(BUS_I2C, SCL, SDA, I2C_FREQ_100K);
	// write data byte
	i2c_slave_write(BUS_I2C, PCF_ADDRESS, NULL, &data, 1);
}

// read byte from PCF on I2C bus
uint8_t read_byte_pcf() {
	uint8_t data;

	// disable radio
	gpio_write(CS_NRF, 1);
	// reinitialize i2c
	i2c_init(BUS_I2C, SCL, SDA, I2C_FREQ_100K);
	// read data byte
	i2c_slave_read(BUS_I2C, PCF_ADDRESS, NULL, &data, 1);

	return data;
}

#else

void write_byte_pcf(uint8_t data) {
	printf("ERROR: I2C BUS NOT ENABLED.");
}

uint8_t read_byte_pcf() {
	printf("ERROR: I2C BUS NOT ENABLED.");
	return NULL;
}

#endif


/*
** EXTERNAL HELPER FUNCTIONS
*/

// Returns address of network (for broadcasts)
const uint8_t* get_network_address() {
	return network_address;
}

// Returns address of this device
uint8_t* get_device_address() {
	return device_address;
}

// Returns current size of table (table_size_cur)
uint8_t get_rtable_size() {
	return table_size_cur;
}

// Returns size of update table (ROWS_PER_MSG)
uint8_t get_utable_size() {
	return ROWS_PER_MSG;
}

// Prints full routing table to stdout
// Takes mutex lock for routing table access
void print_table() {
	BaseType_t mutex_available = xSemaphoreTake(semphr_dsdv_table, (TickType_t) 10);
	while(!mutex_available) {
		// Sleep for 200 ms
		vTaskDelay(pdMS_TO_TICKS(200));
		mutex_available = xSemaphoreTake(semphr_dsdv_table, (TickType_t) 10);
	}

	TickType_t current_time = xTaskGetTickCount();
	printf("-------------------------------------------------\n");
	printf(" DEST  | NXTHOP | LEN | SEQUENCE | MOD | AGE (s)\n");
	for(int i = 0; i < table_size_cur; i++) {
		for (int j = 0; j < ADDR_LEN; j++)
			printf("%02X", routing_table[i].destination[j]);
		printf(" | ");
		for (int j = 0; j < ADDR_LEN; j++)
			printf("%02X", routing_table[i].next_hop[j]);
		printf(" | %03d | ", routing_table[i].hops);
		for (int j = SQNC_LEN - 1; j >= 0; j--)
			printf("%02X", (uint8_t) (routing_table[i].sequence_number >> 8*j) & 0xFF);
		if(routing_table[i].modified)
			printf(" |  X  | ");
		else
			printf(" |     | ");
		printf("%d\n", (current_time - routing_table[i].last_rcvd) / configTICK_RATE_HZ);
	}
	printf("-------------------------------------------------\n");

	xSemaphoreGive(semphr_dsdv_table);
}

// Prints bytes to stdout as hex
void print_bytes(uint8_t* data, int len) {
	for(int i = 0; i < len; i++)
		printf("%02X", data[i]);
	printf("\n");
}


/*
** INTERNAL HELPER FUNCTIONS
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
// Should only be called from mutex lock for routing table access
int addr_index(uint8_t* addr) {
	int addr_ind = -1;
	for(int i = 0; i < table_size_cur; i++) {
		if(equal_addr(routing_table[i].destination, addr))
			addr_ind = i;
	}
	return addr_ind;
}

// Sends nRF24 packet
void nRF24_transmit(uint8_t* data, int len, uint8_t* addr) {
	// Make sure to power down & up before sending
	// This seems device dependent (have not seen others have this issue)
	radio.stopListening();
	radio.powerDown();
	vTaskDelay(pdMS_TO_TICKS(200));
	radio.powerUp();

	radio.openWritingPipe(addr);
	radio.write(data, len, true);

	radio.toggleAllPipes(false);
	radio.openReadingPipe(0, network_address);
	radio.openReadingPipe(1, device_address);
	radio.startListening();

	// If configured, print packet
	if(print_outgoing_packet) {
		printf("SENT PACKET: ");
		print_bytes(data, len);
		printf("TO ADDRESS: ");
		print_bytes(addr, ADDR_LEN);
	}

	// If configured, flash LED
	if(comm_led_flash) {
		uint8_t pcf_byte = read_byte_pcf();
		if(equal_addr(addr, (uint8_t *) network_address))
			write_byte_pcf(pcf_byte & led1);
		else
			write_byte_pcf(pcf_byte & led3);
		vTaskDelay(pdMS_TO_TICKS(200));
		write_byte_pcf(clr_all);
	}
}


/*
** MAIN DSDV FUNCTIONS
*/

// Forwards data packet according to routing table
// Takes mutex lock for routing table access
bool forward_data(uint8_t* data, int len, uint8_t* destination) {
	if(equal_addr(destination, device_address)) {
		if(verbose)
			printf("forward_data: packet addressed to device.\n");

		for(int i = 0; i < len; i++)
			dataRecv[i] = data[i];
		dataLen = len;
		
		xSemaphoreGive(semphr_trgt_packet);
		return true;
	}

	if(len + ADDR_LEN + 1 > MSG_LEN) {
		if(verbose)
			printf("forward_data: packet cannot be routed (too long).\n");
		return false;
	}

	BaseType_t mutex_available = xSemaphoreTake(semphr_dsdv_table, (TickType_t) 10);
	while(!mutex_available) {
		// Sleep for 200 ms
		vTaskDelay(pdMS_TO_TICKS(200));
		mutex_available = xSemaphoreTake(semphr_dsdv_table, (TickType_t) 10);
	}

	int route_ind = addr_index(destination);
	if(route_ind == -1) {
		if(verbose)
			printf("forward_data: packet cannot be routed (no target).\n");

		xSemaphoreGive(semphr_dsdv_table);
		return false;

	} else {
		if(verbose)
			printf("forward_data: routing packet according to table.\n");

		// Format packet as: destination addr, length, content
		for(int i = 0; i < ADDR_LEN; i++)
			forwardSend[i] = destination[i];
		forwardSend[ADDR_LEN] = (uint8_t) len;
		for(int i = 0; i < len; i++)
			forwardSend[ADDR_LEN + i + 1] = data[i];
		
		nRF24_transmit(forwardSend, ADDR_LEN + len + 1, routing_table[route_ind].next_hop);
		xSemaphoreGive(semphr_dsdv_table);
		return true;
	}

}

void parse_data() {
	if(verbose)
		printf("parse_data: parse packet addressed to device.\n");

	// Read packet as: destination addr, length, content
	uint8_t* dest = new uint8_t[ADDR_LEN];
	for(int i = 0; i < ADDR_LEN; i++)
		dest[i] = forwardRecv[i];
	
	int len = (int) forwardRecv[ADDR_LEN];
	if(len + ADDR_LEN + 1 > MSG_LEN)
		len = MSG_LEN - ADDR_LEN - 1;

	uint8_t* data = new uint8_t[len];
	for(int i = 0; i < len; i++)
		data[i] = forwardRecv[ADDR_LEN + i + 1];
	
	if(equal_addr(dest, device_address)) {
		// If destination, copy data and trigger semaphore
		for(int i = 0; i < len; i++)
			dataRecv[i] = data[i];
		dataLen = len;
		
		xSemaphoreGive(semphr_trgt_packet);
	} else // Else route onwards
		forward_data(data, len, dest);

	free(dest);
	free(data);
}

// This function dumps entire table into network
// Should only be called from mutex lock for routing table access
void full_table_dump() {
	if(verbose)
		printf("full_table_dump: dumping entire routing table.\n");

	// Write own information in first row (for all packets)
	for(int j = 0; j < ADDR_LEN; j++)
		dsdvSend[j] = routing_table[0].destination[j];
	for(int j = 0; j < SQNC_LEN; j++)
		dsdvSend[ADDR_LEN + j] = (uint8_t) (routing_table[0].sequence_number >> 8*j) & 0xFF;
	dsdvSend[ADDR_LEN + SQNC_LEN] = routing_table[0].hops;

	int row = 1;
	for(int i = 1; i < table_size_cur; i++) {
		// The first 3 bytes contain routing destination
		for(int j = 0; j < ADDR_LEN; j++)
			dsdvSend[row*MSG_ROW_LEN + j] = routing_table[i].destination[j];
		// The 4 bytes after contain the sequence number
		for(int j = 0; j < SQNC_LEN; j++)
			dsdvSend[row*MSG_ROW_LEN + ADDR_LEN + j] = (uint8_t) (routing_table[i].sequence_number >> 8*j) & 0xFF;
		// The last byte in row contains number of hops
		dsdvSend[row*MSG_ROW_LEN + ADDR_LEN + SQNC_LEN] = routing_table[i].hops;
		routing_table[i].modified = false;

		row++;
		if(row == ROWS_PER_MSG) {
			nRF24_transmit(dsdvSend, MSG_LEN, (uint8_t *) network_address);
			row = 1;
		}
	}

	// Send rest of data (if any)
	if(row > 1) {
		// Insert dummy rows with network address for unused space
		for(int i = row; i < ROWS_PER_MSG; i++) {
			for(int j = 0; j < ADDR_LEN; j++)
				dsdvSend[i*MSG_ROW_LEN + j] = network_address[j];
		}
		nRF24_transmit(dsdvSend, MSG_LEN, (uint8_t *) network_address);
	}
		
	// Update timer
	last_dump = xTaskGetTickCount();

	// Update own entry
	routing_table[0].sequence_number += 2;
	routing_table[0].last_rcvd = last_dump;
}

// This function checks table for dead entries
// Should only be called from mutex lock for routing table access
void check_table() {
	if(verbose)
		printf("check_table: checking table for dead entries.\n");

	TickType_t current_time = xTaskGetTickCount();
	for(int i = 1; i < table_size_cur; i++) {
		// If the entry is already dead (odd sequence number), leave it alone
		if(routing_table[i].sequence_number % 2 != 1 && current_time - routing_table[i].last_rcvd > pdMS_TO_TICKS(TIMEOUT * 1000)) {
			routing_table[i].sequence_number++;
			routing_table[i].hops = 255;
			routing_table[i].modified = true;
		}
	}

	// If entry has been dead for a while, stage it for removal
	int to_remove = 0;
	while(to_remove != -1) {
		to_remove = -1;
		for(int i = 1; i < table_size_cur; i++) {
			if(current_time - routing_table[i].last_rcvd > pdMS_TO_TICKS(ENTRY_DELETE * 1000))
				to_remove = i;
		}

		// Remove dead row if found
		if(to_remove != -1) {
			for(int i = to_remove; i < table_size_cur - 1; i++)
				routing_table[i] = routing_table[i+1];
			table_size_cur--;
		}
	}

	// Update timer
	last_check = xTaskGetTickCount();
}

// This function prepares data for incremental table broadcast and schedules transmission
// Takes mutex lock for routing table access
void brcst_route_info(TimerHandle_t xTimer) {
	BaseType_t mutex_available = xSemaphoreTake(semphr_dsdv_table, (TickType_t) 10);
	while(!mutex_available) {
		// Sleep for 200 ms
		vTaskDelay(pdMS_TO_TICKS(200));
		mutex_available = xSemaphoreTake(semphr_dsdv_table, (TickType_t) 10);
	}

	if(verbose)
		printf("brcst_route_info: broadcasting routing information.\n");

	// Check number of rows to send
	int updated_rows = 0;
	for(int i = 0; i < table_size_cur; i++) {
		if(routing_table[i].modified)
			updated_rows++;
	}

	TickType_t current_time = xTaskGetTickCount();

	// If enough time has passed, check table for dead entries
	if(current_time - last_check > pdMS_TO_TICKS(CHECK_INTERVAL * 1000))
		check_table();

	// If updates cannot be sent in a single message, or enough time has passed, dump entire table
	if(updated_rows > ROWS_PER_MSG || current_time - last_dump > pdMS_TO_TICKS(DUMP_INTERVAL * 1000))
		full_table_dump();

	else {
		// Otherwise write rows into packet
		int row = 0;
		for(int i = 0; i < table_size_cur; i++) {
			if(routing_table[i].modified) {
				// The first 3 bytes of each row entry contain routing destination
				for(int j = 0; j < ADDR_LEN; j++)
					dsdvSend[row*MSG_ROW_LEN + j] = routing_table[i].destination[j];

				// The 4 bytes after destination contain the sequence number
				for(int j = 0; j < SQNC_LEN; j++)
					dsdvSend[row*MSG_ROW_LEN + ADDR_LEN + j] = (uint8_t) (routing_table[i].sequence_number >> 8*j) & 0xFF;

				// The last byte in row contains number of hops
				dsdvSend[row*MSG_ROW_LEN + ADDR_LEN + SQNC_LEN] = routing_table[i].hops;

				row++;
			}
		}

		// Insert dummy rows with network address for unused space
		for(int i = row; i < ROWS_PER_MSG; i++) {
			for(int j = 0; j < ADDR_LEN; j++)
				dsdvSend[i*MSG_ROW_LEN + j] = network_address[j];
		}

		// Send packet
		nRF24_transmit(dsdvSend, MSG_LEN, (uint8_t *) network_address);

		// Update own entry
		routing_table[0].sequence_number += 2;
		routing_table[0].last_rcvd = xTaskGetTickCount();
	}

	xSemaphoreGive(semphr_dsdv_table);
}

// This function updates routing table based on data in update table
// Takes mutex lock for routing table access
void update_table() {
	BaseType_t mutex_available = xSemaphoreTake(semphr_dsdv_table, (TickType_t) 10);
	while(!mutex_available) {
		// Sleep for 200 ms
		vTaskDelay(pdMS_TO_TICKS(200));
		mutex_available = xSemaphoreTake(semphr_dsdv_table, (TickType_t) 10);
	}

	if(verbose)
		printf("update_table: updating routing table.\n");

	for(int i = 0; i < ROWS_PER_MSG; i++) {
		// If the address is equal to network address or own address, discard it
		if(equal_addr(update_data[i].destination, (uint8_t *) network_address) ||
		   equal_addr(update_data[i].destination, (uint8_t *) device_address))
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
			routing_table[table_size_cur].hops = update_data[i].hops;
			routing_table[table_size_cur].sequence_number = update_data[i].sequence_number;
			routing_table[table_size_cur].modified = true;
			routing_table[table_size_cur].last_rcvd = last_rcvd;

			table_size_cur++;

		} else {
			// Update row if sequence number is greater or equal with fewer hops
			if(update_data[i].sequence_number > routing_table[addr_ind].sequence_number ||
			   		(update_data[i].sequence_number == routing_table[addr_ind].sequence_number && 
			   		 update_data[i].hops < routing_table[addr_ind].hops)	) {
				
				routing_table[addr_ind].sequence_number = update_data[i].sequence_number;
				routing_table[addr_ind].last_rcvd = last_rcvd;

				if(routing_table[addr_ind].hops != update_data[i].hops) {
					routing_table[addr_ind].hops = update_data[i].hops;
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

	xSemaphoreGive(semphr_dsdv_table);
}

// This function reads incoming DSDV packets into update table
// Starts processing dsdvRecv whenever semphr_dsdv_packet is available
void parse_dsdv_packet(void *pvParameters) {
	if(verbose)
		printf("parse_dsdv_packet: parses packet into update table.\n");

	while (1) {
		if(xSemaphoreTake(semphr_dsdv_packet, (TickType_t) 10) == pdTRUE) {
			// The packet contains 4 entries of 8 bytes, the first being the sender
			for(int i = 0; i < ROWS_PER_MSG; i++) {
		
				// The first 3 bytes of each row entry contain routing destination
				for(int j = 0; j < ADDR_LEN; j++)
					update_data[i].destination[j] = dsdvRecv[i*MSG_ROW_LEN + j];
		
				// If the address is equal to network address, discard it
				if(equal_addr(update_data[i].destination, (uint8_t *) network_address) ||
				   equal_addr(update_data[i].destination, (uint8_t *) device_address))
					continue;

				// Otherwise, copy rest of data
				// The first 3 bytes of first row contain senders address
				for(int j = 0; j < ADDR_LEN; j++)
					update_data[i].source[j] = dsdvRecv[j];
		
				// The 4 bytes after destination contain the sequence number
				update_data[i].sequence_number = 0;
				for(int j = 0; j < SQNC_LEN; j++)
					update_data[i].sequence_number |= (((uint32_t) dsdvRecv[i*MSG_ROW_LEN + ADDR_LEN + j]) << 8*j);

				// The last byte in row contains number of hops
				update_data[i].hops = dsdvRecv[i*MSG_ROW_LEN + ADDR_LEN + SQNC_LEN];
				if(update_data[i].hops < 255)
					update_data[i].hops += 1;
			}

			// Update table info
			update_table();
		}

		// Sleep for 200 ms
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}


/*
** nRF24 RADIO LOOP AND INITIALIZATION
*/


// Listen for packets
void nRF24_listen(void *pvParameters) {
	if(verbose)
		printf("nRF24_listen: listening for incoming packets.\n");

	uint8_t pipeNum;

	while (1) {
		if (radio.available(&pipeNum)) {

			if(pipeNum == 0) { 
				// received broadcast info
				radio.read(dsdvRecv, MSG_LEN);
				last_rcvd = xTaskGetTickCount();

				// If configured, print packet
				if(print_incoming_packet) {
					printf("RCVD DSDV PACKET: ");
					print_bytes(dsdvRecv, MSG_LEN);
				}

				// If configured, flash LED
				if(comm_led_flash) {
					uint8_t pcf_byte = read_byte_pcf();
					write_byte_pcf(pcf_byte & led2);
					vTaskDelay(pdMS_TO_TICKS(200));
					write_byte_pcf(clr_all);
				}

				xSemaphoreGive(semphr_dsdv_packet);

			} else if(pipeNum == 1) {
				// received data packet
				radio.read(forwardRecv, MSG_LEN);

				// If configured, print packet
				if(print_incoming_packet) {
					printf("RCVD DATA PACKET: ");
					print_bytes(forwardRecv, MSG_LEN);
				}

				// If configured, flash LED
				if(comm_led_flash) {
					uint8_t pcf_byte = read_byte_pcf();
					write_byte_pcf(pcf_byte & led4);
					vTaskDelay(pdMS_TO_TICKS(200));
					write_byte_pcf(clr_all);
				}
				
				parse_data();

			} else {
				radio.read(trash, MSG_LEN);

				// If configured, print packet
				if(print_incoming_packet) {
					printf("RCVD ???? PACKET (pipe %d): ", pipeNum);
					print_bytes(trash, MSG_LEN);
				}
			}
		}

		// sleep for 200 ms
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

// Initializes device and network for DSDV protocol
void DSDV_init(uint8_t* local_address) {
	if(verbose)
		printf("DSDV_init: initalizing DSDV protocol.\n");
	
	uart_set_baud(0, 115200);
	gpio_enable(SCL, GPIO_OUTPUT);
	gpio_enable(CS_NRF, GPIO_OUTPUT);

	// Copy device address
	for (int i = 0; i < ADDR_LEN; i++)
		device_address[i] = local_address[i];

	// Set broadcast and dump timers
	last_brcst = xTaskGetTickCount();
	last_dump = last_brcst;
	last_check = last_brcst;

	// Initialize data fields & tables
	dsdvRecv = new uint8_t[MSG_LEN];
	dsdvSend = new uint8_t[MSG_LEN];
	forwardRecv = new uint8_t[MSG_LEN];
	forwardSend = new uint8_t[MSG_LEN];
	dataRecv = new uint8_t[MSG_LEN - ADDR_LEN - 1];
	trash = new uint8_t[MSG_LEN];

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
	radio.setAddressWidth(ADDR_LEN);
	radio.setPayloadSize(MSG_LEN);
	radio.setDataRate(RF24_1MBPS);
	radio.setAutoAck(false);
	
	radio.toggleAllPipes(false);
    radio.openReadingPipe(0, network_address);
	radio.openReadingPipe(1, device_address);
	radio.startListening();

	// Start listening for incoming packets
	xTaskCreate(nRF24_listen, "nRF24_listen", 1024, NULL, 2, NULL);

	// Create semaphor for packet parsing and start task
	xTaskCreate(parse_dsdv_packet, "parse_dsdv_packet", 1024, NULL, 4, NULL);
	semphr_dsdv_packet = xSemaphoreCreateBinary();
	semphr_dsdv_table = xSemaphoreCreateMutex();
	semphr_trgt_packet = xSemaphoreCreateBinary();

	// Start broadcasting route info on a timer
	TimerHandle_t brcst_timer;
	brcst_timer = xTimerCreate("BRCST TIMER", pdMS_TO_TICKS(1000 * BRCST_INTERVAL), pdTRUE, NULL, brcst_route_info);
	xTimerStart(brcst_timer, 0);

}


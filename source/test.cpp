#include "dsdv.h"
#include "i2c/i2c.h"
# include "espressif/esp_wifi.h"

#define PRINT_INTERVAL	10

#define CS_NRF		0
#define BUS_I2C		0
#define PCF_ADDRESS	0x38
#define SCL 		14
#define SDA 		12

#define button1		0x20	// 0b ??0? ????
#define button2		0x10	// 0b ???0 ????
#define button3		0x80	// 0b 0??? ????
#define button4		0x40	// 0b ?0?? ????
#define clr_all		0xff

// Prints table every 2s
void table_print_loop(void *pvParameters) {
    while (1) {
        print_table();
	    vTaskDelay(pdMS_TO_TICKS(PRINT_INTERVAL * 1000));
    }
}

uint8_t* create_packetA() {
	routing_row* fake_table = (routing_row*) malloc(4 * sizeof(routing_row));
	fake_table[0].destination[0] = 0x00;
	fake_table[0].destination[1] = 0x00;
	fake_table[0].destination[2] = 0x0A;
	fake_table[0].hops = 0;
	fake_table[0].sequence_number = 22;

	fake_table[1].destination[0] = 0x00;
	fake_table[1].destination[1] = 0x00;
	fake_table[1].destination[2] = 0x0B;
	fake_table[1].hops = 1;
	fake_table[1].sequence_number = 20;

	fake_table[2].destination[0] = 0x00;
	fake_table[2].destination[1] = 0x00;
	fake_table[2].destination[2] = 0x6A;
	fake_table[2].hops = 4;
	fake_table[2].sequence_number = 38;

	fake_table[3].destination[0] = 0x00;
	fake_table[3].destination[1] = 0x00;
	fake_table[3].destination[2] = 0x6B;
	fake_table[3].hops = 1;
	fake_table[3].sequence_number = 58;

	uint8_t* fake_packet = new uint8_t[MSG_LEN];
	for(int i = 0; i < 4; i++) {
		for(int j = 0; j < ADDR_LEN; j++)
			fake_packet[i*MSG_ROW_LEN + j] = fake_table[i].destination[j];

		for(int j = 0; j < SQNC_LEN; j++)
			fake_packet[i*MSG_ROW_LEN + ADDR_LEN + j] = (uint8_t) (fake_table[i].sequence_number >> 8*j) & 0xFF;

		fake_packet[i*MSG_ROW_LEN + ADDR_LEN + SQNC_LEN] = fake_table[i].hops;
	}

	return fake_packet;
}

uint8_t* create_packetB() {
	routing_row* fake_table = (routing_row*) malloc(4 * sizeof(routing_row));
	fake_table[0].destination[0] = 0x00;
	fake_table[0].destination[1] = 0x00;
	fake_table[0].destination[2] = 0x0B;
	fake_table[0].hops = 0;
	fake_table[0].sequence_number = 20;

	fake_table[1].destination[0] = 0x00;
	fake_table[1].destination[1] = 0x00;
	fake_table[1].destination[2] = 0x0A;
	fake_table[1].hops = 1;
	fake_table[1].sequence_number = 22;

	fake_table[2].destination[0] = 0x00;
	fake_table[2].destination[1] = 0x00;
	fake_table[2].destination[2] = 0x6A;
	fake_table[2].hops = 3;
	fake_table[2].sequence_number = 38;

	fake_table[3].destination[0] = 0x00;
	fake_table[3].destination[1] = 0x00;
	fake_table[3].destination[2] = 0x6C;
	fake_table[3].hops = 5;
	fake_table[3].sequence_number = 68;

	uint8_t* fake_packet = new uint8_t[MSG_LEN];
	for(int i = 0; i < 4; i++) {
		for(int j = 0; j < ADDR_LEN; j++)
			fake_packet[i*MSG_ROW_LEN + j] = fake_table[i].destination[j];

		for(int j = 0; j < SQNC_LEN; j++)
			fake_packet[i*MSG_ROW_LEN + ADDR_LEN + j] = (uint8_t) (fake_table[i].sequence_number >> 8*j) & 0xFF;

		fake_packet[i*MSG_ROW_LEN + ADDR_LEN + SQNC_LEN] = fake_table[i].hops;
	}

	return fake_packet;
}

// write byte to PCF on I2C bus
static inline void write_byte_pcf(uint8_t data) {

	// disable radio
	gpio_write(CS_NRF, 1);
	// reinitialize i2c
	i2c_init(BUS_I2C, SCL, SDA, I2C_FREQ_100K);
	// write data byte
	i2c_slave_write(BUS_I2C, PCF_ADDRESS, NULL, &data, 1);
}

// read byte from PCF on I2C bus
static inline uint8_t read_byte_pcf() {
	uint8_t data;

	// disable radio
	gpio_write(CS_NRF, 1);
	// reinitialize i2c
	i2c_init(BUS_I2C, SCL, SDA, I2C_FREQ_100K);
	// read data byte
	i2c_slave_read(BUS_I2C, PCF_ADDRESS, NULL, &data, 1);

	return data;
}

void button_task(void *pvParameters) {

	uint8_t* fake_packetA = create_packetA();
	uint8_t* fake_packetB = create_packetB();
	uint8_t pcf_byte;

	while (1) {
		pcf_byte = read_byte_pcf();

		// button 1 is pressed
		if ((pcf_byte & button1) == 0) {
			for(int i = 0; i < MSG_LEN; i++)
				dsdvRecv[i] = fake_packetA[i];

			xSemaphoreGive(semphr_dsdv_packet);
			write_byte_pcf(clr_all);

		// button 2 is pressed
		} else if ((pcf_byte & button2) == 0) {
			for(int i = 0; i < MSG_LEN; i++)
				dsdvRecv[i] = fake_packetB[i];

			xSemaphoreGive(semphr_dsdv_packet);
			write_byte_pcf(clr_all);
		}

		// check again after 200 ms
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

static const uint8_t * get_my_id(void) {
	// Use MAC address for Station as unique ID
	static uint8_t my_id[13];
	static bool my_id_done = false;
	int8_t i;
	uint8_t x;
	if (my_id_done)
		return my_id;
	if (!sdk_wifi_get_macaddr(STATION_IF, my_id))
		return NULL;
	for (i = 5; i >= 0; --i) {
		x = my_id[i] & 0x0F;
		if (x > 9)
			x += 7;
		my_id[i * 2 + 1] = x + '0';
		x = my_id[i] >> 4;
		if (x > 9)
			x += 7;
		my_id[i * 2] = x + '0';
	}
	my_id[12] = '\0';
	my_id_done = true;
	return my_id;
}


extern "C" void user_init(void) {
	// TODO: determine device address
	uint8_t local_addr[ADDR_LEN];
	for(int i=0; i < ADDR_LEN; i++)
		local_addr[i] = 0;
	
	// Turn on verbose mode
	print_incoming_packet = true;
	print_outgoing_packet = true;

	//If enabled, application prints every core function invocation; can be reconfigured dynamically
	verbose = true;
	
	// Initialize DSDV protocol
    DSDV_init(local_addr);

	// Start periodically printing routing table
	xTaskCreate(table_print_loop, "tbl_prnt_loop", 1024, NULL, 2, NULL);

	// Start monitoring button presses
	xTaskCreate(button_task, "button_task", 1024, NULL, 4, NULL);

}


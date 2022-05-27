#include "dsdv.h"

#include <string.h>

#include "espressif/esp_wifi.h"

#define PRINT_INTERVAL	10

#define CS_NRF		0
#define SCL 		14

const uint8_t target_address[] = {0xCF, 0xED, 0xB2};

uint8_t messageLen;
uint8_t *message;

uint8_t* create_packet_DSDV() {
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

// This function displays received messages addressed to this device
void receive_packet(void *pvParameters) {
	while (1) {
		if(xSemaphoreTake(semphr_trgt_packet, (TickType_t) 10) == pdTRUE) {
			dataRecv[dataLen] = (uint8_t) '\0';
			printf("Received message: \"%s\"\n", (char*) dataRecv);
		}

		// Sleep for 200 ms
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

void button_task(void *pvParameters) {

	// uint8_t* fake_packet = create_packet_DSDV();
	// Example use:
	// for(int i = 0; i < MSG_LEN; i++)
	//     dsdvRecv[i] = fake_packet[i];
	// xSemaphoreGive(semphr_dsdv_packet);
	
	uint8_t pcf_byte;

	while (1) {
		pcf_byte = read_byte_pcf();

		if ((pcf_byte & button1) == 0) {
			print_table();

			write_byte_pcf(clr_all);

		} else if ((pcf_byte & button2) == 0) {
			forward_data(message, messageLen, (uint8_t*) target_address);

			write_byte_pcf(clr_all);
		} else if ((pcf_byte & button3) == 0) {
			
			write_byte_pcf(clr_all);
		} else if ((pcf_byte & button4) == 0) {

			write_byte_pcf(clr_all);
		}

		// check again after 200 ms
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}


extern "C" void user_init(void) {
	// Get MAC address
	uint8_t MAC_addr[6];
	sdk_wifi_get_macaddr(STATION_IF, MAC_addr);
	// Disable auto-connect just in case
	sdk_wifi_station_set_auto_connect(0);

	// Use every second byte of MAC as local address
	uint8_t local_addr[ADDR_LEN];
	for(int i=0; i < ADDR_LEN; i++)
		local_addr[i] = MAC_addr[2*i+1];
	
	// Turn on verbose mode
	print_incoming_packet = true;
	print_outgoing_packet = true;

	// Turns on LED flashing
	comm_led_flash = true;

	//prints function invocation
	verbose = true;
	
	// Initialize DSDV protocol
    DSDV_init(local_addr);

	// Prepare message
	messageLen = 16;
	message = (uint8_t *) malloc(16*sizeof(uint8_t));
	char* tmp = (char *) malloc(3*sizeof(uint8_t));
	message[0] = (uint8_t) '\0';
	strcat((char *) message, "This is ");
	for(int i = 0; i < 3; i++) {
		sprintf(tmp, "%02X", local_addr[i]);
		strcat((char *) message, tmp);
	}
	strcat((char *) message, ".");

	// Start monitoring button presses
	xTaskCreate(button_task, "button_task", 1024, NULL, 3, NULL);

	// Start listening for messages
	xTaskCreate(receive_packet, "receive_packet", 1024, NULL, 3, NULL);

	// TODO: light leds when sending & receiving messages

}


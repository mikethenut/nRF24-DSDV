#include <stdint.h>
#include <stdio.h>

// device and network configuration
#define SCL 		14
#define CE_NRF		3
#define CS_NRF		0
#define MSG_LEN		32

const uint8_t channel = 78;
const uint8_t network_address[] = {0x6E, 0x52, 0x46, 0x32, 0x34};
uint8_t device_id[5];

static RF24 radio(CE_NRF, CS_NRF);
uint8_t dataRecv[MSG_LEN];
uint8_t dataSend[MSG_LEN];

// send data
void nRF24_transmit() {
	radio.stopListening();
	bool ackn = radio.write(&dataSend, sizeof(dataSend));
	radio.startListening();

    if (!ackn)
        printf("Message not acknowledged.");
}

// listen for data
void nRF24_listen(void *pvParameters) {
	while (1) {
		if (radio.available())
			radio.read(&dataRecv, sizeof(dataRecv));

		// sleep for 200 ms
		radio.powerDown();
		vTaskDelay(pdMS_TO_TICKS(200));
		radio.powerUp();
	}
}

extern "C" void user_init(void) {
	uart_set_baud(0, 115200);
	gpio_enable(SCL, GPIO_OUTPUT);
	gpio_enable(CS_NRF, GPIO_OUTPUT);

	// TODO: Set unique device local id
	for (int i = 0; i < 5; i++) {
		device_id[i] = 0x00;
	}

	// radio configuration
	radio.begin();
	radio.setChannel(channel);
	radio.openWritingPipe(network_address);
    radio.openReadingPipe(1, network_address);
	radio.startListening();

	xTaskCreate(nRF24_listen, "nRF24_listen", 1024, NULL, 1, NULL);
}

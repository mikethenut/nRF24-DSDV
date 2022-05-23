#include "dsdv.h"

// Prints table every so often
void nRF24_transmit(void *pvParameters) {
    while (1) {
        xTaskCreate(print_table, "print_table", 1024, NULL, 6, NULL);
		vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

extern "C" void user_init(void) {
	xTaskCreate(DSDV_init, "DSDV_init", 1024, NULL, 1, NULL);
    xTaskCreate(nRF24_transmit, "nRF24_transmit", 1024, NULL, 1, NULL);
}

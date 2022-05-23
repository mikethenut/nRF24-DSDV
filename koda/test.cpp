#include "dsdv.h"

#define PRINT_INTERVAL	10


// Prints table every 2s
void table_print_loop(void *pvParameters) {
    while (1) {
        print_table();
	    vTaskDelay(pdMS_TO_TICKS(PRINT_INTERVAL * 1000));
    }
}

extern "C" void user_init(void) {
    DSDV_init();
	xTaskCreate(table_print_loop, "tbl_prnt_loop", 1024, NULL, 2, NULL);
}


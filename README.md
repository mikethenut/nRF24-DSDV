# DSDV Algorithm for nRF24 Module

This repository contains project code implementing the DSDV algorithm for the wireless communication module nRF24L01 in the FreeRTOS environment. It was created during the Wireless Communication Networks course in 2021/2022, under prof. dr. Nikolaj Zimic, dr. Miha Janež and Miran Koprivec, a part of the Master's degree programme at FRI, UL.

## Repository Structure

The topic chosen was 'Create a graph of neighbours and routing algorithm for data transfer using module nRF24L01'. The repository contains three pdf files: *Porocilo.pdf* (report), *Predstavitev.pdf* (presentation slides) and *Demonstracija.pdf* (demonstration experiment report). All are written in Slovene. The source code files are under */source* and include *dsdv.cpp* (DSDV algorithm), *dsdv.h* (algorithm header), *test.cpp* (simple example application), *Makefile* (to make example) and *FreeRTOSConfig.h* (custom config file).

The code is commented and can be inspected for implementation details. Here the source code structure (*dsdv.cpp*) is described together with application configuration and usage (*dsdv.h*). The included example (*test.cpp*) and additional sources are also detailed.

## Algorithm Structure

The algorithm works by storing entries for each known device in the network. Each entry contains the target address, next hop address, number of hops, a sequence number of entry, whether the entry has been modified, and time since last update. This information is broadcast to nearby devices at user specified intervals, and also monitored internally. All devices use a single network address to coordinate routing information, and also individual addresses to route actual data packets sent by the user. The following is a rough diagram of the program drawn on [Canva](www.canva.com/). The items in red can be accessed by an outside application.

![structure](C:\Users\mihae\BSO_projekt\structure.png)

1. Upon calling **DSDV_init**, the algorithm (and device) is initialized. The function creates task **nRF24_listen** (priority 2), which monitors incoming traffic, and sets up a timer that periodically triggers task **brcst_route_info** (priority 6). The task **parse_dsdv_packet** (priority 4), which is activated using a semaphor, is also created.
2. The task **brcst_route_info** periodically broadcasts modified entries in the routing table. If enough time has passed, it calls **check_table**, which checks the table for unresponsive devices. Under certain conditions it will broadcast the entire routing table by calling **full_table_dump**. Both of these use **nRF24_transmit** to send out the data.
3. The task **nRF24_listen** listens for packets. When it receives routing information, it triggers a **semaphor** that activates **parse_dsdv_packet**. The semaphor can also be triggered by the user to simulate receiving such data. This function parses the incoming packet and then calls **update_table** to update the routing table. When a packet addressed to the device is received, **parse_data** is called instead.
4. The function **parse_data** first determines the target address of the packet. If the target is this device, it triggers a **semaphor** that can then be used by the user application to process the received data. Otherwise, it calls **forward_data** that tries to send the packet to the next hop using **nRF24_transmit**. This function is also accessible to the user application to send packets.

## Configuration & Manual

### Basic usage

The following variables can be configured statically inside *dsdv.h*:

- **const uint8_t channel**: Channel for nRF24. [This link](http://www.arduinoetal.net/?q=node/89) recommends channels between 70 and 80.
- **const uint8_t network_address[]**: Address shared between all devices in network.
- **INCLUDE_I2C_BUS_SUPPORT**: Set to 1 if you want to use I2C bus, 0 otherwise.
- **TABLE_SIZE_INIT**: Initial routing table size. 
- **BRCST_INTERVAL**: How often modified routing table rows should be broadcast.
- **DUMP_INTERVAL**: How often the entire routing table should be broadcast.
- **CHECK_INTERVAL**: How often the routing table should be checked for dead entries.
- **TIMEOUT**: How long after last communication is a device considered dead.
- **ENTRY_DELETE**: How long after a row is determined dead it is removed.

The following data fields can be configured or accessed dynamically during execution:

- **bool print_incoming_packet**: Can be enabled to print all incoming traffic to stdout.
- **bool print_outgoing_packet**: Can be enabled to print all outgoing traffic to stdout.
- **bool comm_led_flash**: Can be enabled to blink LEDs in response to traffic. INCLUDE_I2C_BUS_SUPPORT must be set to 1.
- **bool verbose**: Can be enabled to print invocation of certain internal functions.
- **SemaphoreHandle_t semphr_trgt_packet**: Is activated when device receives data that targets it. User application should listen for it and read the data from dataRecv.
- **uint8_t* dataRecv**: Stores incoming data that targets this device. It is not access controlled.
- **int dataLen**: Length of data stored in dataRecv.

The following functions can be accessed by the user application:

- **void DSDV_init(uint8_t* local_address)**: Initializes the protocol. Should be called first by the user application.
- **bool forward_data(uint8_t* data, int len, uint8_t* destination)**: Tries to forward len bytes stored in data to the destination according to routing table information. Returns false upon failure.
- **const uint8_t* get_network_address()**: Returns network address (getter function).
- **uint8_t* get_device_address() **: Returns device address (getter function).
- **uint8_t get_rtable_size()**: Returns size of routing table.
- **uint8_t get_utable_size()**: Returns size of update table.
- **void print_table()**: Prints entire routing table to stdout.
- **void print_bytes(uint8_t* data, int len)**: Prints len bytes from data as hex nibbles to stdout, followed by a new line.
- **void write_byte_pcf(uint8_t data)**: Writes a byte to PCF on I2C bus. INCLUDE_I2C_BUS_SUPPORT must be set to 1.
- **uint8_t read_byte_pcf()**: Reads a byte from PCF on I2C bus. INCLUDE_I2C_BUS_SUPPORT must be set to 1.

### Advanced usage

The following variables can be configured statically inside *dsdv.h*:

- **SCL**: SCL pin number.
- **CE_NRF**: CE NRF pin number.
- **CS_NRF**: CS NRF pin number.
- **MSG_LEN**: Number of bytes in a single nRF24 packet.
- **ROWS_PER_MSG**: Number of routing table rows sent in a single nRF24 packet.
- **ADDR_LEN**: Number of bytes used for addressing devices (3-5).
- **SQNC_LEN**: Number of bytes used to store sequence number. Note that (SQNC_LEN + ADDR_LEN + 1)*ROWS_PER_MSG should be less or equal to MSG_LEN. Otherwise the implementation must be adjusted. 

The following data fields can be configured or accessed dynamically during execution:

- **SemaphoreHandle_t semphr_dsdv_packet**: Is activated when device receives DSDV routing packet. User application can activate it to manipulate the algorithm.
- **uint8_t* dsdvRecv**: Stores incoming DSDV routing packets. It is not access controlled.
- **uint8_t* dsdvSend**: Stores outgoing DSDV routing packets. It is not access controlled.
- **SemaphoreHandle_t semphr_dsdv_table**: Mutex semaphor that controls access to routing table.
- **update_row* update_data**: Update table used as intermediate between incoming DSDV packets and routing table. It is not access controlled.
- **routing_row* routing_table**: Routing table that stores all DSDV routing information. Any changes should be done either by storing data into dsdvRecv and activating semphr_dsdv_packet, or after taking mutex semphr_dsdv_table.

## Description of Example

The example in *test.cpp* first determines its own local address. It enables printing of all network traffic and core function invocations to standard output, as well as blinking of LED lights when receiving or sending packets. Then it initializes the DSDV protocol with its address and prepares the message to route. It launches a task that listens for the semaphor announcing messages addressed to the device, and prints the message to standard output accordingly. It launches also a task that listens for button presses. The first button prints the routing table to standard output, while the second button routes the prepared message to the hard-coded destination. The third button copies fake information into the field for received DSDV packets and triggers the semaphor that tells the protocol to process it. The fourth button doesn't do anything.

## Sources & Reading

- An older version of the [nRF24 library](https://github.com/nRF24/RF24) and its [documentation](https://nrf24.github.io/RF24/index.html) were used to operate the nRF24L01 module. The exact sources used can be provided upon request.
- This list of [FreeRTOS crashes](https://github.com/SuperHouse/esp-open-rtos/wiki/Crash-Dumps) is useful for debugging.
- Some advice on [nRF24 addressing](http://maniacalbits.blogspot.com/2013/04/rf24-addressing-nrf24l01-radios-require.html).
- This issue on [nRF24 pipe 7](https://github.com/nRF24/RF24/issues/394). It was fixed by updating relevant functions in the nRF24 library. Further fix was necessary by powering down the radio briefly when switching between receiving and sending messages.
- Two similar projects by [joshua-jerred](https://github.com/joshua-jerred/DSDV) and [lukefilma](https://github.com/lukeflima/DSDV) were also inspected. Although never used as a direct source, they were convenient for comparing implementation details and might be of use to others interested in the topic.

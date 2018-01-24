/*
 Motion Sensor
 
 Monitors the motion sensor via the controller E931.96. It sends every 4s the state of motion sensor
 and battery voltage via nRF24 transmitter.
 It goes to sleep after sending. The current consumption is about 32uA average. A pair lithium cells of
 AA (3000mAh), should stay about 8 years, depending on the rate of wake ups, maybe only 4 - 5 years.
 */
 
// ATtiny24/44/84 Pin map
//
//               +-\/-+
//         VCC  1|o   |14  GND
//     TxD PB0  2|    |13  PA0 SERIN
//         PB1  3|    |12  PA1 DOCI
//   RESET PB3  4|    |11  PA2
//      CE PB2  5|    |10  PA3
//     CSN PA7  6|    |9   PA4 SCK
//    MISO PA6  7|    |8   PA5 MOSI
//               +----+

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include <util/crc16.h>
#include "mirf.h"
#include "spi.h"

// program boot loops if SERIAL_DEBUG and AES
//#define SERIAL_DEBUG 1
#ifdef SERIAL_DEBUG
#include "uart.h"
#endif

#define WITH_AES 1
#ifdef WITH_AES
#include <AESLib.h>
#endif

// constants for boot_program_page
#define RWWSRE CTPB
#ifdef ASRE
#define __COMMON_ASRE   ASRE
#else
#define __COMMON_ASRE   RWWSRE
#endif

#define TYPE_SENSOR 1 // Motion

// I/O constants
#define CE  PB2
#define CSN PA7
#define REED PA3
#define SERIN PA0
#define DOCI PA1

#define __boot_rww_enable_short()                      \
(__extension__({                                 \
    __asm__ __volatile__                         \
    (                                            \
        "out %0, %1\n\t"                         \
        "spm\n\t"                                \
        :                                        \
        : "i" (_SFR_IO_ADDR(__SPM_REG)),        \
          "r" ((uint8_t)__BOOT_RWW_ENABLE)       \
    );                                           \
}))

void boot_program_page (uint16_t page, uint8_t *buf) BOOTLOADER_SECTION;

void writeConfig (void);
void readConfig (void);
void printConfig (void);

uint16_t adc;
uint16_t v_bat[4];
uint8_t node;

data_payload data_out;
data_payload_init data_in;

//some typedefs for AES
typedef struct {
	uint8_t key[16];
	uint8_t buffer[16];
}aes_as_struct;

typedef union {
	aes_as_struct as_struct;
	uint8_t data[32];
}aes_as_union;

// Data structure for AES key
aes_as_union aes_data = {
		{{0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46},
		{0x01, 0x0, 0x0, 0xe2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x00, 0x0, 0x0}}
};

// Data structure in the flash memory, AESkexy, node and CRC
// Reserved 64 bytes as the flashing is page based
const uint8_t config[64] __attribute__((progmem, aligned (64))) = {
		0x00, 0x00, 0x6e, 0x5a, 0x32, 0x53, 0x62, 0x4b, 0x71, 0x44, 0x4e, 0x4d, 0x37, 0x62, 0x65, 0x00,
		0xff, TYPE_SENSOR, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

////Creating the status structure for the E931.96
typedef struct {
	unsigned test_mode:1;
	unsigned clamp_input:1;
	unsigned size_cap:1;
	unsigned self_test:1;
	unsigned supply_reg_enable:1;
	unsigned source_filter:2;
	unsigned source_irq:1;
	unsigned motion_enable:1;
	unsigned window_time:2;
	unsigned pulse_counter:2;
	unsigned blind_time:4;
	unsigned sensitivity:8;
	unsigned pir_voltage:14;
	unsigned pir_oor:1;
}status_as_struct;

typedef struct {
	uint8_t data[5];
}status_as_array;

typedef union {
	status_as_struct as_struct;
	status_as_array as_array;
} union_status;

//uint64_t
union_status pir_status;

//Creating the config structure for the E931.96
typedef struct {
	uint8_t data[4];
}config_as_array;

typedef struct {
	unsigned user_test:2;
	unsigned size_cap:1;
	unsigned self_test:1;
	unsigned enable_supply_reg:1;
	unsigned source_filter:2;
	unsigned source_irq:1;
	unsigned enable_motion:1;
	unsigned window_time:2;
	unsigned pulse_counter:2;
	unsigned blind_time:4;
	unsigned sensitivity:8;
}config_as_struct;

typedef union {
	config_as_struct as_struct;
	config_as_array as_array;
} union_config;

union_config pir_config;

// Pin change interrupt
ISR (PCINT0_vect)
{
	//To avoid massive interrupts
	cli();
	wdt_disable();  // disable watchdog
}

// watchdog interrupt
ISR (WDT_vect)
{
	wdt_disable();  // disable watchdog
}

void(* resetFunc) (void) = 0;

void resetWatchdog (void)
{
	// clear various "reset" flags
	MCUSR = 0;
	// allow changes, disable reset, clear existing interrupt
	WDTCSR = (1<<WDCE) | (1<<WDE) | (1<<WDIF);
	// set interrupt mode and an interval (WDE must be changed from 1 to 0 here)
	WDTCSR = (1<<WDIE) | (1<<WDP3);    // set WDIE, and 4 seconds delay
	// pat the dog
	wdt_reset();
}  // end of resetWatchdog

void setup (void)
{

	// Copy of 32 bytes flash data to aes_data
	memcpy_P(aes_data.data, config, 32);

	node = aes_data.as_struct.buffer[0];

	spi_init(); // Initialise SPI

	mirf_init();
	_delay_ms(50);
	mirf_config();


	DDRA |= (1<<CSN); // SPI DO
	DDRB |= (1<<CE); // SPI DO

	DDRA &= ~(1<<REED);  //REED DI

	DDRA |= (1<<SERIN); //SERIN DO
	DDRA |= (1<<DOCI);  //DOCI DO

	//Configuration values for the E931.96
	pir_config.as_struct.sensitivity = 20; //20*6.5uV
	pir_config.as_struct.blind_time = 6; //3=1.5s
	pir_config.as_struct.pulse_counter = 2; //=2
	pir_config.as_struct.window_time = 0; //0=4s
	pir_config.as_struct.source_irq = 0; //0
	pir_config.as_struct.enable_motion = 1; //1
	pir_config.as_struct.source_filter = 1;
	pir_config.as_struct.self_test = 0;
	pir_config.as_struct.enable_supply_reg = 0; //0=enable

	writeConfig();
	_delay_ms(1);
	readConfig();
	printConfig();

	// pin change interrupt
	PCMSK0  = (1<<PCINT1);  // want pin PA1 / pin 12
	GIFR  |= (1<<PCIF0);    // clear any outstanding interrupts
	GIMSK |= (1<<PCIE0);    // enable pin change interrupts

	//adc_init();

}  // end of setup

void adc_init(void)
{
	// Set ADC prescaler to 64 - 125KHz sample rate @ 8MHz
	ADCSRA = (1<<ADPS2) | (1<<ADPS1);

	// adc source=1.1 ref; adc ref (base for the 1023 maximum)=Vcc
	ADMUX =  0b00100001;

	// Enable ADC
	ADCSRA |= (1<<ADEN);
}

void goToSleep (void)
{
	set_sleep_mode (SLEEP_MODE_PWR_DOWN);
	ADCSRA = 0;
	cli ();
	resetWatchdog ();
	sleep_enable ();
	sei ();
	sleep_cpu ();
	sleep_disable ();
}

//Recreate data_out structure
void init_data_out(void)
{
	data_out.as_struct.node = node;
	data_out.as_struct.v_bat = 220;
	data_out.as_struct.type = TYPE_SENSOR;
	data_out.as_struct.info = node;
	data_out.as_struct.closed = 0;
	data_out.as_struct.error = 0;
}


// Flashing a page
void boot_program_page (uint16_t address, uint8_t *buf)
{
	uint16_t i;
	uint8_t sreg;

#ifdef SERIAL_DEBUG
	serial_print ("address: ");
	serial_print_int(address/SPM_PAGESIZE);
	serial_print ("\n\r");
	_delay_ms(2000);
#endif

	// Disable interrupts.
	sreg = SREG;
	cli();

	eeprom_busy_wait ();
	boot_page_erase (address);

	// Wait until the memory is erased.
	boot_spm_busy_wait ();

	// Fill the data block of 64 bytes
	for (i=0; i<32; i+=2)
	{
		uint16_t w = *(buf + i);
		w += *(buf + i + 1) << 8;
		boot_page_fill (address + i, w);
	}
	for (i=32; i<SPM_PAGESIZE; i+=2)
	{
		uint16_t w = 0xffff;
		boot_page_fill (address + i, w);
	}

	eeprom_busy_wait ();
	boot_spm_busy_wait ();

	boot_page_write (address);

	__boot_rww_enable_short();

	// Re-enable interrupts (if they were ever enabled).
	SREG = sreg;

}

//Read the config register of the E931.96
void readConfig (void)
{
	int j, i;

	//serial_print("status: ");

	PORTA &= ~(1<<DOCI);
	_delay_us(128);
	PORTA |=  (1<<DOCI);
	_delay_us(64);

	//loop
	for (j = 4; j >= 0; j--) {
		for (i = 7; i >= 0; i--) {

			PORTA &= ~(1<<DOCI);
			_delay_us(0.2);
			PORTA |=  (1<<DOCI);
			_delay_us(0.2);
			DDRA &= ~(1<<DOCI); //DOCI DI
			PORTA &= ~(1 << DOCI); //Tristate
			_delay_us(0.5);

			if (PINA & (1<<DOCI)) {
				pir_status.as_array.data[j] |= (1<<i);
				//serial_print("1");
			}
			else {
				pir_status.as_array.data[j] &= ~(1<<i);
				//serial_print("0");
			}
			//config_status:j = PINA & (1<<DOCI);
			DDRA |= (1<<DOCI);  //DOCI DO
		}
	}
	//DOCI must be Input to receive Interrupt
	PORTA &= ~(1<<DOCI); //Tristate
	DDRA &= ~(1<<DOCI);  //DOCI DI
}

void ack_int(void) {
	DDRA |= (1<<DOCI);  //DOCI DO
	PORTA &= ~(1<<DOCI); //DOCI to low
	DDRA &= ~(1<<DOCI); //DOCI DI
	PORTA &= ~(1 << DOCI); //Tristate
}

//Read the config register of the E931.96
void writeConfig (void)
{
	int j, i, p;
	p = 0;

	//serial_print("config: ");

	for (j = 3; j >= 0; j--) {

		for (i = p; i >= 0; i--) {


			PORTA |=  (1<<SERIN);
			_delay_us(2);

			if (pir_config.as_array.data[j] & (1<<i)) {
				PORTA |=  (1<<SERIN);
				//serial_print("1");
			} else {
				PORTA &=  ~(1<<SERIN);
				//serial_print("0");
			}
			_delay_us(64);
			PORTA &=  ~(1<<SERIN);
			_delay_us(2);
		}
		p = 7;
	}
	//serial_print("\n\r");
}

//Print the config register of the E931.96
void printConfig (void) {
#ifdef SERIAL_DEBUG
	serial_print("pir voltage: ");
	serial_print_dec((int)pir_status.as_struct.pir_voltage);
	serial_print("\n\r");

	serial_print("motion_enable: ");
	serial_print_dec((int)pir_status.as_struct.motion_enable);
	serial_print("\n\r");

	serial_print("supply_enable: ");
	serial_print_dec((int)pir_status.as_struct.supply_reg_enable);
	serial_print("\n\r");

	serial_print("sensitivity: ");
	serial_print_dec((int)pir_status.as_struct.sensitivity);
	serial_print("\n\r");

	serial_print("blind_time: ");
	serial_print_dec((int)pir_status.as_struct.blind_time);
	serial_print("\n\r");

	serial_print("pulse_counter: ");
	serial_print_dec((int)pir_status.as_struct.pulse_counter);
	serial_print("\n\r");

	serial_print("pir_oor: ");
	serial_print_dec((int)pir_status.as_struct.pir_oor);
	serial_print("\n\r");
#endif
}

int main (void)
{
	int cnt1, cnt2;

	unsigned char ResetSrc = MCUSR;   // save reset source
	MCUSR = 0x00;  // cleared for next reset detection

	setup();

#ifdef SERIAL_DEBUG
	uart_init();

	serial_print("rf24_motion based on mirf\r\n");
#endif

#ifdef SERIAL_DEBUG
	int j;
	serial_print("node: ");
	serial_print_int(node);
	serial_print("\n\r");

	_delay_ms(100);
#endif

	cnt2 = 0;

	// Calculate a CRC value
	data_out.as_crc.crc[0] = node;
	for (cnt1 = 0; cnt1 < 16; cnt1++) {
		data_out.as_crc.crc[cnt1 + 1] = aes_data.as_struct.key[cnt1];
	}

	uint16_t crc = 0;
	for (cnt1 = 0; cnt1 < 17; cnt1++) {
	    crc = _crc_xmodem_update(crc, data_out.as_crc.crc[cnt1]);
	}

	//Recreate data_out structure
	init_data_out();

	uint16_t crc_flash = aes_data.as_struct.buffer[1] + (aes_data.as_struct.buffer[2]<<8);

	//Check if the sensor wasn't initialized yet and if CRC is valid
	//aes_data structure have been copied from flash area in function setup()
	if (aes_data.as_struct.buffer[0] == 0xff) {
		data_out.as_struct.node = 0xff;
		data_out.as_struct.info = 0xff;

#ifdef SERIAL_DEBUG
		serial_print("Uninitialized, force node to 0xff\n\r ");
#endif

	} else if (crc != crc_flash) {
		data_out.as_struct.node = 0xff;
		data_out.as_struct.info = 0xff;
	}

	data_out.as_struct.node = 0xff;

	//Loop forever
	while (1) {

		while (data_out.as_struct.node == 0xff)
		{
			//Leave if it was a soft reset
			if (ResetSrc == 0) {
				//WD_WDP = (1<<WDP2) | (1<<WDP0); // 0.5s
				//WD_cnt = 10;
				data_out.as_struct.node = node;
				break;
			}
			//If was configured before, exit after 5 loops
			if (data_out.as_struct.info != 0xff) {
				if (cnt2 > 4) {
					//WD_WDP = (1<<WDP2) | (1<<WDP0); // 0.5s
					//WD_cnt = 10;
					data_out.as_struct.node = node;
					break;
				}
			}

			cnt1 = 0;

#ifdef SERIAL_DEBUG
			serial_print("node uninitialized: sending payload...\r\n");
#endif

	    	// Power up the nRF24L01
	    	TX_POWERUP;
	    	_delay_ms(3);

	    	// Send the plain data
	    	mirf_transmit_data();

	    	// Change to listening mode

	    	mirf_reconfig_rx();

			mirf_CSN_lo;
			spi_transfer(FLUSH_RX);
			mirf_CSN_hi;

			mirf_CE_hi; // Start listening

			// Wait for incoming requests
			while (!(mirf_status() & (1<<RX_DR))) {
				_delay_us(500);
				cnt1++;
				if (cnt1 > 100) {
					mirf_CE_lo; // Stop listening
#ifdef SERIAL_DEBUG
					serial_print("Timeout...\n\r");
#endif
					break; //break after 50ms
				}
			}
#ifdef SERIAL_DEBUG
			if (cnt1 < 100) serial_print("Data received ...\n\r");
#endif
			if (cnt1 > 100) {
				_delay_ms(100);
				cnt2++;
				continue;
			}

			mirf_CE_lo; // Stop listening

#ifdef SERIAL_DEBUG
			serial_print("Read the data received ...\n\r");
#endif
			// Read the data received
			mirf_receive_data();

			if ((data_in.as_struct.node > 0) & (data_in.as_struct.node < 255)) {
				node = data_in.as_struct.node;


#ifdef SERIAL_DEBUG
				serial_print("got node: ");
				serial_print_int(node);
				serial_print("\n\r");
				serial_print("got key: ");

				for (j = 0; j < 16; j++) {
					//serial_print("0x");
					serial_print_int(data_in.as_struct.key[j]);
					aes_data.as_struct.key[j] = data_in.as_struct.key[j];
					serial_print(" ");
				}
				serial_print("\r\n");

				serial_print("CRC16: 0x");
				serial_print_int(crc);
				serial_print("\r\n");
#endif


				// Calculate CRC16 and save to dat structure pos 17:18
				data_out.as_crc.crc[0] = node;
				for (cnt1 = 0; cnt1 < 16; cnt1++) {
					data_out.as_crc.crc[cnt1 + 1] = data_in.as_struct.key[cnt1];
				}
				uint16_t crc = 0;
				for (cnt1 = 0; cnt1 < 17; cnt1++)
				    crc = _crc_xmodem_update(crc, data_out.as_crc.crc[cnt1]);

				// Check the CRC to avoid false init messages
				if (crc != data_in.as_struct.crc) {

#ifdef SERIAL_DEBUG
					serial_print("Bad CRC, make a restart ...\r\n");
					_delay_ms(20);
#endif

					// Failed, set CRC error (1)
					init_data_out();
					data_out.as_struct.node = 0xff;
					data_out.as_struct.info = node;
					data_out.as_struct.error = 1;

			    	// Power up the nRF24L01
			    	TX_POWERUP;
			    	_delay_ms(3);

			    	// send the plain data
			    	mirf_transmit_data();


					// Do a soft reset
					mirf_config();
					//TX_POWERUP;
					resetFunc();
				}

				// Recreate data_out structure
				init_data_out();

				memcpy( aes_data.as_struct.key, data_in.as_struct.key, 16);

				aes_data.data[16] = data_out.as_data.data[0];
				aes_data.data[17] = (uint8_t)crc;
				aes_data.data[18] = (uint8_t)(crc>>8);

#ifdef SERIAL_DEBUG
				serial_print("aes_data.data:");
				for (j = 0; j < 32; j++) {
					//serial_print("0x");
					serial_print_int(aes_data.data[j]);
					serial_print(" ");
				}
				serial_print("\r\n");
#endif

				// Flash the received node and AES key and CRC
				boot_program_page((uint16_t)config, aes_data.data);

				// Success, send acknowledge (99)
				init_data_out();
				data_out.as_struct.node = 0xff;
				data_out.as_struct.info = node;
				data_out.as_struct.error = 99;

		    	// Power up the nRF24L01
		    	TX_POWERUP;
		    	_delay_ms(3);

		    	// send the plain data
		    	mirf_transmit_data();


				// Do a soft reset
				// mirf_config();
				_delay_ms(3);

				resetFunc();
			}
		}

		// Power down the nRF24L01
		POWERDOWN;

		// And go to sleep
		goToSleep ();

		// The Attiny woke up.

#ifdef SERIAL_DEBUG
		serial_print("waked up...\r\n");
#endif

		adc_init();
		data_out.as_struct.v_bat = 0;

		_delay_ms(2);

    	//Check if motion detected
        if (PINA & (1<<DOCI)) {
    		data_out.as_struct.closed = 1;
    		ack_int();
#ifdef SERIAL_DEBUG
			serial_print("motion detected!");
			serial_print("\n\r");
#endif
    	}
    	else {
    		data_out.as_struct.closed = 0;
#ifdef SERIAL_DEBUG
			serial_print("no motion detected!");
			serial_print("\n\r");
#endif
    	}

    	// Do 4 times measuring and drop the first
        for (cnt1 = 0; cnt1 < 4; cnt1++) {
       	  ADCSRA |= _BV(ADSC);
       	  while((ADCSRA & (1<<ADSC)) !=0);
       	  if (cnt1 == 0) continue;
       	  data_out.as_struct.v_bat += ADC;
        }
        // And calculate the average
        data_out.as_struct.v_bat = data_out.as_struct.v_bat / 3 ;

        // Calculate to mV: vcc = 1100 x 1024 / adc
        data_out.as_struct.v_bat = 1126400L / data_out.as_struct.v_bat;


        //build the crc from first 6 bytes
    	crc = 0;
    	for (cnt1 = 0; cnt1 < 5; cnt1++) {
    	    crc = _crc_xmodem_update(crc, data_out.as_data.data[cnt1]);
    	}

    	data_out.as_struct.crc = crc;

#ifdef WITH_AES
    	// Encryption of the payload
    	memcpy( aes_data.as_struct.buffer, data_out.as_data.data, 16);

    	aes128_enc_single(aes_data.as_struct.key, aes_data.as_struct.buffer);

    	memcpy( data_out.as_data.data, aes_data.as_struct.buffer, 16);
#endif

    	// If it is traffic on the air, do a loop
    	// without transmitting data
		RX_POWERUP;
		_delay_ms(3);
		if (mirf_is_traffic()) {
			_delay_ms(1);
			continue;
		}

    	// Power up the nRF24L01
    	TX_POWERUP;
    	//_delay_ms(3);

    	// Send the encrypted payload
    	mirf_transmit_data();

    	// Recreate data_out structure
    	init_data_out();
	}
}  // end of loop


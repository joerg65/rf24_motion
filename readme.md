RF24 Motion Sensor

Runs on ATtiny84. This is part of my Alarm Central written for Odroid C1 / Android.
Communicates with the ultra low current motion controller E931.96. If a motion is detected or every 4s the state of contact and battery voltage is sent via nRF24.
After sending the cpu goes sleep. The current consumption is about 32uA average. Two lithium cells AA (3000mAh), should stay about 4 to 8 years. The lifetime depends on the amount of detected motion. To ensure the minimum of current during sleeping the Brown Out Detection is switched off.
An unconfigured sensor sends its node FFh to the Central in plain data then changes to receiving mode. The Central does a autonumbering an sends the node and the AES128key to the sensor. The message contains a Xmodem CRC16 field at byte17:18. It is calculated over the first 17 bytes byte0 node and byte1:16 AES128key. The sensor stores this data to its Flash and does a reboot. From now on it sends every 4 seconds, or at pin change of reed contact, node, battery value and state (stillness/motion) AES128 encrypted to the Central.

The basic of the mirf library I found here: https://github.com/MattKunze/avr-playground/tree/master/mirf  
And the AESLib I found here: https://github.com/DavyLandman/AESLib  

The project was made with Eclipse, so if someone want to make it with Eclipse, it need to be added the 'AVR Eclipse Plugin' to Eclipse.  

To import the source code to Eclipse I found the easiest way as this:  

Select File/New/Other and then select C Project and Next. Give Project name and select AVR Cross Target Application and Next. Deselect Debug and the select Finish.
Next, select your new Project with right mouse click and select Import.../Filesystem and browse for the source code folder. Mark in the left window to select everything and select Finish.

It should build now without errors. To build, select Project/Build Project. The output should be:
```
20:13:05 **** Incremental Build of configuration Release for project rf24_motion ****
make all 
Building file: ../main.c
Invoking: AVR Compiler
avr-gcc -I"/home/joerg/Development/eclipse/rf24_motion/aes" -Wall -Os -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -std=gnu99 -funsigned-char -funsigned-bitfields -mmcu=attiny84 -DF_CPU=8000000UL -MMD -MP -MF"main.d" -MT"main.o" -c -o "main.o" "../main.c"
Finished building: ../main.c
 
Building file: ../mirf.c
Invoking: AVR Compiler
avr-gcc -I"/home/joerg/Development/eclipse/rf24_motion/aes" -Wall -Os -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -std=gnu99 -funsigned-char -funsigned-bitfields -mmcu=attiny84 -DF_CPU=8000000UL -MMD -MP -MF"mirf.d" -MT"mirf.o" -c -o "mirf.o" "../mirf.c"
Finished building: ../mirf.c
 
Building file: ../spi.c
Invoking: AVR Compiler
avr-gcc -I"/home/joerg/Development/eclipse/rf24_motion/aes" -Wall -Os -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -std=gnu99 -funsigned-char -funsigned-bitfields -mmcu=attiny84 -DF_CPU=8000000UL -MMD -MP -MF"spi.d" -MT"spi.o" -c -o "spi.o" "../spi.c"
Finished building: ../spi.c
 
Building file: ../uart.c
Invoking: AVR Compiler
avr-gcc -I"/home/joerg/Development/eclipse/rf24_motion/aes" -Wall -Os -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -std=gnu99 -funsigned-char -funsigned-bitfields -mmcu=attiny84 -DF_CPU=8000000UL -MMD -MP -MF"uart.d" -MT"uart.o" -c -o "uart.o" "../uart.c"
Finished building: ../uart.c
 
Building target: rf24_motion.elf
Invoking: AVR C++ Linker
avr-g++ -Wl,-Map,rf24_motion.map,--cref -mrelax -Wl,--gc-sections -mmcu=attiny84 -o "rf24_motion.elf"  ./aes/AESLib.o ./aes/aes_dec-asm_faster.o ./aes/aes_enc-asm.o ./aes/aes_invsbox-asm.o ./aes/aes_keyschedule-asm.o ./aes/aes_sbox-asm.o ./aes/avr-asm-macros.o ./aes/bcal-basic.o ./aes/bcal-cbc.o ./aes/bcal-cmac.o ./aes/bcal-ofb.o ./aes/bcal_aes128.o ./aes/bcal_aes192.o ./aes/bcal_aes256.o ./aes/gf256mul.o ./aes/keysize_descriptor.o ./aes/memxor.o  ./main.o ./mirf.o ./spi.o ./uart.o   
Finished building target: rf24_motion.elf
 
Invoking: AVR Create Extended Listing
avr-objdump -h -S rf24_motion.elf  >"rf24_motion.lss"
Finished building: rf24_motion.lss
 
Create Flash image (ihex format)
avr-objcopy -R .eeprom -R .fuse -R .lock -R .signature -O ihex rf24_motion.elf  "rf24_motion.hex"
Finished building: rf24_motion.hex
 
Create eeprom image (ihex format)
avr-objcopy -j .eeprom --no-change-warnings --change-section-lma .eeprom=0 -O ihex rf24_motion.elf  "rf24_motion.eep"
Finished building: rf24_motion.eep
 
Invoking: Print Size
avr-size --format=avr --mcu=attiny84 rf24_motion.elf
AVR Memory Usage
----------------
Device: attiny84

Program:    3856 bytes (47.1% Full)
(.text + .data + .bootloader)

Data:        125 bytes (24.4% Full)
(.data + .bss + .noinit)


Finished building: sizedummy
 

20:13:05 Build Finished (took 573ms)
```

To flash with avrdude:
```
avrdude -cavrisp2 -P/dev/ttyACM0 -pt84 -Uflash:w:rf24_motion.hex:a -Ulfuse:w:0xe2:m -Uhfuse:w:0xdf:m -Uefuse:w:0xfe:m
```
Of course one need to adjust the '-cavrisp2' to his flashing tool and depending also the port.

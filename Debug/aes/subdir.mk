################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../aes/AESLib.c \
../aes/bcal-basic.c \
../aes/bcal-cbc.c \
../aes/bcal-cmac.c \
../aes/bcal-ofb.c \
../aes/bcal_aes128.c \
../aes/bcal_aes192.c \
../aes/bcal_aes256.c \
../aes/keysize_descriptor.c 

S_UPPER_SRCS += \
../aes/aes_dec-asm_faster.S \
../aes/aes_enc-asm.S \
../aes/aes_invsbox-asm.S \
../aes/aes_keyschedule-asm.S \
../aes/aes_sbox-asm.S \
../aes/avr-asm-macros.S \
../aes/gf256mul.S \
../aes/memxor.S 

C_DEPS += \
./aes/AESLib.d \
./aes/bcal-basic.d \
./aes/bcal-cbc.d \
./aes/bcal-cmac.d \
./aes/bcal-ofb.d \
./aes/bcal_aes128.d \
./aes/bcal_aes192.d \
./aes/bcal_aes256.d \
./aes/keysize_descriptor.d 

OBJS += \
./aes/AESLib.o \
./aes/aes_dec-asm_faster.o \
./aes/aes_enc-asm.o \
./aes/aes_invsbox-asm.o \
./aes/aes_keyschedule-asm.o \
./aes/aes_sbox-asm.o \
./aes/avr-asm-macros.o \
./aes/bcal-basic.o \
./aes/bcal-cbc.o \
./aes/bcal-cmac.o \
./aes/bcal-ofb.o \
./aes/bcal_aes128.o \
./aes/bcal_aes192.o \
./aes/bcal_aes256.o \
./aes/gf256mul.o \
./aes/keysize_descriptor.o \
./aes/memxor.o 

S_UPPER_DEPS += \
./aes/aes_dec-asm_faster.d \
./aes/aes_enc-asm.d \
./aes/aes_invsbox-asm.d \
./aes/aes_keyschedule-asm.d \
./aes/aes_sbox-asm.d \
./aes/avr-asm-macros.d \
./aes/gf256mul.d \
./aes/memxor.d 


# Each subdirectory must supply rules for building sources it contributes
aes/%.o: ../aes/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: AVR Compiler'
	avr-gcc -I"/home/joerg/Development/eclipse/rf24_motion/aes" -Wall -g2 -gstabs -Os -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -std=gnu99 -funsigned-char -funsigned-bitfields -mmcu=attiny84 -DF_CPU=8000000UL -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

aes/%.o: ../aes/%.S
	@echo 'Building file: $<'
	@echo 'Invoking: AVR Assembler'
	avr-gcc -x assembler-with-cpp -I"/home/joerg/Development/eclipse/rf24_motion/aes" -g2 -gstabs -mmcu=attiny84 -DF_CPU=8000000UL -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '



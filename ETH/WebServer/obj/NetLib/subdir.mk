################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
d:/ch32v307evt_official/EVT/EXAM/ETH/NetLib/eth_driver_10M.c 

C_DEPS += \
./NetLib/eth_driver_10M.d 

OBJS += \
./NetLib/eth_driver_10M.o 

DIR_OBJS += \
./NetLib/*.o \

DIR_DEPS += \
./NetLib/*.d \

DIR_EXPANDS += \
./NetLib/*.271r.expand \


# Each subdirectory must supply rules for building sources it contributes
NetLib/eth_driver_10M.o: d:/ch32v307evt_official/EVT/EXAM/ETH/NetLib/eth_driver_10M.c
	@	riscv32-wch-elf-gcc -march=rv32imac_xw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -gdwarf-4 -I"d:/ch32v307evt_official/EVT/EXAM/ETH/NetLib" -I"d:/ch32v307evt_official/EVT/EXAM/ETH/WebServer/HTTP" -I"d:/ch32v307evt_official/EVT/EXAM/SRC/Core" -I"d:/ch32v307evt_official/EVT/EXAM/SRC/Debug" -I"d:/ch32v307evt_official/EVT/EXAM/SRC/Peripheral/inc" -I"d:/ch32v307evt_official/EVT/EXAM/ETH/WebServer/User" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"


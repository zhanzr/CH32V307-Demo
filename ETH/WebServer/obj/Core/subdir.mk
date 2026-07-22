################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
d:/ch32v307evt_official/EVT/EXAM/SRC/Core/core_riscv.c 

C_DEPS += \
./Core/core_riscv.d 

OBJS += \
./Core/core_riscv.o 

DIR_OBJS += \
./Core/*.o \

DIR_DEPS += \
./Core/*.d \

DIR_EXPANDS += \
./Core/*.271r.expand \


# Each subdirectory must supply rules for building sources it contributes
Core/core_riscv.o: d:/ch32v307evt_official/EVT/EXAM/SRC/Core/core_riscv.c
	@	riscv32-wch-elf-gcc -march=rv32imac_xw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -gdwarf-4 -I"d:/ch32v307evt_official/EVT/EXAM/ETH/NetLib" -I"d:/ch32v307evt_official/EVT/EXAM/ETH/WebServer/HTTP" -I"d:/ch32v307evt_official/EVT/EXAM/SRC/Core" -I"d:/ch32v307evt_official/EVT/EXAM/SRC/Debug" -I"d:/ch32v307evt_official/EVT/EXAM/SRC/Peripheral/inc" -I"d:/ch32v307evt_official/EVT/EXAM/ETH/WebServer/User" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"


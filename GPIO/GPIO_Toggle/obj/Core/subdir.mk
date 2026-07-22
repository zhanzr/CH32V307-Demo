################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
d:/ch32v307_ms_prj/SRC/Core/core_riscv.c 

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
Core/core_riscv.o: d:/ch32v307_ms_prj/SRC/Core/core_riscv.c
	@	riscv32-wch-elf-gcc -march=rv32imafc_xw -mabi=ilp32f -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -fsingle-precision-constant -Wunused -Wuninitialized -g -gdwarf-4 -I"d:/ch32v307_ms_prj/SRC/Debug" -I"d:/ch32v307_ms_prj/SRC/Core" -I"d:/ch32v307_ms_prj/GPIO/GPIO_Toggle/User" -I"d:/ch32v307_ms_prj/SRC/Peripheral/inc" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

